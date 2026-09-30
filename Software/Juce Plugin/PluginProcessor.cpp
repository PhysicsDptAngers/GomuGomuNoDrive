#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// PEDAL A: RUBBER ZENER SOFT-CLIPPING SOLVER (2D NEWTON-RAPHSON)
// ==============================================================================

double RubberZenerSoft::solve_step (double X[2], double vin_abs, double dt, double C, int pos,
                                   double R_up, double R_low, double RF)
{
    const double X_prev[2] = { X[0], X[1] };
    const double v_target = (RF / R1) * vin_abs;
    const double c_dt = (C > 1e-12) ? (C / dt) : 0.0;

    for (int iter = 0; iter < 30; ++iter)
    {
        const double x0 = std::max (0.0, X[0]);
        const double x1 = juce::jlimit (-0.5, 0.88, X[1]);

        const double exp_be = std::exp (juce::jlimit (-50.0, 35.0, x1 / V_T));
        const double ic = I_S * (exp_be - 1.0);
        const double dic_dx1 = (I_S / V_T) * exp_be;

        const double ib = (I_S / B_F) * (exp_be - 1.0);
        const double dib_dx1 = dic_dx1 / B_F;

        double i_up = 0.0, diup_dx0 = 0.0;
        double f1 = 0.0, j10 = 0.0, j11 = 0.0;

        if (pos == 0) { // Mode 1 (ATK) : C // R_up
            i_up = x0 / R_up + c_dt * (x0 - X_prev[0]);
            diup_dx0 = 1.0 / R_up + c_dt;
            f1 = i_up - ib - x1 / R_low;
            j10 = diup_dx0;
            j11 = -dib_dx1 - 1.0 / R_low;
        } else {        // Mode 2 (BODY) : C // R_low
            i_up = x0 / R_up;
            diup_dx0 = 1.0 / R_up;
            f1 = i_up - ib - x1 / R_low - c_dt * (x1 - X_prev[1]);
            j10 = diup_dx0;
            j11 = -dib_dx1 - 1.0 / R_low - c_dt;
        }

        const double i_d = ic + i_up;
        const double did_dx0 = diup_dx0;
        const double did_dx1 = dic_dx1;

        double vd = 0.0, dvd_did = 0.0;
        if (i_d >= 0.0) {
            vd = V_T * std::log (1.0 + i_d / I_SD);
            dvd_did = V_T / (i_d + I_SD);
        } else {
            vd = (V_T / I_SD) * i_d;
            dvd_did = V_T / I_SD;
        }

        const double f0 = vd + x0 + x1 + RF * i_d - v_target;
        const double d_loop_did = dvd_did + RF;
        const double j00 = d_loop_did * did_dx0 + 1.0;
        const double j01 = d_loop_did * did_dx1 + 1.0;

        const double det = j00 * j11 - j01 * j10;
        if (std::abs (det) < 1e-14) break;

        double dx0 = (f0 * j11 - f1 * j01) / det;
        double dx1 = (j00 * f1 - j10 * f0) / det;

        dx0 = juce::jlimit (-0.5, 0.5, dx0);
        dx1 = juce::jlimit (-0.1, 0.1, dx1);

        X[0] -= dx0;
        X[1] -= dx1;
        X[0] = std::max (0.0, X[0]);
        X[1] = std::min (0.88, X[1]);

        if (std::max (std::abs (dx0), std::abs (dx1)) < 1e-6) break;
    }

    const double x0 = std::max (0.0, X[0]);
    const double x1 = juce::jlimit (-0.5, 0.88, X[1]);
    const double exp_be = std::exp (juce::jlimit (-50.0, 35.0, x1 / V_T));
    const double ic = I_S * (exp_be - 1.0);
    const double i_up = (pos == 0) ? (x0 / R_up + c_dt * (x0 - X_prev[0])) : (x0 / R_up);
    const double i_d = ic + i_up;
    const double vd = (i_d >= 0.0) ? (V_T * std::log (1.0 + i_d / I_SD)) : ((V_T / I_SD) * i_d);

    return vd + x0 + x1;
}

double RubberZenerSoft::processSample (double vin, double dt,
                                      double alpha_up, double C_up, int pos_up,
                                      double alpha_dn, double C_dn, int pos_dn,
                                      double RF)
{
    const double R_up_U  = (1.0 - alpha_up) * 50000.0 + 1000.0;
    const double R_low_U = alpha_up * 50000.0 + 4700.0;
    const double R_up_D  = (1.0 - alpha_dn) * 50000.0 + 1000.0;
    const double R_low_D = alpha_dn * 50000.0 + 4700.0;

    double v_out = vin;

    if (vin >= 0.0) {
        const double v_dip = solve_step (X_up, vin, dt, C_up, pos_up, R_up_U, R_low_U, RF);
        v_out = vin + v_dip;

        // Branche DOWN inactive : seul le nœud réactif conserve sa mémoire dynamique
        if (C_dn > 1e-12) {
            const double R_bleed = (pos_dn == 0) ? R_up_D : R_low_D;
            if (pos_dn == 0) {
                X_dn[0] *= std::exp (-dt / (R_bleed * C_dn));
                X_dn[1] = 0.0;
            } else {
                X_dn[0] = 0.0;
                X_dn[1] *= std::exp (-dt / (R_bleed * C_dn));
            }
        } else {
            X_dn[0] = X_dn[1] = 0.0;
        }
    } else {
        const double v_dip = solve_step (X_dn, -vin, dt, C_dn, pos_dn, R_up_D, R_low_D, RF);
        v_out = vin - v_dip;

        // Branche UP inactive : seul le nœud réactif conserve sa mémoire dynamique
        if (C_up > 1e-12) {
            const double R_bleed = (pos_up == 0) ? R_up_U : R_low_U;
            if (pos_up == 0) {
                X_up[0] *= std::exp (-dt / (R_bleed * C_up));
                X_up[1] = 0.0;
            } else {
                X_up[0] = 0.0;
                X_up[1] *= std::exp (-dt / (R_bleed * C_up));
            }
        } else {
            X_up[0] = X_up[1] = 0.0;
        }
    }

    return v_out;
}

// ==============================================================================
// PEDAL B: RUBBER ZENER HARD-CLIPPING SOLVER (4D NEWTON-RAPHSON)
// ==============================================================================

bool RubberZenerHard::solveLinearSystem4x4 (const double J[4][4], const double F[4], double dX[4]) {
    double A[4][5];
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) A[i][j] = J[i][j];
        A[i][4] = F[i];
    }
    for (int i = 0; i < 4; ++i) {
        int pivot = i;
        for (int j = i + 1; j < 4; ++j) {
            if (std::abs (A[j][i]) > std::abs (A[pivot][i])) pivot = j;
        }
        if (std::abs (A[pivot][i]) < 1e-12) return false;
        if (pivot != i) {
            for (int k = 0; k < 5; ++k) std::swap (A[i][k], A[pivot][k]);
        }
        for (int j = i + 1; j < 4; ++j) {
            double factor = A[j][i] / A[i][i];
            for (int k = i; k < 5; ++k) A[j][k] -= factor * A[i][k];
        }
    }
    for (int i = 3; i >= 0; --i) {
        dX[i] = A[i][4];
        for (int j = i + 1; j < 4; ++j) dX[i] -= A[i][j] * dX[j];
        dX[i] /= A[i][i];
    }
    return true;
}

void RubberZenerHard::solve_ebers_moll_step (double X[4], double vin, double dt, double C, 
                                            double R_up, double R_low, double R_p, double R_s, int cap_pos) 
{
    const double X_prev[4] = { X[0], X[1], X[2], X[3] };
    
    for (int iter = 0; iter < 40; ++iter) {
        const double Vup = X[0], Vbe = X[1], Vbc = X[2], Vd = X[3];
        
        const double x_be = juce::jlimit (-100.0, 80.0, Vbe / V_T);
        const double x_bc = juce::jlimit (-100.0, 80.0, Vbc / V_T);
        const double x_d  = juce::jlimit (-100.0, 80.0, Vd  / V_T);

        const double exp_be = std::exp (x_be);
        const double exp_bc = std::exp (x_bc);
        const double exp_d  = std::exp (x_d);

        const double dexp_dvbe = exp_be / V_T;
        const double dexp_dvbc = exp_bc / V_T;
        const double dexp_dvd  = exp_d  / V_T;
        
        const double ic = I_S * (exp_be - exp_bc) - (I_S / B_R) * (exp_bc - 1.0);
        const double ib = (I_S / B_F) * (exp_be - 1.0) + (I_S / B_R) * (exp_bc - 1.0);
        const double iD = I_SD * (exp_d - 1.0);
        
        const double dic_dvbe = I_S * dexp_dvbe;
        const double dic_dvbc = -I_S * dexp_dvbc - (I_S / B_R) * dexp_dvbc;
        const double dib_dvbe = (I_S / B_F) * dexp_dvbe;
        const double dib_dvbc = (I_S / B_R) * dexp_dvbc;
        const double diD_dvd  = I_SD * dexp_dvd;
        
        const double f1 = Vbc - R_p * ic + Vup;
        const double df1_dvup = 1.0;
        const double df1_dvbe = -R_p * dic_dvbe;
        const double df1_dvbc = 1.0 - R_p * dic_dvbc;
        const double df1_dvd  = 0.0;
        
        double f2 = 0.0, df2_dvup = 0.0, df2_dvbe = 0.0, df2_dvbc = 0.0;
        if (C > 1e-12) {
            if (cap_pos == 0) {
                f2 = C * (Vup - X_prev[0]) / dt - Vbe / R_low - ib + Vup / R_up;
                df2_dvup = C / dt + 1.0 / R_up;
                df2_dvbe = -1.0 / R_low - dib_dvbe;
                df2_dvbc = -dib_dvbc;
            } else {
                f2 = C * (Vbe - X_prev[1]) / dt + Vbe / R_low + ib - Vup / R_up;
                df2_dvup = -1.0 / R_up;
                df2_dvbe = C / dt + 1.0 / R_low + dib_dvbe;
                df2_dvbc = dib_dvbc;
            }
        } else {
            f2 = -Vbe / R_low - ib + Vup / R_up;
            df2_dvup = 1.0 / R_up;
            df2_dvbe = -1.0 / R_low - dib_dvbe;
            df2_dvbc = -dib_dvbc;
        }

        double f3 = 0.0, df3_dvup = 0.0, df3_dvbe = 0.0, df3_dvbc = 0.0;
        if (cap_pos == 0) {
            f3 = iD - ic - ib - Vbe / R_low;
            df3_dvbe = -dic_dvbe - dib_dvbe - 1.0 / R_low;
            df3_dvbc = -dic_dvbc - dib_dvbc;
        } else {
            f3 = iD - ic - Vup / R_up;
            df3_dvup = -1.0 / R_up;
            df3_dvbe = -dic_dvbe;
            df3_dvbc = -dic_dvbc;
        }
        
        const double f4 = vin - R_s * iD - Vd - Vup - Vbe;
        
        const double F[4] = { f1, f2, f3, f4 };
        const double J[4][4] = {
            { df1_dvup, df1_dvbe, df1_dvbc, df1_dvd },
            { df2_dvup, df2_dvbe, df2_dvbc, 0.0 },
            { df3_dvup, df3_dvbe, df3_dvbc, diD_dvd },
            { -1.0,     -1.0,     0.0,      -R_s * diD_dvd - 1.0 }
        };
        
        double dX[4] = { 0.0, 0.0, 0.0, 0.0 };
        if (!solveLinearSystem4x4 (J, F, dX)) {
            for (int k = 0; k < 4; ++k) dX[k] = F[k] * 0.01; 
        }
        
        for (int k = 0; k < 4; ++k) {
            X[k] -= juce::jlimit (-1.0, 1.0, dX[k]);
        }
        
        X[1] = std::min (X[1], 0.9);
        X[2] = std::min (X[2], 0.9);
        X[3] = std::min (X[3], 0.9);
        
        double max_err = 0.0;
        for (int k = 0; k < 4; ++k) {
            if (std::abs (dX[k]) > max_err) max_err = std::abs (dX[k]);
        }
        if (max_err < 1e-6) break;
    }
}

double RubberZenerHard::processSample (double vin, double R_in, double dt, 
                                      double alpha_up, double R_k_up, double R_p_up, double C_up, int pos_up,
                                      double alpha_dn, double R_k_dn, double R_p_dn, double C_dn, int pos_dn) 
{
    const double R_up_U  = (1.0 - alpha_up) * 50000.0 + 1000.0;
    const double R_low_U = alpha_up * 50000.0 + 4700.0;
    const double R_s_U   = R_in + R_k_up + R_D;

    const double R_up_D  = (1.0 - alpha_dn) * 50000.0 + 1000.0;
    const double R_low_D = alpha_dn * 50000.0 + 4700.0;
    const double R_s_D   = R_in + R_k_dn + R_D;

    double v_out = vin;

    if (vin >= 0.0) {
        solve_ebers_moll_step (X_up, vin, dt, C_up, R_up_U, R_low_U, R_p_up, R_s_U, pos_up);
        const double x_d = juce::jlimit (-100.0, 80.0, X_up[3] / V_T);
        const double iD  = I_SD * (std::exp (x_d) - 1.0);
        v_out = vin - R_in * iD;

        if (C_dn > 1e-12) {
            const double R_bleed = (pos_dn == 0) ? R_up_D : R_low_D;
            const int idx = (pos_dn == 0) ? 0 : 1;
            X_dn[idx] *= std::exp (-dt / (R_bleed * C_dn));
        } else {
            X_dn[0] = X_dn[1] = 0.0;
        }
        X_dn[2] = X_dn[3] = 0.0;
    } else {
        solve_ebers_moll_step (X_dn, -vin, dt, C_dn, R_up_D, R_low_D, R_p_dn, R_s_D, pos_dn);
        const double x_d = juce::jlimit (-100.0, 80.0, X_dn[3] / V_T);
        const double iD  = I_SD * (std::exp (x_d) - 1.0);
        v_out = vin + R_in * iD;

        if (C_up > 1e-12) {
            const double R_bleed = (pos_up == 0) ? R_up_U : R_low_U;
            const int idx = (pos_up == 0) ? 0 : 1;
            X_up[idx] *= std::exp (-dt / (R_bleed * C_up));
        } else {
            X_up[0] = X_up[1] = 0.0;
        }
        X_up[2] = X_up[3] = 0.0;
    }

    return v_out;
}

// ==============================================================================
// JUCE AUDIO PROCESSOR MAIN IMPLEMENTATION
// ==============================================================================

GomuGomuNoDrive::GomuGomuNoDrive()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameters()),
      cabConvolution (juce::dsp::Convolution::NonUniform { 128 })
{
    routingParam   = apvts.getRawParameterValue ("ROUTING");
    interCutParam  = apvts.getRawParameterValue ("INTER_CUT");
    cabEnableParam = apvts.getRawParameterValue ("CAB_ENABLE");
    cabSelectParam = apvts.getRawParameterValue ("CAB_SELECT");

    aEnableParam   = apvts.getRawParameterValue ("A_ENABLE");
    aDriveParam    = apvts.getRawParameterValue ("A_DRIVE");
    aLevelParam    = apvts.getRawParameterValue ("A_LEVEL");
    aToneParam     = apvts.getRawParameterValue ("A_TONE");
    aVoicingParam  = apvts.getRawParameterValue ("A_VOICING");
    aThreshPParam  = apvts.getRawParameterValue ("A_THRESH_P");
    aThreshMParam  = apvts.getRawParameterValue ("A_THRESH_M");
    aCapPParam     = apvts.getRawParameterValue ("A_CAP_P");
    aCapMParam     = apvts.getRawParameterValue ("A_CAP_M");
    aCapPosPParam  = apvts.getRawParameterValue ("A_CAP_POS_P");
    aCapPosMParam  = apvts.getRawParameterValue ("A_CAP_POS_M");

    bEnableParam   = apvts.getRawParameterValue ("B_ENABLE");
    bDriveParam    = apvts.getRawParameterValue ("B_DRIVE");
    bLevelParam    = apvts.getRawParameterValue ("B_LEVEL");
    bToneParam     = apvts.getRawParameterValue ("B_TONE");
    bVoicingParam  = apvts.getRawParameterValue ("B_VOICING");
    bThreshPParam  = apvts.getRawParameterValue ("B_THRESH_P");
    bThreshMParam  = apvts.getRawParameterValue ("B_THRESH_M");
    bCapPParam     = apvts.getRawParameterValue ("B_CAP_P");
    bCapMParam     = apvts.getRawParameterValue ("B_CAP_M");
    bCapPosPParam  = apvts.getRawParameterValue ("B_CAP_POS_P");
    bCapPosMParam  = apvts.getRawParameterValue ("B_CAP_POS_M");
    bKneePParam    = apvts.getRawParameterValue ("B_KNEE_P");
    bKneeMParam    = apvts.getRawParameterValue ("B_KNEE_M");
    bPinchPParam   = apvts.getRawParameterValue ("B_PINCH_P");
    bPinchMParam   = apvts.getRawParameterValue ("B_PINCH_M");

    for (size_t i = 0; i < numEqBands; ++i) {
        preEqParams[i]  = apvts.getRawParameterValue ("PRE_EQ_" + juce::String (i));
        postEqParams[i] = apvts.getRawParameterValue ("POST_EQ_" + juce::String (i));
        lastPreGains[i] = lastPostGains[i] = 0.0f;
    }
}

GomuGomuNoDrive::~GomuGomuNoDrive() {}

juce::AudioProcessorValueTreeState::ParameterLayout GomuGomuNoDrive::createParameters() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    juce::StringArray routingChoices = { "A -> B", "B -> A", "A // B" };
    params.push_back (std::make_unique<juce::AudioParameterChoice>("ROUTING", "Chain Routing", routingChoices, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("INTER_CUT", "Inter-Stage Tightener", 0.0f, 1.0f, 0.35f));

    juce::StringArray voicingChoices = { "TS-VOICE", "RAT-VOICE", "MUFF-VOICE", "LAB / CUSTOM" };
    juce::StringArray capPosChoices = { "Parallel R_up", "Parallel R_low" };

    // --- PEDAL A (SOFT CLIPPING) ---
    params.push_back (std::make_unique<juce::AudioParameterBool>("A_ENABLE", "Pedal A Active", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("A_DRIVE", "Drive A", 0.0f, 1.0f, 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("A_LEVEL", "Level A (dB)", -24.0f, 12.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("A_TONE", "Tone A", 0.0f, 1.0f, 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterChoice>("A_VOICING", "Voicing A", voicingChoices, 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat>("A_THRESH_P", "Alpha Pot + A", 0.0f, 1.0f, 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("A_CAP_P", "Cap + A (nF)", 0.0f, 1000.0f, 100.0f));
    params.push_back (std::make_unique<juce::AudioParameterChoice>("A_CAP_POS_P", "Cap Pos + A", capPosChoices, 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat>("A_THRESH_M", "Alpha Pot - A", 0.0f, 1.0f, 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("A_CAP_M", "Cap - A (nF)", 0.0f, 1000.0f, 100.0f));
    params.push_back (std::make_unique<juce::AudioParameterChoice>("A_CAP_POS_M", "Cap Pos - A", capPosChoices, 0));

    // --- PEDAL B (HARD CLIPPING) ---
    params.push_back (std::make_unique<juce::AudioParameterBool>("B_ENABLE", "Pedal B Active", true));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("B_DRIVE", "Drive B", 0.0f, 1.0f, 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("B_LEVEL", "Level B (dB)", -24.0f, 12.0f, 0.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("B_TONE", "Tone B", 0.0f, 1.0f, 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterChoice>("B_VOICING", "Voicing B", voicingChoices, 1));

    params.push_back (std::make_unique<juce::AudioParameterFloat>("B_THRESH_P", "Alpha Pot + B", 0.0f, 1.0f, 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("B_CAP_P", "Cap + B (nF)", 0.0f, 1000.0f, 100.0f));
    params.push_back (std::make_unique<juce::AudioParameterChoice>("B_CAP_POS_P", "Cap Pos + B", capPosChoices, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("B_KNEE_P", "R_k + B (Ohms)", 0.0f, 10000.0f, 10.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("B_PINCH_P", "Pinch + B (Ohms)", 1.0f, 5000.0f, 1000.0f));

    params.push_back (std::make_unique<juce::AudioParameterFloat>("B_THRESH_M", "Alpha Pot - B", 0.0f, 1.0f, 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("B_CAP_M", "Cap - B (nF)", 0.0f, 1000.0f, 100.0f));
    params.push_back (std::make_unique<juce::AudioParameterChoice>("B_CAP_POS_M", "Cap Pos - B", capPosChoices, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("B_KNEE_M", "R_k - B (Ohms)", 0.0f, 10000.0f, 10.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("B_PINCH_M", "Pinch - B (Ohms)", 1.0f, 5000.0f, 1000.0f));

    for (size_t i = 0; i < numEqBands; ++i) {
        params.push_back (std::make_unique<juce::AudioParameterFloat>("PRE_EQ_" + juce::String (i), "Pre EQ " + juce::String ((int)eqFrequencies[i]), -15.0f, 15.0f, 0.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat>("POST_EQ_" + juce::String (i), "Post EQ " + juce::String ((int)eqFrequencies[i]), -15.0f, 15.0f, 0.0f));
    }

    params.push_back (std::make_unique<juce::AudioParameterBool>("CAB_ENABLE", "Cabinet IR Enable", false));

    juce::StringArray cabChoices;
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i) {
        juce::String name = juce::String::fromUTF8 (BinaryData::originalFilenames[i]);
        if (name.endsWithIgnoreCase (".wav")) name = name.dropLastCharacters (4);
        cabChoices.add (name);
    }
    if (cabChoices.isEmpty()) cabChoices.add ("Default Cab");
    params.push_back (std::make_unique<juce::AudioParameterChoice>("CAB_SELECT", "Cabinet Model", cabChoices, 0));

    return { params.begin(), params.end() };
}

void GomuGomuNoDrive::loadBundledIR (int index)
{
    if (index < 0 || index >= BinaryData::namedResourceListSize) return;
    int dataSize = 0;
    const char* data = BinaryData::getNamedResource (BinaryData::namedResourceList[index], dataSize);
    if (data != nullptr && dataSize > 0) {
        cabConvolution.loadImpulseResponse (data, static_cast<size_t>(dataSize),
            juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::yes, 0, juce::dsp::Convolution::Normalise::yes);
        hasValidIR.store (true);
        currentLoadedIRIndex.store (index);
    }
}

void GomuGomuNoDrive::updateFilters()
{
    double sr = getSampleRate();
    if (sr <= 0.0) return;

    bool srChanged = (std::abs (sr - lastSampleRate) > 1e-3);
    lastSampleRate = sr;

    float curInterCut = interCutParam != nullptr ? interCutParam->load() : 0.35f;
    if (srChanged || std::abs (curInterCut - lastInterCut) > 0.005f) {
        lastInterCut = curInterCut;
        float cutFreq = 40.0f * std::pow (20.0f, curInterCut);
        auto interCoeffs = juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sr, cutFreq);
        interCutFilters[0].coefficients = interCoeffs;
        interCutFilters[1].coefficients = interCoeffs;
    }

    int vA = aVoicingParam != nullptr ? (int)aVoicingParam->load() : 0;
    float tA = aToneParam != nullptr ? aToneParam->load() : 0.5f;
    float dA = aDriveParam != nullptr ? aDriveParam->load() : 0.5f;

    if (srChanged || vA != lastVoicingA || std::abs (tA - lastToneA) > 0.005f || std::abs (dA - lastDriveA) > 0.01f) {
        lastVoicingA = vA; lastToneA = tA; lastDriveA = dA;
        if (vA == Voicing_TS) {
            auto p1 = juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sr, 160.0f);
            auto p2 = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 850.0f, 1.0f, juce::Decibels::decibelsToGain (5.0f));
            auto po1 = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, 5800.0f, 0.707f);
            auto po2 = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, 120.0f, 0.707f, juce::Decibels::decibelsToGain (2.5f));
            auto tC = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 2500.0f, 0.707f, juce::Decibels::decibelsToGain (-8.0f + tA * 16.0f));
            for (size_t ch = 0; ch < 2; ++ch) {
                preA1[ch].coefficients = p1; preA2[ch].coefficients = p2;
                postA1[ch].coefficients = po1; postA2[ch].coefficients = po2; toneA[ch].coefficients = tC;
            }
        } else if (vA == Voicing_RAT) {
            auto p1 = juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sr, 220.0f);
            auto p2 = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 2200.0f, 1.3f, juce::Decibels::decibelsToGain (4.5f));
            auto po1 = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, 7200.0f, 0.65f);
            auto po2 = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 100.0f, 1.8f, juce::Decibels::decibelsToGain (5.5f));
            auto tC = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, 1800.0f * std::pow (5.5f, tA), 0.707f);
            for (size_t ch = 0; ch < 2; ++ch) {
                preA1[ch].coefficients = p1; preA2[ch].coefficients = p2;
                postA1[ch].coefficients = po1; postA2[ch].coefficients = po2; toneA[ch].coefficients = tC;
            }
        } else if (vA == Voicing_Muff) {
            auto p1 = juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sr, 80.0f);
            auto p2 = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 280.0f, 1.0f, juce::Decibels::decibelsToGain (3.0f));
            auto po1 = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 450.0f, 0.9f, juce::Decibels::decibelsToGain (-6.5f));
            auto po2 = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 95.0f, 1.6f, juce::Decibels::decibelsToGain (6.5f));
            auto tC = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 1400.0f, 0.707f, juce::Decibels::decibelsToGain (-10.0f + tA * 20.0f));
            for (size_t ch = 0; ch < 2; ++ch) {
                preA1[ch].coefficients = p1; preA2[ch].coefficients = p2;
                postA1[ch].coefficients = po1; postA2[ch].coefficients = po2; toneA[ch].coefficients = tC;
            }
        }
    }

    int vB = bVoicingParam != nullptr ? (int)bVoicingParam->load() : 1;
    float tB = bToneParam != nullptr ? bToneParam->load() : 0.5f;
    float dB = bDriveParam != nullptr ? bDriveParam->load() : 0.5f;

    if (srChanged || vB != lastVoicingB || std::abs (tB - lastToneB) > 0.005f || std::abs (dB - lastDriveB) > 0.01f) {
        lastVoicingB = vB; lastToneB = tB; lastDriveB = dB;
        if (vB == Voicing_TS) {
            auto p1 = juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sr, 160.0f);
            auto p2 = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 850.0f, 1.0f, juce::Decibels::decibelsToGain (5.0f));
            auto po1 = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, 5800.0f, 0.707f);
            auto po2 = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, 120.0f, 0.707f, juce::Decibels::decibelsToGain (2.5f));
            auto tC = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 2500.0f, 0.707f, juce::Decibels::decibelsToGain (-8.0f + tB * 16.0f));
            for (size_t ch = 0; ch < 2; ++ch) {
                preB1[ch].coefficients = p1; preB2[ch].coefficients = p2;
                postB1[ch].coefficients = po1; postB2[ch].coefficients = po2; toneB[ch].coefficients = tC;
            }
        } else if (vB == Voicing_RAT) {
            auto p1 = juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sr, 220.0f);
            auto p2 = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 2200.0f, 1.3f, juce::Decibels::decibelsToGain (4.5f));
            auto po1 = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, 7200.0f, 0.65f);
            auto po2 = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 100.0f, 1.8f, juce::Decibels::decibelsToGain (6.5f));
            auto tC = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, 1800.0f * std::pow (5.5f, tB), 0.707f);
            for (size_t ch = 0; ch < 2; ++ch) {
                preB1[ch].coefficients = p1; preB2[ch].coefficients = p2;
                postB1[ch].coefficients = po1; postB2[ch].coefficients = po2; toneB[ch].coefficients = tC;
            }
        } else if (vB == Voicing_Muff) {
            auto p1 = juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sr, 80.0f);
            auto p2 = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 280.0f, 1.0f, juce::Decibels::decibelsToGain (3.0f));
            auto po1 = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 450.0f, 0.9f, juce::Decibels::decibelsToGain (-6.5f));
            auto po2 = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 90.0f, 1.5f, juce::Decibels::decibelsToGain (7.0f));
            auto tC = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 1400.0f, 0.707f, juce::Decibels::decibelsToGain (-10.0f + tB * 20.0f));
            for (size_t ch = 0; ch < 2; ++ch) {
                preB1[ch].coefficients = p1; preB2[ch].coefficients = p2;
                postB1[ch].coefficients = po1; postB2[ch].coefficients = po2; toneB[ch].coefficients = tC;
            }
        }
    }

    if (vA == Voicing_Lab || vB == Voicing_Lab) {
        constexpr float qFactor = 1.4f;
        for (size_t b = 0; b < numEqBands; ++b) {
            float prG = preEqParams[b] != nullptr ? preEqParams[b]->load() : 0.0f;
            if (srChanged || std::abs (prG - lastPreGains[b]) > 1e-4f || preEqFilters[0][b].coefficients == nullptr) {
                lastPreGains[b] = prG;
                auto c = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, eqFrequencies[b], qFactor, juce::Decibels::decibelsToGain (prG));
                preEqFilters[0][b].coefficients = preEqFilters[1][b].coefficients = c;
            }
            float poG = postEqParams[b] != nullptr ? postEqParams[b]->load() : 0.0f;
            if (srChanged || std::abs (poG - lastPostGains[b]) > 1e-4f || postEqFilters[0][b].coefficients == nullptr) {
                lastPostGains[b] = poG;
                auto c = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, eqFrequencies[b], qFactor, juce::Decibels::decibelsToGain (poG));
                postEqFilters[0][b].coefficients = postEqFilters[1][b].coefficients = c;
            }
        }
    }
}

void GomuGomuNoDrive::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    size_t numChannels = static_cast<size_t>(juce::jmax (1, getTotalNumOutputChannels()));
    
    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        numChannels, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
    oversampler->initProcessing (static_cast<size_t>(samplesPerBlock));
    oversampler->reset();
    
    for (size_t ch = 0; ch < 2; ++ch) {
        clippersA[ch].reset();
        clippersB[ch].reset();
        dcBlockerX[ch] = dcBlockerY[ch] = 0.0f;
    }

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(numChannels);
    cabConvolution.prepare (spec);
    cabConvolution.reset();

    lastSampleRate = 0.0;
    lastVoicingA = lastVoicingB = -1;

    juce::dsp::ProcessSpec filterSpec;
    filterSpec.sampleRate = sampleRate;
    filterSpec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    filterSpec.numChannels = 1;

    for (size_t ch = 0; ch < 2; ++ch) {
        preA1[ch].prepare (filterSpec); preA2[ch].prepare (filterSpec);
        postA1[ch].prepare (filterSpec); postA2[ch].prepare (filterSpec); toneA[ch].prepare (filterSpec);
        preB1[ch].prepare (filterSpec); preB2[ch].prepare (filterSpec);
        postB1[ch].prepare (filterSpec); postB2[ch].prepare (filterSpec); toneB[ch].prepare (filterSpec);
        interCutFilters[ch].prepare (filterSpec);

        for (size_t b = 0; b < numEqBands; ++b) {
            preEqFilters[ch][b].prepare (filterSpec);
            postEqFilters[ch][b].prepare (filterSpec);
        }
    }

    updateFilters();
    int targetIR = cabSelectParam != nullptr ? (int)cabSelectParam->load() : 0;
    loadBundledIR (targetIR);
}

void GomuGomuNoDrive::releaseResources() {
    oversampler.reset();
    cabConvolution.reset();
}

bool GomuGomuNoDrive::isBusesLayoutSupported (const BusesLayout& layouts) const {
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() && 
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet();
}

void GomuGomuNoDrive::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);

    int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() == 0 || numSamples == 0) return;
    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, numSamples);

    updateFilters();

    if (cabSelectParam != nullptr) {
        int selectedIR = static_cast<int>(cabSelectParam->load());
        if (selectedIR != currentLoadedIRIndex.load()) {
            loadBundledIR (selectedIR);
        }
    }

    const bool aActive = (aEnableParam != nullptr && aEnableParam->load() > 0.5f);
    const bool bActive = (bEnableParam != nullptr && bEnableParam->load() > 0.5f);
    const int routing  = routingParam != nullptr ? (int)routingParam->load() : 0;

    double sr = getSampleRate();
    float R_dc = (sr > 0.0) ? static_cast<float>(1.0 - (juce::MathConstants<double>::twoPi * 15.0 / sr)) : 0.998f;
    int maxChannels = std::min (buffer.getNumChannels(), 2);

    for (int ch = 0; ch < maxChannels; ++ch) {
        float* chData = buffer.getWritePointer (ch);
        float x1 = dcBlockerX[ch];
        float y1 = dcBlockerY[ch];
        for (int i = 0; i < numSamples; ++i) {
            float inSample = chData[i];
            float outSample = inSample - x1 + R_dc * y1;
            x1 = inSample; y1 = outSample;
            chData[i] = outSample;
        }
        dcBlockerX[ch] = x1; dcBlockerY[ch] = y1;
    }

    // --- PARAMÈTRES PÉDALE A (SOFT CLIPPING) ---
    const double aDriveAlpha = aDriveParam != nullptr ? (double)aDriveParam->load() : 0.5;
    // R_F variable : 54.7 kOhm à 504.7 kOhm
    const double RF_A = 4700.0 + 50000.0 * (1.0 + 9.0 * std::pow (aDriveAlpha, 1.6));
    // Niveau d'entrée calibré à 0.35 V crête
    constexpr double toVoltsScaleA = 0.35;
    const float levelLinA = aLevelParam != nullptr ? juce::Decibels::decibelsToGain (aLevelParam->load()) : 1.0f;
    const double alpha_up_A = aThreshPParam != nullptr ? (double)aThreshPParam->load() : 0.5;
    const double alpha_dn_A = aThreshMParam != nullptr ? (double)aThreshMParam->load() : 0.5;
    const double C_up_A = aCapPParam != nullptr ? std::max ((double)aCapPParam->load() * 1e-9, 1e-15) : 1e-7;
    const double C_dn_A = aCapMParam != nullptr ? std::max ((double)aCapMParam->load() * 1e-9, 1e-15) : 1e-7;
    const int pos_up_A = aCapPosPParam != nullptr ? (int)aCapPosPParam->load() : 0;
    const int pos_dn_A = aCapPosMParam != nullptr ? (int)aCapPosMParam->load() : 0;
    const int voicingA = aVoicingParam != nullptr ? (int)aVoicingParam->load() : 0;

    // --- PARAMÈTRES PÉDALE B (HARD CLIPPING) ---
    const double bDriveAlpha = bDriveParam != nullptr ? (double)bDriveParam->load() : 0.5;
    const double toVoltsScaleB = 0.4 * juce::Decibels::decibelsToGain (bDriveAlpha * 45.0);
    const float levelLinB = bLevelParam != nullptr ? juce::Decibels::decibelsToGain (bLevelParam->load()) : 1.0f;
    const double alpha_up_B = bThreshPParam != nullptr ? (double)bThreshPParam->load() : 0.5;
    const double alpha_dn_B = bThreshMParam != nullptr ? (double)bThreshMParam->load() : 0.5;
    const double C_up_B = bCapPParam != nullptr ? std::max ((double)bCapPParam->load() * 1e-9, 1e-15) : 1e-7;
    const double C_dn_B = bCapMParam != nullptr ? std::max ((double)bCapMParam->load() * 1e-9, 1e-15) : 1e-7;
    const int pos_up_B = bCapPosPParam != nullptr ? (int)bCapPosPParam->load() : 0;
    const int pos_dn_B = bCapPosMParam != nullptr ? (int)bCapPosMParam->load() : 0;
    const double R_k_up_B = bKneePParam != nullptr ? (double)bKneePParam->load() : 10.0;
    const double R_k_dn_B = bKneeMParam != nullptr ? (double)bKneeMParam->load() : 10.0;
    const double R_p_up_B = bPinchPParam != nullptr ? (double)bPinchPParam->load() : 1000.0;
    const double R_p_dn_B = bPinchMParam != nullptr ? (double)bPinchMParam->load() : 1000.0;
    const int voicingB = bVoicingParam != nullptr ? (int)bVoicingParam->load() : 1;

    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::AudioBlock<float> osBlock;
    if (oversampler != nullptr) osBlock = oversampler->processSamplesUp (block);
    else osBlock = block;

    double sampleRateOS = getSampleRate() * (oversampler != nullptr ? oversampler->getOversamplingFactor() : 1);
    double dt = 1.0 / sampleRateOS;

    int startIdx = scopeData.writeIndex.load (std::memory_order_relaxed);
    int wIdx = startIdx;

    float pPlusVA = 0.0f, pMinusVA = 0.0f;
    float pPlusVB = 0.0f, pMinusVB = 0.0f;

    auto processPedalA = [&](float inSample, size_t ch) -> float {
        float s = inSample;
        if (voicingA == Voicing_Lab) {
            for (size_t b = 0; b < numEqBands; ++b) s = preEqFilters[ch][b].processSample (s);
        } else {
            s = preA1[ch].processSample (s);
            s = preA2[ch].processSample (s);
        }

        double vin = static_cast<double>(s) * toVoltsScaleA;
        if (ch == 0) {
            if (vin > pPlusVA) pPlusVA = (float)vin;
            if (vin < pMinusVA) pMinusVA = (float)vin;
        }

        double vout = clippersA[ch].processSample (vin, dt, alpha_up_A, C_up_A, pos_up_A,
                                                   alpha_dn_A, C_dn_A, pos_dn_A, RF_A);
        float outNorm = static_cast<float>(vout / 1.8) * levelLinA;

        if (voicingA == Voicing_Lab) {
            for (size_t b = 0; b < numEqBands; ++b) outNorm = postEqFilters[ch][b].processSample (outNorm);
        } else {
            outNorm = postA1[ch].processSample (outNorm);
            outNorm = postA2[ch].processSample (outNorm);
            outNorm = toneA[ch].processSample (outNorm);
        }
        return outNorm;
    };

    auto processPedalB = [&](float inSample, size_t ch) -> float {
        float s = inSample;
        if (voicingB == Voicing_Lab) {
            for (size_t b = 0; b < numEqBands; ++b) s = preEqFilters[ch][b].processSample (s);
        } else {
            s = preB1[ch].processSample (s);
            s = preB2[ch].processSample (s);
        }

        double vin = static_cast<double>(s) * toVoltsScaleB;
        if (ch == 0) {
            if (vin > pPlusVB) pPlusVB = (float)vin;
            if (vin < pMinusVB) pMinusVB = (float)vin;
        }

        double vout = clippersB[ch].processSample (vin, 10000.0, dt,
            alpha_up_B, R_k_up_B, R_p_up_B, C_up_B, pos_up_B,
            alpha_dn_B, R_k_dn_B, R_p_dn_B, C_dn_B, pos_dn_B);
        
        float outNorm = static_cast<float>(vout / 1.8) * levelLinB;

        if (voicingB == Voicing_Lab) {
            for (size_t b = 0; b < numEqBands; ++b) outNorm = postEqFilters[ch][b].processSample (outNorm);
        } else {
            outNorm = postB1[ch].processSample (outNorm);
            outNorm = postB2[ch].processSample (outNorm);
            outNorm = toneB[ch].processSample (outNorm);
        }
        return outNorm;
    };

    for (size_t ch = 0; ch < osBlock.getNumChannels(); ++ch) {
        float* data = osBlock.getChannelPointer (ch);
        if (ch >= 2) continue;

        for (size_t i = 0; i < osBlock.getNumSamples(); ++i) {
            float inSig = data[i];
            float outSig = inSig;

            float curInA = inSig, curOutA = inSig;
            float curInB = inSig, curOutB = inSig;

            if (routing == Routing_A_to_B) {
                curInA = inSig;
                curOutA = aActive ? processPedalA (curInA, ch) : curInA;
                
                curInB = (aActive && bActive) ? interCutFilters[ch].processSample (curOutA) : curOutA;
                curOutB = bActive ? processPedalB (curInB, ch) : curInB;
                outSig = curOutB;
            }
            else if (routing == Routing_B_to_A) {
                curInB = inSig;
                curOutB = bActive ? processPedalB (curInB, ch) : curInB;

                curInA = (aActive && bActive) ? interCutFilters[ch].processSample (curOutB) : curOutB;
                curOutA = aActive ? processPedalA (curInA, ch) : curInA;
                outSig = curOutA;
            }
            else {
                curInA = inSig;
                curOutA = aActive ? processPedalA (curInA, ch) : curInA;

                curInB = inSig;
                curOutB = bActive ? processPedalB (curInB, ch) : curInB;

                if (aActive && bActive) outSig = 0.5f * (curOutA + curOutB);
                else if (aActive)      outSig = curOutA;
                else if (bActive)      outSig = curOutB;
                else                   outSig = inSig;
            }

            data[i] = outSig;

            if (ch == 0 && (i % 2 == 0)) {
                scopeData.bufferInA[(size_t)wIdx]  = curInA;
                scopeData.bufferOutA[(size_t)wIdx] = curOutA;
                scopeData.bufferInB[(size_t)wIdx]  = curInB;
                scopeData.bufferOutB[(size_t)wIdx] = curOutB;
                wIdx = (wIdx + 1) % ScopeData::bufferSize;
            }
        }
    }

    if (oversampler != nullptr) oversampler->processSamplesDown (block);

    bool cabEnabled = (cabEnableParam != nullptr && cabEnableParam->load() > 0.5f);
    if (cabEnabled && hasValidIR.load()) {
        cabConvolution.process (juce::dsp::ProcessContextReplacing<float>(block));
    }

    scopeData.writeIndex.store (wIdx, std::memory_order_release);
    scopeData.peakInVoltsA_Plus.store (pPlusVA, std::memory_order_relaxed);
    scopeData.peakInVoltsA_Minus.store (pMinusVA, std::memory_order_relaxed);
    scopeData.peakInVoltsB_Plus.store (pPlusVB, std::memory_order_relaxed);
    scopeData.peakInVoltsB_Minus.store (pMinusVB, std::memory_order_relaxed);
}

const juce::String GomuGomuNoDrive::getName() const { return JucePlugin_Name; }
bool GomuGomuNoDrive::acceptsMidi() const { return false; }
bool GomuGomuNoDrive::producesMidi() const { return false; }
bool GomuGomuNoDrive::isMidiEffect() const { return false; }
double GomuGomuNoDrive::getTailLengthSeconds() const { return 0.0; }
int GomuGomuNoDrive::getNumPrograms() { return 1; }
int GomuGomuNoDrive::getCurrentProgram() { return 0; }
void GomuGomuNoDrive::setCurrentProgram (int index) { juce::ignoreUnused (index); }
const juce::String GomuGomuNoDrive::getProgramName (int index) { juce::ignoreUnused (index); return {}; }
void GomuGomuNoDrive::changeProgramName (int index, const juce::String& newName) { juce::ignoreUnused (index, newName); }

bool GomuGomuNoDrive::hasEditor() const { return true; }
juce::AudioProcessorEditor* GomuGomuNoDrive::createEditor() { return new GomuGomuNoDriveEditor (*this); }

void GomuGomuNoDrive::getStateInformation (juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void GomuGomuNoDrive::setStateInformation (const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType())) {
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
        if (cabSelectParam != nullptr) loadBundledIR (static_cast<int>(cabSelectParam->load()));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GomuGomuNoDrive(); }
