#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
===============================================================================
RUBBER ZENER TOPOLOGY - GLOBAL MULTI-TRACE EBERS-MOLL EXTRACTION ENGINE
===============================================================================
Author: Matthieu Loumaigne
Date: September 2026

Description:
This script performs a joint, global non-linear least-squares optimization 
(Levenberg-Marquardt via LMFIT) across multiple experimental acquisition 
traces of the half-wave Rubber Zener clipping circuit.

Key Physical & Algorithmic Features:
1. Universal BJT Parameters:
   - The core transistor physical properties (saturation current log_Is, 
     forward current gain Bf, and reverse current gain Br) are strictly global 
     and shared across all operating regimes and frequencies.
2. Transient Warm-Up (Periodic Steady State):
   - Numerical integration starts 8 cycles prior to the acquisition window 
     to eliminate initial reactive transients and guarantee true steady-state 
     harmonic evaluation.
3. Dual-Mode Reactive Topologies:
   - Mode 1: Capacitor placed across the upper divider branch (cap_pos="up", 
     C || R_up), producing high-frequency transient clamping.
   - Mode 2: Capacitor placed across the lower divider branch (cap_pos="down", 
     C || R_low), creating an acute turn-on overshoot followed by envelope collapse.
4. Input Phase & Amplitude Alignment:
   - For oscilloscope acquisitions lacking a dedicated generator reference channel, 
     the input excitation waveform is reconstructed analytically via linear 
     least-squares orthogonal projection on the unclipped negative half-wave.
5. Publication-Ready Data Export:
   - Formatted CSV tables (time, Vin, Vout_meas, Vout_model, residual) are exported 
     for direct vector plotting (e.g., TikZ/PGFPlots).
===============================================================================
"""

import os
import glob
import numpy as np
import matplotlib.pyplot as plt
from lmfit import Parameters, minimize, fit_report

# =============================================================================
# 1. EXPERIMENTAL CONDITIONS & COMPONENT NOMINAL SPECIFICATIONS
# =============================================================================
# Tolerances applied as relative boundary bounds in the parameter optimizer:
TOL_PASSIVES = 0.08    # +/- 8% bounding window on nominal passive components (R, C)
TOL_RLOW = 0.15        # +/- 15% bounding window on potentiometer wiper position
N_WARMUP_CYCLES = 8    # Pre-simulation cycles executed to reach periodic steady state

# Target dataset registry matching laboratory acquisition file prefixes:
EXP_CONFIGS = {
    "AC_200Hz_32p1nF_Rpinch2.47k_Rgamma_36p6K": {
        "freq": 200.0,
        "C": 32.1e-9,
        "R_pinch": 2.47e3,
        "R_gamma": 36.6e3,
        "R_low_nom": 8.0e3,
        "R_s": 1.48e3,
        "cap_pos": "up",
    },
    "AC_500Hz_32p1nF_Rpinch2.47k_Rgamma_36p6K": {
        "freq": 500.0,
        "C": 32.1e-9,
        "R_pinch": 2.47e3,
        "R_gamma": 36.6e3,
        "R_low_nom": 13.0e3,
        "R_s": 1.48e3,
        "cap_pos": "up",
    },
    "AC_1000Hz_32p1nF_Rgamma_15p6K": {
        "freq": 1000.0,
        "C": 32.1e-9,
        "R_pinch": 0.0,
        "R_gamma": 15.6e3,
        "R_low_nom": 11.8e3,
        "R_s": 1.48e3,
        "cap_pos": "down",  # Mode 2: dynamic turn-on overshoot
    },
    "AC_1000Hz_32p1nF_Rpinch2.47k_Rgamma_36p6K": {
        "freq": 1000.0,
        "C": 32.1e-9,
        "R_pinch": 2.47e3,
        "R_gamma": 36.6e3,
        "R_low_nom": 9.5e3,
        "R_s": 1.48e3,
        "cap_pos": "up",
    },
    "AC_1000Hz_no_capa_Rpinch600Ohm": {
        "freq": 1000.0,
        "C": 0.0,
        "R_pinch": 600.0,
        "R_gamma": 15.6e3,
        "R_low_nom": 34.0e3,
        "R_s": 1.48e3,
        "cap_pos": "up",
    },
    "AC_1000Hz_Rgamma_15p6K_Rserie_304Ohm": {
        "freq": 1000.0,
        "C": 0.0,
        "R_pinch": 0.0,
        "R_gamma": 15.6e3,
        "R_low_nom": 21.0e3,
        "R_s": 304.0,
        "cap_pos": "up",
    },
}

# =============================================================================
# 2. NUMERICAL SOLVER: STATE-SPACE EBERS-MOLL WITH STEADY-STATE WARM-UP
# =============================================================================
def simulate_single_clipper(
    t, Vin, C, R_up, R_low, R_p, R_s, Is, Bf, Br, cap_pos="up",
    freq=1000.0, amp=3.5, phi=0.0, voff=0.0, n_warmup=N_WARMUP_CYCLES,
    Vt=0.02585, IsD=21e-9
):
    """
    Simulates the transient response of the half-wave Rubber Zener circuit.
    
    Parameters:
    -----------
    t : ndarray
        Time evaluation grid (seconds).
    Vin : ndarray
        Input excitation voltage vector (Volts).
    C : float
        Capacitance value (Farads).
    R_up : float
        Upper branch divider resistance (Ohms).
    R_low : float
        Lower branch divider resistance (Ohms).
    R_p : float
        Plateau resistor in series with the collector (Ohms).
    R_s : float
        Total series impedance combining generator and knee resistance (Ohms).
    Is : float
        BJT transport saturation current (Amperes).
    Bf : float
        Forward common-emitter current gain (beta_F).
    Br : float
        Reverse common-emitter current gain (beta_R).
    cap_pos : str
        Capacitor placement: "up" for Mode 1 (C || R_up), "down" for Mode 2 (C || R_low).
    freq, amp, phi, voff : float
        Harmonic parameters used to synthesize the pre-simulation warm-up signal.
    n_warmup : int
        Number of pre-simulation fundamental periods.
    Vt : float
        Thermal voltage (V_T = k_B * T / q).
    IsD : float
        Series Schottky diode reverse saturation current (Amperes).

    Returns:
    --------
    ndarray: Modeled output voltage vector matching the time grid 't'.
    """
    dt = t[1] - t[0]

    # --- Pre-simulation Warm-up Stage ---
    # Synthesize prior sinusoidal cycles to decay initial charge transients
    if C > 0.0 and n_warmup > 0:
        T_period = 1.0 / freq
        n_pts_period = max(1, int(round(T_period / dt)))
        n_warmup_pts = n_warmup * n_pts_period

        t_pre = t[0] - np.arange(n_warmup_pts, 0, -1, dtype=np.float64) * dt
        vin_pre = amp * np.sin(2.0 * np.pi * freq * t_pre + phi) + voff

        t_sim = np.concatenate([t_pre, t])
        vin_sim = np.concatenate([vin_pre, Vin])
        offset = n_warmup_pts
    else:
        t_sim = t
        vin_sim = Vin
        offset = 0

    N = len(t_sim)
    Vout = np.zeros(N, dtype=np.float64)
    X = np.zeros(4, dtype=np.float64)  # State vector: [V_up, V_BE, V_BC, V_D]

    # --- Time-Stepping Integration Loop ---
    for n in range(N):
        vin = vin_sim[n]

        # ---------------------------------------------------------------------
        # Negative Half-Wave: Diode is reverse-biased (branch off)
        # ---------------------------------------------------------------------
        if vin <= 0.0:
            Vout[n] = vin
            # Passive exponential bleeding of the divider capacitor
            if C > 0.0:
                if cap_pos == "up":
                    X[0] *= np.exp(-dt / (R_up * C))
                    X[1] = 0.0
                else:
                    X[1] *= np.exp(-dt / (R_low * C))
                    X[0] = 0.0
            else:
                X[0] = 0.0
                X[1] = 0.0
            X[2] = 0.0
            X[3] = 0.0
            continue

        # ---------------------------------------------------------------------
        # Positive Half-Wave: Diode conducts -> Solve 4D Implicit Newton-Raphson
        # ---------------------------------------------------------------------
        Vup_prev = X[0]
        Vbe_prev = X[1]

        for _ in range(30):
            Vup, Vbe, Vbc, Vd = X

            # Argument clamping to protect against floating-point exponential overflow
            x_be = np.clip(Vbe / Vt, -60.0, 35.0)
            x_bc = np.clip(Vbc / Vt, -60.0, 35.0)
            x_d = np.clip(Vd / Vt, -60.0, 35.0)

            e_be = np.exp(x_be)
            e_bc = np.exp(x_bc)
            e_d = np.exp(x_d)

            # Ebers-Moll transport currents and Schottky diode Shockley law
            ic = Is * (e_be - e_bc) - (Is / Br) * (e_bc - 1.0)
            ib = (Is / Bf) * (e_be - 1.0) + (Is / Br) * (e_bc - 1.0)
            iD = IsD * (e_d - 1.0)

            # Analytic partial derivatives for the Jacobian formulation
            dic_dvbe = (Is / Vt) * e_be
            dic_dvbc = -(Is / Vt) * (1.0 + 1.0 / Br) * e_bc
            dib_dvbe = (Is / (Bf * Vt)) * e_be
            dib_dvbc = (Is / (Br * Vt)) * e_bc
            diD_dvd = (IsD / Vt) * e_d

            # Equation F1: Internal collector loop Kirchhoff's Voltage Law (KVL) across R_p
            f1 = Vbc - R_p * ic + Vup
            df1_dvup = 1.0
            df1_dvbe = -R_p * dic_dvbe
            df1_dvbc = 1.0 - R_p * dic_dvbc
            df1_dvd = 0.0

            # Equations F2 & F3: Base Kirchhoff's Current Law (KCL) conditioned by capacitor mode
            if C > 0.0:
                if cap_pos == "up":
                    # Mode 1: Capacitor in parallel with upper resistor (C || R_up)
                    f2 = C * (Vup - Vup_prev) / dt + Vup / R_up - ib - Vbe / R_low
                    df2_dvup = C / dt + 1.0 / R_up
                    df2_dvbe = -dib_dvbe - 1.0 / R_low
                    df2_dvbc = -dib_dvbc
                    df2_dvd = 0.0

                    f3 = iD - ic - ib - Vbe / R_low
                    df3_dvup = 0.0
                    df3_dvbe = -dic_dvbe - dib_dvbe - 1.0 / R_low
                    df3_dvbc = -dic_dvbc - dib_dvbc
                    df3_dvd = diD_dvd
                else:
                    # Mode 2: Capacitor in parallel with lower resistor (C || R_low)
                    f2 = C * (Vbe - Vbe_prev) / dt + Vbe / R_low + ib - Vup / R_up
                    df2_dvup = -1.0 / R_up
                    df2_dvbe = C / dt + 1.0 / R_low + dib_dvbe
                    df2_dvbc = dib_dvbc
                    df2_dvd = 0.0

                    f3 = iD - ic - Vup / R_up
                    df3_dvup = -1.0 / R_up
                    df3_dvbe = -dic_dvbe
                    df3_dvbc = -dic_dvbc
                    df3_dvd = diD_dvd
            else:
                # Memoryless regime (C = 0)
                f2 = Vup / R_up - ib - Vbe / R_low
                df2_dvup = 1.0 / R_up
                df2_dvbe = -dib_dvbe - 1.0 / R_low
                df2_dvbc = -dib_dvbc
                df2_dvd = 0.0

                f3 = iD - ic - ib - Vbe / R_low
                df3_dvup = 0.0
                df3_dvbe = -dic_dvbe - dib_dvbe - 1.0 / R_low
                df3_dvbc = -dic_dvbc - dib_dvbc
                df3_dvd = diD_dvd

            # Equation F4: Global input loop KVL incorporating source impedance R_s
            f4 = vin - R_s * iD - Vd - Vup - Vbe
            df4_dvup = -1.0
            df4_dvbe = -1.0
            df4_dvbc = 0.0
            df4_dvd = -R_s * diD_dvd - 1.0

            # Residual vector and Jacobian matrix assembly
            F = np.array([f1, f2, f3, f4], dtype=np.float64)
            J = np.array(
                [
                    [df1_dvup, df1_dvbe, df1_dvbc, df1_dvd],
                    [df2_dvup, df2_dvbe, df2_dvbc, df2_dvd],
                    [df3_dvup, df3_dvbe, df3_dvbc, df3_dvd],
                    [df4_dvup, df4_dvbe, df4_dvbc, df4_dvd],
                ],
                dtype=np.float64,
            )

            # Linear solve for state update vector dX
            try:
                dX = np.linalg.solve(J, F)
            except np.linalg.LinAlgError:
                dX = F * 0.01

            # Step damping: clamp update magnitude to prevent divergence on steep gradients
            step = np.clip(dX, -0.3, 0.3)
            X -= step

            # Enforce physical silicon conduction ceilings on forward-biased junctions
            X[1] = min(X[1], 0.88)
            X[2] = min(X[2], 0.88)
            X[3] = min(X[3], 0.88)

            # Convergence check based on infinity norm of update step
            if np.max(np.abs(step)) < 1e-6:
                break

        # Compute output voltage at current sample: Vout = Vin - Rs * iD
        e_d_final = np.exp(np.clip(X[3] / Vt, -60.0, 35.0))
        iD_final = IsD * (e_d_final - 1.0)
        Vout[n] = vin - R_s * iD_final

    # Crop the warm-up transient to return the aligned steady-state window
    return Vout[offset:]

# =============================================================================
# 3. EXPERIMENTAL DATA LOADER & PARSER
# =============================================================================
def load_data_traces(exp_configs, data_dirs=["data", ".", "mesures"]):
    """
    Scans candidate directories, parses oscilloscope text files, and extracts 
    time and voltage arrays.
    """
    loaded_data = {}
    missing_files = []

    for key in exp_configs.keys():
        found = False
        for d in data_dirs:
            pattern = os.path.join(d, f"{key}*")
            matches = glob.glob(pattern)
            for m in matches:
                if os.path.isfile(m) and not m.endswith(".py"):
                    try:
                        # Attempt reading with various standard delimiters
                        raw = np.genfromtxt(m, delimiter=None, skip_header=1)
                        if np.isnan(raw).any() or len(raw) == 0:
                            raw = np.genfromtxt(m, delimiter=";", skip_header=1)
                        if np.isnan(raw).any() or len(raw) == 0:
                            raw = np.genfromtxt(m, delimiter=",", skip_header=1)

                        if raw.shape[1] >= 2:
                            t = raw[:, 0]
                            t = t - t[0]  # Normalize time origin to zero

                            # Identify single-trace (Vout only) vs dual-trace (Vin, Vout) acquisitions
                            if raw.shape[1] >= 3 and not np.allclose(raw[:, 2], 0):
                                Vin_raw = raw[:, 1]
                                Vout_raw = raw[:, 2]
                            else:
                                Vin_raw = None
                                Vout_raw = raw[:, 1]

                            loaded_data[key] = {
                                "t": t,
                                "Vin": Vin_raw,
                                "Vout": Vout_raw,
                            }
                            found = True
                            break
                    except Exception:
                        continue
            if found:
                break

        if not found:
            missing_files.append(key)

    if missing_files:
        print(f"[WARNING] Missing acquisition data files: {missing_files}")

    return loaded_data

# =============================================================================
# 4. LMFIT PARAMETER INITIALIZATION (GLOBAL CONSTRAINTS)
# =============================================================================
def build_global_lmfit_parameters(exp_configs, data_dict):
    """
    Constructs the unified parameter dictionary for multi-curve fitting:
    - Global shared physical transistor variables (log_Is, Bf, Br).
    - Local bounded physical tolerances per experimental trace.
    - Linear least-squares phase/amplitude estimates for unmonitored excitation.
    """
    params = Parameters()

    # --- 1. TRANSISTOR PARAMETERS: STRICTLY SHARED & GLOBAL ---
    params.add("log_Is", value=-14.15, min=-15.5, max=-12.5)
    params.add("Bf", value=450.0, min=200.0, max=700.0)
    params.add("Br", value=2.88, min=1.0, max=6.0)

    # --- 2. LOCAL PARAMETERS (PER ACQUISITION TRACE) ---
    tol_p = TOL_PASSIVES
    tol_rl = TOL_RLOW

    for i, (key, cfg) in enumerate(exp_configs.items()):
        prefix = f"c{i}_"
        d = data_dict[key]

        # Branch capacitor C
        if cfg["C"] > 0:
            params.add(f"{prefix}d_C", value=0.0, min=-tol_p, max=tol_p)
        else:
            params.add(f"{prefix}d_C", value=0.0, vary=False)

        # Plateau collector resistor R_p
        if cfg["R_pinch"] > 0:
            params.add(f"{prefix}d_Rp", value=0.0, min=-tol_p, max=tol_p)
        else:
            params.add(f"{prefix}d_Rp", value=0.0, vary=False)

        # Resistors R_gamma and R_s
        params.add(f"{prefix}d_Rg", value=0.0, min=-tol_p, max=tol_p)
        params.add(f"{prefix}d_Rs", value=0.0, min=-tol_p, max=tol_p)

        # Potentiometer lower branch resistance R_low
        params.add(f"{prefix}d_Rlow", value=0.0, min=-tol_rl, max=tol_rl)

        # When Vin is not directly measured, fit the input sine via the negative arch
        if d["Vin"] is None:
            neg_mask = d["Vout"] < -0.5
            t_neg = d["t"][neg_mask]
            v_neg = d["Vout"][neg_mask]
            omega = 2.0 * np.pi * cfg["freq"]

            # Linear least-squares projection onto [cos(wt), sin(wt), 1]
            M = np.column_stack(
                [np.cos(omega * t_neg), np.sin(omega * t_neg), np.ones_like(t_neg)]
            )
            coeffs, _, _, _ = np.linalg.lstsq(M, v_neg, rcond=None)
            A, B, C = coeffs

            amp_init = float(np.sqrt(A**2 + B**2))
            phi_init = float(np.arctan2(A, B))
            voff_init = float(C)

            params.add(
                f"{prefix}amp",
                value=amp_init,
                min=amp_init * 0.90,
                max=amp_init * 1.10,
            )
            params.add(
                f"{prefix}phi",
                value=phi_init,
                min=phi_init - 0.25,
                max=phi_init + 0.25,
            )
            params.add(f"{prefix}voff", value=voff_init, min=-0.08, max=0.08)
        else:
            params.add(f"{prefix}gain_in", value=1.0, min=0.96, max=1.04)
            params.add(f"{prefix}voff", value=0.0, min=-0.05, max=0.05)

    return params

# =============================================================================
# 5. GLOBAL RESIDUAL COST FUNCTION
# =============================================================================
def global_residual(params, exp_configs, data_dict, decimate_factor=1):
    """
    Computes the concatenated, normalized error vector across all datasets.
    """
    Is = 10.0 ** params["log_Is"].value
    Bf = params["Bf"].value
    Br = params["Br"].value

    residuals = []

    for i, (key, cfg) in enumerate(exp_configs.items()):
        prefix = f"c{i}_"
        d = data_dict[key]

        # Apply optimized relative deviations to nominal values
        C = cfg["C"] * (1.0 + params[f"{prefix}d_C"].value)
        R_p = cfg["R_pinch"] * (1.0 + params[f"{prefix}d_Rp"].value)
        R_up = cfg["R_gamma"] * (1.0 + params[f"{prefix}d_Rg"].value)
        R_s = cfg["R_s"] * (1.0 + params[f"{prefix}d_Rs"].value)
        R_low = cfg["R_low_nom"] * (1.0 + params[f"{prefix}d_Rlow"].value)

        t = d["t"][::decimate_factor]
        Vout_meas = d["Vout"][::decimate_factor]

        # Determine instantaneous input excitation
        if d["Vin"] is None:
            amp = params[f"{prefix}amp"].value
            phi = params[f"{prefix}phi"].value
            voff = params[f"{prefix}voff"].value
            Vin_eval = amp * np.sin(2.0 * np.pi * cfg["freq"] * t + phi) + voff
        else:
            gain_in = params[f"{prefix}gain_in"].value
            voff = params[f"{prefix}voff"].value
            Vin_eval = d["Vin"][::decimate_factor] * gain_in + voff
            amp = np.max(np.abs(Vin_eval))
            phi = 0.0

        cap_pos = cfg.get("cap_pos", "up")
        Vout_model = simulate_single_clipper(
            t,
            Vin_eval,
            C,
            R_up,
            R_low,
            R_p,
            R_s,
            Is,
            Bf,
            Br,
            cap_pos=cap_pos,
            freq=cfg["freq"],
            amp=amp,
            phi=phi,
            voff=voff,
            n_warmup=N_WARMUP_CYCLES,
        )

        # Normalize by oscilloscope vertical noise floor estimate (10 mV)
        sigma_noise = 0.010
        residuals.append((Vout_meas - Vout_model) / sigma_noise)

    return np.concatenate(residuals)

# =============================================================================
# 6. EXECUTION OF GLOBAL FIT, EXPORT & VISUALIZATION
# =============================================================================
def main():
    print("=" * 80)
    print("GLOBAL EBERS-MOLL EXTRACTION (STEADY-STATE & SHARED TRANSISTOR PARAMETERS)")
    print("=" * 80)

    data_dict = load_data_traces(EXP_CONFIGS)

    first_key = list(EXP_CONFIGS.keys())[0]
    total_pts = len(data_dict[first_key]["t"])
    decim = max(1, total_pts // 600)

    print(f"Raw points per trace               : {total_pts}")
    print(f"Decimation factor for optimization : {decim}")
    print(f"Steady-state warm-up duration      : {N_WARMUP_CYCLES} cycles")

    params = build_global_lmfit_parameters(EXP_CONFIGS, data_dict)
    n_libres = sum(1 for p in params.values() if p.vary)
    print(f"Total free parameters              : {n_libres} (including 3 global BJT parameters)")

    print("\nRunning lmfit.minimize (Levenberg-Marquardt algorithm)...")
    result = minimize(
        global_residual,
        params,
        args=(EXP_CONFIGS, data_dict, decim),
        method="leastsq",
        max_nfev=400,
    )

    print("\n" + "=" * 80)
    print("LMFIT OPTIMIZATION REPORT")
    print("=" * 80)
    print(fit_report(result))

    p_opt = result.params
    Is_opt = 10.0 ** p_opt["log_Is"].value
    Bf_opt = p_opt["Bf"].value
    Br_opt = p_opt["Br"].value

    print("\n" + "-" * 80)
    print("IDENTIFIED PHYSICAL TRANSISTOR PARAMETERS (GLOBAL)")
    print(f"  Is       = {Is_opt:.3e} A  (log_Is = {p_opt['log_Is'].value:.3f})")
    print(f"  Beta_F   = {Bf_opt:.1f}")
    print(f"  Beta_R   = {Br_opt:.2f}")
    print("-" * 80)

    fig, axes = plt.subplots(3, 2, figsize=(15, 11), sharex=False, sharey=True)
    axes = axes.flatten()

    total_chi2 = 0.0
    total_pts_full = 0
    summary_rows = []

    # -------------------------------------------------------------------------
    # EXPORT RAW CURVES FOR PUBLICATION LAYOUT (PGFPLOTS / VECTOR ASSETS)
    # -------------------------------------------------------------------------
    out_dir = "export_curves_for_paper"
    os.makedirs(out_dir, exist_ok=True)

    for i, (key, cfg) in enumerate(EXP_CONFIGS.items()):
        prefix = f"c{i}_"
        d = data_dict[key]
        t = d["t"]
        Vout_meas = d["Vout"]

        # Re-synthesize aligned excitation signals on full grid
        if d["Vin"] is None:
            amp = p_opt[f"{prefix}amp"].value
            phi = p_opt[f"{prefix}phi"].value
            voff = p_opt[f"{prefix}voff"].value
            Vin = amp * np.sin(2.0 * np.pi * cfg["freq"] * t + phi) + voff
        else:
            Vin = d["Vin"] * p_opt[f"{prefix}gain_in"].value + p_opt[f"{prefix}voff"].value

        Vout_sim = simulate_single_clipper(
            t, Vin,
            cfg["C"] * (1.0 + p_opt[f"{prefix}d_C"].value),
            cfg["R_gamma"] * (1.0 + p_opt[f"{prefix}d_Rg"].value),
            cfg["R_low_nom"] * (1.0 + p_opt[f"{prefix}d_Rlow"].value),
            cfg["R_pinch"] * (1.0 + p_opt[f"{prefix}d_Rp"].value),
            cfg["R_s"] * (1.0 + p_opt[f"{prefix}d_Rs"].value),
            Is_opt, Bf_opt, Br_opt,
            cap_pos=cfg.get("cap_pos", "up"),
            freq=cfg["freq"],
            amp=np.max(np.abs(Vin)),
            phi=0.0 if d["Vin"] is not None else phi,
            voff=voff,
            n_warmup=N_WARMUP_CYCLES,
        )

        # Export clean 5-column CSV file
        data_table = np.column_stack([
            t * 1000.0,                   # Time (ms)
            Vin,                          # Vin (V)
            Vout_meas,                    # Measured Vout (V)
            Vout_sim,                     # Simulated Ebers-Moll Vout (V)
            (Vout_meas - Vout_sim) * 1e3  # Direct residual (mV)
        ])

        header = "time_ms,Vin_V,Vout_meas_V,Vout_model_V,residual_mV"
        csv_path = os.path.join(out_dir, f"fit_{key}.csv")
        np.savetxt(csv_path, data_table, delimiter=",", header=header, comments="", fmt="%.6e")

    print(f"\n[EXPORT] Processed traces successfully exported to ./{out_dir}/")

    # -------------------------------------------------------------------------
    # METRICS EVALUATION & MATPLOTLIB FIGURE RENDERING
    # -------------------------------------------------------------------------
    for i, (key, cfg) in enumerate(EXP_CONFIGS.items()):
        prefix = f"c{i}_"
        d = data_dict[key]
        t = d["t"]
        Vout_meas = d["Vout"]

        C_opt = cfg["C"] * (1.0 + p_opt[f"{prefix}d_C"].value)
        Rp_opt = cfg["R_pinch"] * (1.0 + p_opt[f"{prefix}d_Rp"].value)
        Rg_opt = cfg["R_gamma"] * (1.0 + p_opt[f"{prefix}d_Rg"].value)
        Rs_opt = cfg["R_s"] * (1.0 + p_opt[f"{prefix}d_Rs"].value)
        Rlow_opt = cfg["R_low_nom"] * (1.0 + p_opt[f"{prefix}d_Rlow"].value)

        if d["Vin"] is None:
            amp = p_opt[f"{prefix}amp"].value
            phi = p_opt[f"{prefix}phi"].value
            voff = p_opt[f"{prefix}voff"].value
            Vin = amp * np.sin(2.0 * np.pi * cfg["freq"] * t + phi) + voff
        else:
            Vin = d["Vin"] * p_opt[f"{prefix}gain_in"].value + p_opt[f"{prefix}voff"].value
            amp = np.max(np.abs(Vin))
            phi = 0.0
            voff = p_opt[f"{prefix}voff"].value

        cap_pos = cfg.get("cap_pos", "up")
        Vout_sim = simulate_single_clipper(
            t,
            Vin,
            C_opt,
            Rg_opt,
            Rlow_opt,
            Rp_opt,
            Rs_opt,
            Is_opt,
            Bf_opt,
            Br_opt,
            cap_pos=cap_pos,
            freq=cfg["freq"],
            amp=amp,
            phi=phi,
            voff=voff,
            n_warmup=N_WARMUP_CYCLES,
        )

        res = Vout_meas - Vout_sim
        chi2_local = np.sum((res / 0.010) ** 2)
        nrmse = (
            np.sqrt(np.mean(res**2)) / (np.max(Vout_meas) - np.min(Vout_meas)) * 100.0
        )

        total_chi2 += chi2_local
        total_pts_full += len(t)

        summary_rows.append(
            {
                "key": key,
                "nrmse": nrmse,
                "chi2": chi2_local / len(t),
                "cap_pos": cap_pos,
                "rlow": Rlow_opt / 1e3,
            }
        )

        # Plot the first 2.2 fundamental periods for clarity
        period = 1.0 / cfg["freq"]
        idx_plot = t <= (2.2 * period)

        ax = axes[i]
        ax.plot(
            t[idx_plot] * 1000,
            Vin[idx_plot],
            "k:",
            alpha=0.4,
            lw=1.2,
            label="Vin (Input)",
        )
        ax.plot(
            t[idx_plot] * 1000,
            Vout_meas[idx_plot],
            "b.",
            ms=2.5,
            alpha=0.5,
            label="Measurement",
        )
        ax.plot(
            t[idx_plot] * 1000,
            Vout_sim[idx_plot],
            "r-",
            lw=2.0,
            label=f"Model ({cap_pos})",
        )

        d_rg_val = p_opt[f"{prefix}d_Rg"].value * 100
        d_rl_val = p_opt[f"{prefix}d_Rlow"].value * 100
        title = f"{key}\nNRMSE = {nrmse:.2f}% | R_low = {Rlow_opt/1e3:.2f} kΩ ({d_rl_val:+.1f}%)"
        if cfg["R_pinch"] > 0:
            title += f" | d_Rp = {p_opt[f'{prefix}d_Rp'].value*100:+.1f}%"
        if cfg["C"] > 0:
            title += f" | d_C = {p_opt[f'{prefix}d_C'].value*100:+.1f}%"

        ax.set_title(title, fontsize=8.5, fontweight="bold")
        ax.set_xlabel("Time (ms)", fontsize=8.5)
        ax.set_ylabel("Voltage (V)", fontsize=8.5)
        ax.grid(True, ls=":", alpha=0.6)
        if i == 0:
            ax.legend(loc="lower right", fontsize=8)

    redchi_global = total_chi2 / (total_pts_full - result.nvarys)
    plt.suptitle(
        f"Experimental Validation of the Ebers-Moll Rubber Zener Model (Global LMFIT)\n"
        f"Global Reduced Chi2 = {redchi_global:.2f} | Shared Global BJT Parameters",
        fontsize=12,
        fontweight="bold",
    )
    plt.tight_layout()

    out_png = "validation_ebers_moll_global_fit_final.png"
    plt.savefig(out_png, dpi=300)
    print(f"\n[SUCCESS] High-resolution summary figure saved: {out_png}")

    print("\n" + "=" * 80)
    print(f"{'Trace':<42} | {'NRMSE':<7} | {'Chi2/pt':<8} | {'Mode C':<6} | {'R_low (kΩ)':<10}")
    print("-" * 80)
    for row in summary_rows:
        print(
            f"{row['key']:<42} | {row['nrmse']:>5.2f}% | {row['chi2']:>8.2f} | {row['cap_pos']:<6} | {row['rlow']:>9.2f}"
        )
    print("=" * 80)

    plt.show()

if __name__ == "__main__":
    main()