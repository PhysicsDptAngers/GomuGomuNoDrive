/**
 * ==============================================================================
 * RUBBER ZENER TOPOLOGY - REAL-TIME DSP & ADAA BENCHMARK ENGINE (C++)
 * ==============================================================================
 * Authors: Dominique Guichaoua & Matthieu Loumaigne
 * Date: 2026
 * 
 * Fully compliant with rubber_zener_core Python API:
 *   - 4D decoupled (real-time) and 8D exact cross-discharge modes
 *   - Optional Backtracking Line Search for stiff convergence
 *   - First-order Antiderivative Antialiasing (ADAA)
 *   - Polyphase 41-tap Half-Band FIR Multirate Cascades (1x, 2x, 4x, 8x)
 * 
 * Compilation :
 *   g++ -O3 -std=c++17 benchmark_rubber_zener.cpp -o benchmark_rubber_zener
 * Exécution :
 *   ./benchmark_rubber_zener
 * ==============================================================================
 */

#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <string>

// =============================================================================
// 1. PHYSICAL CONSTANTS & CIRCUIT PARAMETERS
// =============================================================================
struct CircuitParams {
    double Vt    = 0.026;        // Thermal voltage at 25°C (26 mV)
    double Is    = 7.049e-15;    // BC550C BJT saturation current
    double Bf    = 493.2;        // Forward Beta
    double Br    = 2.886;        // Reverse Beta
    double IsD   = 21.0e-9;      // BAT54 Schottky saturation current
    double Rd    = 1.0;          // Schottky series resistance (Ohms)
    double Rin   = 10000.0;      // Shunt input resistor (10 kOhms)
    double Rk    = 10.0;         // Series knee resistor (10 Ohms)
    double Rp    = 1000.0;       // Collector saturation resistor (1 kOhm)
    double C     = 100.0e-9;     // Dynamic state capacitor (100 nF)
    double alpha = 0.5;          // Divider potentiometer wiper [0, 1]

    // Architectural modes (strictly matching rubber_zener_core)
    bool cap_pos_up = true;             // true: C || Rup (Mode 1), false: C || Rlow (Mode 2)
    bool exact_cross_discharge = false; // false: 4D decoupled (real-time) | true: 8D exact cross-coupled
    bool line_search = false;           // false: standard clamp | true: Backtracking Line Search
};

// =============================================================================
// 2. FIRST-ORDER ADAA TRANSCENDENTAL EVALUATION
// =============================================================================
inline void eval_exp(double v, double vp, double Vt, bool use_adaa, double& E, double& dEdv) {
    double x = std::clamp(v / Vt, -100.0, 80.0);
    if (!use_adaa) {
        E = std::exp(x);
        dEdv = E / Vt;
        return;
    }

    double xp = std::clamp(vp / Vt, -100.0, 80.0);
    double dx = x - xp;

    if (std::abs(dx) > 1.0e-4) {
        double ex = std::exp(x);
        double exp_p = std::exp(xp);
        E = (ex - exp_p) / dx;
        double dE_dx = (ex * dx - (ex - exp_p)) / (dx * dx);
        dEdv = dE_dx / Vt;
    } else {
        // 2nd-order Taylor expansion near singularity (dx -> 0)
        double exp_p = std::exp(xp);
        E = exp_p * (1.0 + dx * 0.5 + (dx * dx) / 6.0);
        double dE_dx = exp_p * (0.5 + dx / 3.0);
        dEdv = dE_dx / Vt;
    }
}

// =============================================================================
// 3. 4x4 GAUSSIAN ELIMINATION SOLVER WITH PARTIAL PIVOTING
// =============================================================================
inline bool solve4x4(double A[4][4], const double b[4], double x[4]) {
    double M[4][5];
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) M[i][j] = A[i][j];
        M[i][4] = b[i];
    }

    for (int i = 0; i < 4; ++i) {
        int max_r = i;
        double max_val = std::abs(M[i][i]);
        for (int k = i + 1; k < 4; ++k) {
            double v = std::abs(M[k][i]);
            if (v > max_val) {
                max_val = v;
                max_r = k;
            }
        }
        if (max_val < 1.0e-15) return false;
        if (max_r != i) {
            for (int j = i; j <= 4; ++j) std::swap(M[i][j], M[max_r][j]);
        }
        for (int k = i + 1; k < 4; ++k) {
            double factor = M[k][i] / M[i][i];
            for (int j = i; j <= 4; ++j) M[k][j] -= factor * M[i][j];
        }
    }

    for (int i = 3; i >= 0; --i) {
        double sum = M[i][4];
        for (int j = i + 1; j < 4; ++j) sum -= M[i][j] * x[j];
        x[i] = sum / M[i][i];
    }
    return true;
}

inline double norm4(const double v[4]) {
    return std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2] + v[3]*v[3]);
}

// =============================================================================
// 4. IMPLICIT NEWTON-RAPHSON SOLVER (ONE SAMPLE STEP)
// =============================================================================
inline void solve_ebers_moll(const double X_prev[4], double vin, double dt, 
                             const CircuitParams& p, bool use_adaa, double X_out[4]) {
    double Rup = (1.0 - p.alpha) * 50.0e3 + 1.0e3;
    double Rlow = p.alpha * 50.0e3 + 4.7e3;
    double Rs = p.Rin + p.Rk + p.Rd;

    for (int i = 0; i < 4; ++i) X_out[i] = X_prev[i];
    double final_step[4] = {0.0, 0.0, 0.0, 0.0};

    for (int iter = 0; iter < 50; ++iter) {
        double Vup = X_out[0];
        double Vbe = X_out[1];
        double Vbc = X_out[2];
        double Vd  = X_out[3];

        double evbe, devbe, evbc, devbc, evd, devd;
        eval_exp(Vbe, X_prev[1], p.Vt, use_adaa, evbe, devbe);
        eval_exp(Vbc, X_prev[2], p.Vt, use_adaa, evbc, devbc);
        eval_exp(Vd,  X_prev[3], p.Vt, use_adaa, evd,  devd);

        double ic = p.Is * (evbe - evbc) - (p.Is / p.Br) * (evbc - 1.0);
        double ib = (p.Is / p.Bf) * (evbe - 1.0) + (p.Is / p.Br) * (evbc - 1.0);
        double iD = p.IsD * (evd - 1.0);

        double dic_dvbe = p.Is * devbe;
        double dic_dvbc = -p.Is * devbc - (p.Is / p.Br) * devbc;
        double dib_dvbe = (p.Is / p.Bf) * devbe;
        double dib_dvbc = (p.Is / p.Br) * devbc;
        double diD_dvd  = p.IsD * devd;

        // F1: Collector KVL
        double f1 = Vbc - p.Rp * ic + Vup;
        double df1_dvup = 1.0;
        double df1_dvbe = -p.Rp * dic_dvbe;
        double df1_dvbc = 1.0 - p.Rp * dic_dvbc;
        double df1_dvd  = 0.0;

        // F2: Base node KCL
        double f2, df2_dvup, df2_dvbe, df2_dvbc, df2_dvd = 0.0;
        if (p.C > 0.0) {
            if (p.cap_pos_up) {
                f2 = p.C * (Vup - X_prev[0]) / dt - Vbe / Rlow - ib + Vup / Rup;
                df2_dvup = p.C / dt + 1.0 / Rup;
                df2_dvbe = -1.0 / Rlow - dib_dvbe;
                df2_dvbc = -dib_dvbc;
            } else {
                f2 = p.C * (Vbe - X_prev[1]) / dt + Vbe / Rlow + ib - Vup / Rup;
                df2_dvup = -1.0 / Rup;
                df2_dvbe = p.C / dt + 1.0 / Rlow + dib_dvbe;
                df2_dvbc = dib_dvbc;
            }
        } else {
            f2 = -Vbe / Rlow - ib + Vup / Rup;
            df2_dvup = 1.0 / Rup;
            df2_dvbe = -1.0 / Rlow - dib_dvbe;
            df2_dvbc = -dib_dvbc;
        }

        // F3: Total Current Conservation
        double f3, df3_dvup, df3_dvbe, df3_dvbc, df3_dvd = diD_dvd;
        if (p.cap_pos_up) {
            f3 = iD - ic - ib - Vbe / Rlow;
            df3_dvup = 0.0;
            df3_dvbe = -dic_dvbe - dib_dvbe - 1.0 / Rlow;
            df3_dvbc = -dic_dvbc - dib_dvbc;
        } else {
            f3 = iD - ic - Vup / Rup;
            df3_dvup = -1.0 / Rup;
            df3_dvbe = -dic_dvbe;
            df3_dvbc = -dic_dvbc;
        }

        // F4: Global Input KVL
        double f4 = vin - Rs * iD - Vd - Vup - Vbe;
        double df4_dvup = -1.0;
        double df4_dvbe = -1.0;
        double df4_dvbc = 0.0;
        double df4_dvd  = -Rs * diD_dvd - 1.0;

        double F[4] = { f1, f2, f3, f4 };
        double J[4][4] = {
            { df1_dvup, df1_dvbe, df1_dvbc, df1_dvd },
            { df2_dvup, df2_dvbe, df2_dvbc, df2_dvd },
            { df3_dvup, df3_dvbe, df3_dvbc, df3_dvd },
            { df4_dvup, df4_dvbe, df4_dvbc, df4_dvd }
        };

        double dX[4];
        if (!solve4x4(J, F, dX)) {
            for (int i = 0; i < 4; ++i) dX[i] = F[i] * 0.01;
        }

        if (p.line_search) {
            double norm_F = norm4(F);
            double alpha_ls = 1.0;
            for (int ls = 0; ls < 5; ++ls) {
                double step[4];
                for (int i = 0; i < 4; ++i) step[i] = std::clamp(alpha_ls * dX[i], -1.0, 1.0);
                double X_c[4];
                for (int i = 0; i < 4; ++i) X_c[i] = X_out[i] - step[i];
                X_c[1] = std::min(X_c[1], 0.9);
                X_c[2] = std::min(X_c[2], 0.9);
                X_c[3] = std::min(X_c[3], 0.9);

                double evbe_c, devbe_c, evbc_c, devbc_c, evd_c, devd_c;
                eval_exp(X_c[1], X_prev[1], p.Vt, use_adaa, evbe_c, devbe_c);
                eval_exp(X_c[2], X_prev[2], p.Vt, use_adaa, evbc_c, devbc_c);
                eval_exp(X_c[3], X_prev[3], p.Vt, use_adaa, evd_c, devd_c);

                double ic_c = p.Is * (evbe_c - evbc_c) - (p.Is / p.Br) * (evbc_c - 1.0);
                double ib_c = (p.Is / p.Bf) * (evbe_c - 1.0) + (p.Is / p.Br) * (evbc_c - 1.0);
                double iD_c = p.IsD * (evd_c - 1.0);

                double F_c[4];
                F_c[0] = X_c[2] - p.Rp * ic_c + X_c[0];
                if (p.C > 0.0) {
                    if (p.cap_pos_up) F_c[1] = p.C * (X_c[0] - X_prev[0]) / dt - X_c[1] / Rlow - ib_c + X_c[0] / Rup;
                    else              F_c[1] = p.C * (X_c[1] - X_prev[1]) / dt + X_c[1] / Rlow + ib_c - X_c[0] / Rup;
                } else {
                    F_c[1] = -X_c[1] / Rlow - ib_c + X_c[0] / Rup;
                }

                if (p.cap_pos_up) F_c[2] = iD_c - ic_c - ib_c - X_c[1] / Rlow;
                else              F_c[2] = iD_c - ic_c - X_c[0] / Rup;
                F_c[3] = vin - Rs * iD_c - X_c[3] - X_c[0] - X_c[1];

                if (norm4(F_c) < norm_F || alpha_ls < 0.05) {
                    for (int i = 0; i < 4; ++i) {
                        X_out[i] = X_c[i];
                        final_step[i] = step[i];
                    }
                    break;
                }
                alpha_ls *= 0.5;
            }
        } else {
            for (int i = 0; i < 4; ++i) {
                final_step[i] = std::clamp(dX[i], -1.0, 1.0);
                X_out[i] -= final_step[i];
            }
            X_out[1] = std::min(X_out[1], 0.9);
            X_out[2] = std::min(X_out[2], 0.9);
            X_out[3] = std::min(X_out[3], 0.9);
        }

        double max_err = 0.0;
        for (int i = 0; i < 4; ++i) max_err = std::max(max_err, std::abs(final_step[i]));
        if (max_err < 1.0e-6) break;
    }
}

// =============================================================================
// 5. PRODUCTION-GRADE 41-TAP HALF-BAND POLYPHASE FIR FILTER
// =============================================================================
class HalfBandFilter {
private:
    static constexpr int NUM_COEFFS = 10; // 10 non-zero odd polyphase coefficients
    double c[NUM_COEFFS];                 // Filter taps
    double hist_up[20];                   // Shift buffer for upsampling (1x rate)
    double hist_dn[41];                   // Shift buffer for downsampling (2x rate)

public:
    HalfBandFilter() {
        reset();
        const double pi = 3.14159265358979323846;
        double raw[NUM_COEFFS];
        double sum = 0.0;

        for (int i = 0; i < NUM_COEFFS; ++i) {
            int k = 2 * i + 1; // 1, 3, 5, ..., 19
            int n = 20 - k;
            double theta = 2.0 * pi * n / 40.0;
            double w = 0.35875 - 0.48829 * std::cos(theta) 
                               + 0.14128 * std::cos(2.0 * theta) 
                               - 0.01168 * std::cos(3.0 * theta);
            double sinc = std::sin(0.5 * pi * k) / (pi * k);
            raw[i] = sinc * w;
            sum += raw[i];
        }

        double norm = 0.25 / sum;
        for (int i = 0; i < NUM_COEFFS; ++i) {
            c[i] = raw[i] * norm;
        }
    }

    void reset() {
        for (int i = 0; i < 20; ++i) hist_up[i] = 0.0;
        for (int i = 0; i < 41; ++i) hist_dn[i] = 0.0;
    }

    inline void upsample(double in, double& out0, double& out1) {
        for (int j = 19; j > 0; --j) hist_up[j] = hist_up[j - 1];
        hist_up[0] = in;
        out0 = hist_up[10]; // Center tap delay

        double acc = 0.0;
        for (int i = 0; i < NUM_COEFFS; ++i) {
            acc += c[i] * (hist_up[9 - i] + hist_up[10 + i]);
        }
        out1 = 2.0 * acc;
    }

    inline double downsample(double in0, double in1) {
        for (int j = 40; j >= 2; --j) hist_dn[j] = hist_dn[j - 2];
        hist_dn[1] = in0;
        hist_dn[0] = in1;

        double center = 0.5 * hist_dn[21];
        double acc = 0.0;
        for (int i = 0; i < NUM_COEFFS; ++i) {
            int k1 = 21 - (2 * i + 1);
            int k2 = 21 + (2 * i + 1);
            acc += c[i] * (hist_dn[k1] + hist_dn[k2]);
        }
        return center + acc;
    }
};

// =============================================================================
// 6. PROCESSING ENGINES FOR 1x, 2x, 4x, 8x
// =============================================================================
class RubberZenerProcessor {
private:
    CircuitParams params;
    bool use_adaa;
    int ovs_factor;

    double X_up[4] = {0,0,0,0};
    double X_dn[4] = {0,0,0,0};

    HalfBandFilter hb1, hb2, hb3;

public:
    RubberZenerProcessor(CircuitParams p, bool adaa, int ovs) 
        : params(p), use_adaa(adaa), ovs_factor(ovs) {}

    void reset() {
        for (int i = 0; i < 4; ++i) X_up[i] = X_dn[i] = 0.0;
        hb1.reset(); hb2.reset(); hb3.reset();
    }

    inline double process_core(double vin, double dt) {
        double Rup = (1.0 - params.alpha) * 50.0e3 + 1.0e3;
        double Rlow = params.alpha * 50.0e3 + 4.7e3;
        double vout = 0.0;

        if (vin >= 0.0) {
            double X_new[4];
            solve_ebers_moll(X_up, vin, dt, params, use_adaa, X_new);

            double evd, devd;
            eval_exp(X_new[3], X_up[3], params.Vt, use_adaa, evd, devd);
            double iD = params.IsD * (evd - 1.0);
            vout = vin - params.Rin * iD;

            for (int i = 0; i < 4; ++i) X_up[i] = X_new[i];

            if (params.exact_cross_discharge) {
                double X_dn_new[4];
                solve_ebers_moll(X_dn, -vout, dt, params, use_adaa, X_dn_new);
                for (int i = 0; i < 4; ++i) X_dn[i] = X_dn_new[i];
            } else {
                if (params.C > 0.0) {
                    double R_bleed = params.cap_pos_up ? Rup : Rlow;
                    int state_idx = params.cap_pos_up ? 0 : 1;
                    X_dn[state_idx] *= std::exp(-dt / (R_bleed * params.C));
                }
                X_dn[2] = X_dn[3] = 0.0;
                if (params.cap_pos_up) X_dn[1] = 0.0; else X_dn[0] = 0.0;
            }
        } else {
            double X_new[4];
            solve_ebers_moll(X_dn, -vin, dt, params, use_adaa, X_new);

            double evd, devd;
            eval_exp(X_new[3], X_dn[3], params.Vt, use_adaa, evd, devd);
            double iD = params.IsD * (evd - 1.0);
            vout = vin + params.Rin * iD;

            for (int i = 0; i < 4; ++i) X_dn[i] = X_new[i];

            if (params.exact_cross_discharge) {
                double X_up_new[4];
                solve_ebers_moll(X_up, vout, dt, params, use_adaa, X_up_new);
                for (int i = 0; i < 4; ++i) X_up[i] = X_up_new[i];
            } else {
                if (params.C > 0.0) {
                    double R_bleed = params.cap_pos_up ? Rup : Rlow;
                    int state_idx = params.cap_pos_up ? 0 : 1;
                    X_up[state_idx] *= std::exp(-dt / (R_bleed * params.C));
                }
                X_up[2] = X_up[3] = 0.0;
                if (params.cap_pos_up) X_up[1] = 0.0; else X_up[0] = 0.0;
            }
        }
        return vout;
    }

    void process_block(const std::vector<double>& in, std::vector<double>& out, double Fs) {
        int N = (int)in.size();
        out.resize(N);
        double dt = 1.0 / (Fs * ovs_factor);

        if (ovs_factor == 1) {
            for (int n = 0; n < N; ++n) {
                out[n] = process_core(in[n], dt);
            }
        } else if (ovs_factor == 2) {
            for (int n = 0; n < N; ++n) {
                double s0, s1;
                hb1.upsample(in[n], s0, s1);
                double y0 = process_core(s0, dt);
                double y1 = process_core(s1, dt);
                out[n] = hb1.downsample(y0, y1);
            }
        } else if (ovs_factor == 4) {
            for (int n = 0; n < N; ++n) {
                double s0, s1;
                hb1.upsample(in[n], s0, s1);

                double s00, s01, s10, s11;
                hb2.upsample(s0, s00, s01);
                hb2.upsample(s1, s10, s11);

                double y00 = process_core(s00, dt);
                double y01 = process_core(s01, dt);
                double y10 = process_core(s10, dt);
                double y11 = process_core(s11, dt);

                double d0 = hb2.downsample(y00, y01);
                double d1 = hb2.downsample(y10, y11);
                out[n] = hb1.downsample(d0, d1);
            }
        } else if (ovs_factor == 8) {
            for (int n = 0; n < N; ++n) {
                double a0, a1;
                hb1.upsample(in[n], a0, a1);

                double b0, b1, b2, b3;
                hb2.upsample(a0, b0, b1);
                hb2.upsample(a1, b2, b3);

                double c[8];
                hb3.upsample(b0, c[0], c[1]);
                hb3.upsample(b1, c[2], c[3]);
                hb3.upsample(b2, c[4], c[5]);
                hb3.upsample(b3, c[6], c[7]);

                double y[8];
                for (int k = 0; k < 8; ++k) y[k] = process_core(c[k], dt);

                double d[4];
                d[0] = hb3.downsample(y[0], y[1]);
                d[1] = hb3.downsample(y[2], y[3]);
                d[2] = hb3.downsample(y[4], y[5]);
                d[3] = hb3.downsample(y[6], y[7]);

                double e0 = hb2.downsample(d[0], d[1]);
                double e1 = hb2.downsample(d[2], d[3]);
                out[n] = hb1.downsample(e0, e1);
            }
        }
    }
};

// =============================================================================
// 7. MAIN BENCHMARK DRIVER
// =============================================================================
int main() {
    std::cout << "==================================================================\n";
    std::cout << "  RUBBER ZENER TOPOLOGY: REAL-TIME DSP & ADAA BENCHMARK (C++)\n";
    std::cout << "==================================================================\n";

    const double Fs = 48000.0;
    const double f0 = 2500.0;
    const double duration = 0.20; // 200 ms buffer
    const int N = (int)(duration * Fs);

    std::vector<double> t(N), Vin(N);
    const double pi = 3.14159265358979323846;
    for (int n = 0; n < N; ++n) {
        t[n] = n / Fs;
        Vin[n] = 10.0 * std::sin(2.0 * pi * f0 * t[n]); // 10 Vpeak hard overdrive
    }

    CircuitParams params;
    // Default architecture: 4D decoupled (real-time audio), Mode 1 (C || Rup)
    params.cap_pos_up = true;
    params.exact_cross_discharge = false;
    params.line_search = false;

    struct Config {
        std::string name;
        int ovs;
        bool adaa;
    };

    std::vector<Config> configs = {
        { "1x_No_ADAA", 1, false },
        { "1x_ADAA",    1, true  },
        { "2x_No_ADAA", 2, false },
        { "2x_ADAA",    2, true  },
        { "4x_No_ADAA", 4, false },
        { "8x_No_ADAA", 8, false }
    };

    std::vector<std::vector<double>> results(configs.size());
    std::vector<double> timings_ms(configs.size());

    const int REPEATS = 5;

    std::ofstream f_metrics("benchmark_metrics.csv");
    f_metrics << "Config,OVS,ADAA,Time_ms\n";

    for (size_t i = 0; i < configs.size(); ++i) {
        RubberZenerProcessor proc(params, configs[i].adaa, configs[i].ovs);

        // Warm-up run
        proc.process_block(Vin, results[i], Fs);

        auto t0 = std::chrono::high_resolution_clock::now();
        for (int r = 0; r < REPEATS; ++r) {
            proc.reset();
            proc.process_block(Vin, results[i], Fs);
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count() / REPEATS;
        timings_ms[i] = elapsed_ms;

        std::cout << std::left << std::setw(15) << configs[i].name 
                  << " | OVS: " << configs[i].ovs 
                  << " | ADAA: " << (configs[i].adaa ? "YES" : "NO ")
                  << " | Time: " << std::fixed << std::setprecision(2) << elapsed_ms << " ms\n";

        f_metrics << configs[i].name << "," << configs[i].ovs << "," 
                  << (configs[i].adaa ? 1 : 0) << "," << elapsed_ms << "\n";
    }
    f_metrics.close();

    std::cout << "\nExporting waveforms to waveforms.csv...\n";
    std::ofstream f_wave("waveforms.csv");
    f_wave << "time,vin,vout_1x_noadaa,vout_1x_adaa,vout_2x_noadaa,vout_2x_adaa,vout_4x_noadaa,vout_8x_noadaa\n";
    f_wave << std::setprecision(7);

    for (int n = 0; n < N; ++n) {
        f_wave << t[n] << "," << Vin[n];
        for (size_t i = 0; i < configs.size(); ++i) {
            f_wave << "," << results[i][n];
        }
        f_wave << "\n";
    }
    f_wave.close();

    std::cout << "[OK] Benchmarks complete. Launch analyze_benchmark.py.\n";
    return 0;
}
