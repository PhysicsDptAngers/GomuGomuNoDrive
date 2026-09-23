#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Wed Sep 23 11:24:32 2026

@author: matth
"""

#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
===============================================================================
RUBBER ZENER DIODE - TOUCH SENSITIVITY & SOFT-KNEE CLIPPING (EXPERIMENT 2)
===============================================================================
Authors: Dominique Guichaoua & Matthieu Loumaigne
Date: 2026

Simulates the steady-state AC response and computes Total Harmonic Distortion (THD)
as a function of input drive amplitude (V_in peak from 0.1 V to 7.0 V).
Demonstrates the parametric transition from hard to soft clipping via R_k.
Allows selecting between SPICE and the discrete Ebers-Moll ODE engine.
===============================================================================
"""

import warnings
warnings.filterwarnings("ignore")

import numpy as np
import matplotlib.pyplot as plt

from rubber_zener_core import (
    compute_gamma, simulate_spice_transient, simulate_ode_transient, extract_steady_state
)

# =============================================================================
# CHOIX DU MOTEUR DE SIMULATION
# =============================================================================
ENGINE = 'SPICE'     # 'SPICE' ou 'ODE'
OVS = 2              # Facteur OVS (si ENGINE='ODE')
USE_ADAA = False     # ADAA d'ordre 1 (si ENGINE='ODE')

# =============================================================================
# WRAPPER TRANSIENT & CALCUL DU THD
# =============================================================================
def run_transient(v_in_amp, freq_hz, alpha, r_k_ohm, num_periods=8.0):
    if ENGINE == 'SPICE':
        t, _, vout = simulate_spice_transient(
            v_in_peak=v_in_amp, freq_hz=freq_hz, num_periods=num_periods,
            alpha_up=alpha, alpha_dn=alpha,
            r_k_up=r_k_ohm, r_k_dn=r_k_ohm,
            r_p_up=10.0, r_p_dn=10.0,
            c_up=0.0, c_dn=0.0
        )
        return t, vout
    else:
        Fs = 96000
        total_time = num_periods / freq_hz
        t = np.linspace(0.0, total_time, int(total_time * Fs), endpoint=False)
        vin = v_in_amp * np.sin(2.0 * np.pi * freq_hz * t)
        vout = simulate_ode_transient(
            vin, Fs, alpha_up=alpha, alpha_dn=alpha,
            r_k_up=r_k_ohm, r_k_dn=r_k_ohm,
            r_p_up=10.0, r_p_dn=10.0,
            c_up=0.0, c_dn=0.0,
            ovs=OVS, use_adaa=USE_ADAA
        )
        return t, vout

def calculate_thd(t, v, freq_hz, num_periods=4):
    """Calcule le taux de distorsion harmonique (THD en %) sur les 10 premières harmoniques."""
    t_uni, v_uni = extract_steady_state(t, v, freq_hz, num_periods=num_periods, n_pts=2048)
    N = len(v_uni)
    dt = t_uni[1] - t_uni[0]
    yf = np.abs(np.fft.rfft(v_uni)) * (2.0 / N)
    xf = np.fft.rfftfreq(N, dt)

    harmonics = []
    for k in range(1, 11):
        idx = np.argmin(np.abs(xf - k * freq_hz))
        harmonics.append(yf[idx])

    f0_amp = harmonics[0]
    if f0_amp < 1e-3:
        return 0.0
    thd = np.sqrt(np.sum(np.square(harmonics[1:]))) / f0_amp * 100.0
    return thd

# =============================================================================
# EXÉCUTION DU CALCUL & PLOT
# =============================================================================
if __name__ == "__main__":
    FREQ = 1000.0
    ALPHA_TEST = 0.30

    # Balayage logarithmique de l'amplitude d'entrée (de 0,1 V à 7,0 V crête)
    v_in_sweep = np.logspace(-1.0, np.log10(7.0), 60)

    # Résistances de genou (valeurs en Ohms pour l'API)
    r_k_configs = [
        {"val_ohm": 0.0,     "label": r"0 $\Omega$ (Hard clip)", "color": "#d62728"},
        {"val_ohm": 1000.0,  "label": r"1 k$\Omega$",            "color": "#1f77b4"},
        {"val_ohm": 10000.0, "label": r"10 k$\Omega$",           "color": "#2ca02c"},
        {"val_ohm": 47000.0, "label": r"47 k$\Omega$ (Soft clip)", "color": "#17becf"}
    ]

    print(f"Running Touch Sensitivity sweep using ENGINE = '{ENGINE}' (OVS={OVS}, ADAA={USE_ADAA})...")

    # Seuil théorique statique
    gamma_dc = compute_gamma(ALPHA_TEST, C_F=0.0, freq_hz=FREQ, mode='up')
    v_th = gamma_dc * 0.65 + 0.30

    plt.rcParams.update({
        'font.size': 11, 'axes.labelsize': 12, 'axes.titlesize': 13,
        'legend.fontsize': 9.5, 'lines.linewidth': 2.0
    })

    fig, ax = plt.subplots(figsize=(7.2, 4.8))

    for cfg in r_k_configs:
        print(f" -> Sweeping R_k = {cfg['label']}...")
        thd_vals = []
        for vin in v_in_sweep:
            t, vout = run_transient(vin, FREQ, ALPHA_TEST, cfg["val_ohm"])
            thd = calculate_thd(t, vout, FREQ)
            thd_vals.append(thd)

        lbl_engine = ENGINE if ENGINE == 'SPICE' else f"ODE ({OVS}x)"
        ax.plot(v_in_sweep, thd_vals, 'o-', color=cfg["color"], markersize=4.5,
                label=rf"$R_k$ = {cfg['label']}")

    # Seuil théorique V_th vertical
    ax.axvline(v_th, color='black', linestyle='--', linewidth=1.4, alpha=0.75, zorder=1)
    ax.text(v_th * 1.05, 12.0, rf'Theoretical $V_{{th}} \approx {v_th:.1f}\,\mathrm{{V}}$',
            color='black', fontsize=9.5, rotation=90, va='bottom')

    ax.set_xscale('log')
    ax.set_xlim(0.1, 7.0)
    ax.set_ylim(-2.0, 95.0)
    ax.set_xlabel(r"Input Amplitude $V_{\mathrm{in}}$ (V peak)")
    ax.set_ylabel("Total Harmonic Distortion (THD %)")
    ax.set_title(f"Touch Sensitivity and Soft-Knee Transition vs. $R_k$ [{ENGINE}]", fontweight='bold')
    ax.grid(True, which="both", ls=':', alpha=0.6)
    ax.legend(loc='upper left', framealpha=0.95)

    plt.tight_layout()
    plt.savefig("exp2_touch_sensitivity.png", dpi=300)
    plt.savefig("exp2_touch_sensitivity.pdf")
    print("[OK] Saved 'exp2_touch_sensitivity.png' & '.pdf'")
    plt.show()