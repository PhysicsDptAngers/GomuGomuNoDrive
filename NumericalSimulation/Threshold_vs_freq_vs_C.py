#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
===============================================================================
RUBBER ZENER DIODE - FREQUENCY-DEPENDENT THRESHOLD & TRANSIENT DIVERGENCE
===============================================================================
Authors: Dominique Guichaoua & Matthieu Loumaigne
Date: 2026

Compares idealized AC divider predictions against large-signal transient simulation.
Generates composite figure with time-domain waveform insets at 20 Hz and 4.5 kHz.
Allows selecting between SPICE and the discrete ODE solver.
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
# SIMULATION ENGINE WRAPPER
# =============================================================================
def run_transient(v_in_amp, freq_hz, a_val, c_nF, cap_pos='up', num_periods=15):
    if ENGINE == 'SPICE':
        t, vin, vout = simulate_spice_transient(
            v_in_peak=v_in_amp, freq_hz=freq_hz, num_periods=num_periods,
            alpha_up=a_val, alpha_dn=a_val, c_up=c_nF * 1e-9, c_dn=c_nF * 1e-9,
            pos_up=cap_pos, pos_dn=cap_pos, r_p_up=0.0, r_p_dn=0.0
        )
        return t, vin, vout
    else:
        Fs = max(96000, int(freq_hz * 60))
        total_time = num_periods / freq_hz
        t = np.linspace(0.0, total_time, int(total_time * Fs), endpoint=False)
        vin = v_in_amp * np.sin(2.0 * np.pi * freq_hz * t)
        vout = simulate_ode_transient(
            vin, Fs, alpha_up=a_val, alpha_dn=a_val,
            c_up=c_nF * 1e-9, c_dn=c_nF * 1e-9,
            pos_up=cap_pos, pos_dn=cap_pos, r_p_up=0.0, r_p_dn=0.0,
            ovs=OVS, use_adaa=USE_ADAA
        )
        return t, vin, vout

# =============================================================================
# EXÉCUTION DU CALCUL & PLOT
# =============================================================================
if __name__ == "__main__":
    C_TEST_NF = 22.0
    ALPHA_TEST = 0.086
    V_IN_MAX = 7.0
    V_DIODE = 0.30

    freq_sweep = np.logspace(1.3, 4.3, 15)
    freq_theory = np.logspace(1.3, 4.3, 500)

    print(f"Running Experiment 3 using ENGINE = '{ENGINE}' (OVS={OVS}, ADAA={USE_ADAA})...")

    # Étalonnage basse fréquence à 100 Hz
    gamma_dc_cal = float(compute_gamma(ALPHA_TEST, C_F=0.0, freq_hz=100.0, mode='up'))
    t_cal, _, v_cal = run_transient(V_IN_MAX, 100.0, ALPHA_TEST, 0.0, cap_pos='up', num_periods=10)
    _, v_cal_ss = extract_steady_state(t_cal, v_cal, 100.0, num_periods=2)
    vbe_eff = (np.max(np.abs(v_cal_ss)) - V_DIODE) / gamma_dc_cal

    vth_mode1, vth_mode2 = [], []

    for f in freq_sweep:
        t1, _, v1 = run_transient(V_IN_MAX, f, ALPHA_TEST, C_TEST_NF, cap_pos='up')
        _, v1_ss = extract_steady_state(t1, v1, f, num_periods=2)
        vth_mode1.append((np.max(np.abs(v1_ss)) - V_DIODE) / vbe_eff)

        t2, _, v2 = run_transient(V_IN_MAX, f, ALPHA_TEST, C_TEST_NF, cap_pos='down')
        _, v2_ss = extract_steady_state(t2, v2, f, num_periods=2)
        vth_mode2.append((np.max(np.abs(v2_ss)) - V_DIODE) / vbe_eff)

    # Modèle analytique
    gamma_th1 = compute_gamma(ALPHA_TEST, C_F=C_TEST_NF * 1e-9, freq_hz=freq_theory, mode='up')
    gamma_th2 = compute_gamma(ALPHA_TEST, C_F=C_TEST_NF * 1e-9, freq_hz=freq_theory, mode='down')
    gamma_ceiling = (V_IN_MAX - V_DIODE) / vbe_eff
    gamma_th1 = np.minimum(gamma_th1, gamma_ceiling)
    gamma_th2 = np.minimum(gamma_th2, gamma_ceiling)

    # Tracé
    fig3, ax3 = plt.subplots(figsize=(10, 6))

    ax3.plot(freq_theory, gamma_th1, '-', color='royalblue', alpha=0.8, lw=2.2,
             label=r'Idealized AC divider, Mode 1 ($C_{\gamma} \parallel R_{up}$, $i_B = 0$)')
    ax3.plot(freq_theory, gamma_th2, '-', color='crimson', alpha=0.8, lw=2.2,
             label=r'Idealized AC divider, Mode 2 ($C_{\gamma} \parallel R_{low}$, $i_B = 0$)')

    lbl_engine = "SPICE" if ENGINE == 'SPICE' else f"ODE (OVS {OVS}x)"
    ax3.plot(freq_sweep, vth_mode1, 's', color='darkblue', markersize=6.5, label=f'{lbl_engine} (Mode 1)')
    ax3.plot(freq_sweep, vth_mode2, '^', color='darkred', markersize=7.0, label=f'{lbl_engine} (Mode 2)')

    ax3.axhline(gamma_ceiling, color='black', linestyle='--', linewidth=1, alpha=0.5)
    ax3.text(25, gamma_ceiling - 0.35, rf'Physical ceiling forced by $V_{{\mathrm{{in}}}} = {V_IN_MAX}\,\mathrm{{V}}$',
             color='black', fontsize=9.5, va='top', ha='left')

    ax3.set_xscale('log')
    ax3.set_xlabel("Frequency (Hz)", fontsize=11)
    ax3.set_ylabel(r"Effective Multiplier Factor $|\gamma|$", fontsize=11)
    ax3.set_ylim(0, gamma_ceiling * 1.25)
    ax3.legend(fontsize=9.5, loc='upper left', framealpha=0.9)
    ax3.grid(True, which="both", ls=':', alpha=0.6)

    # Insets fonction
    def plot_inset(bounds, freq, cap_pos, color, target_x, target_y, arrow_start):
        t, vin, vout = run_transient(V_IN_MAX, freq, ALPHA_TEST, C_TEST_NF, cap_pos=cap_pos)
        t_ss, vout_ss = extract_steady_state(t, vout, freq, num_periods=2)
        _, vin_ss = extract_steady_state(t, vin, freq, num_periods=2)
        t_ms = (t_ss - t_ss[0]) * 1000.0

        axins = ax3.inset_axes(bounds)
        axins.plot(t_ms, vin_ss, 'k--', alpha=0.45, lw=1.1)
        axins.plot(t_ms, vout_ss, color=color, lw=1.5)
        axins.set_xticks([]); axins.set_yticks([])
        axins.set_facecolor('#ffffff')
        for spine in axins.spines.values():
            spine.set_edgecolor('black')
            spine.set_linewidth(0.8)

        ax3.annotate('', xy=(target_x, target_y), xytext=arrow_start, textcoords='axes fraction',
                     arrowprops=dict(arrowstyle="-|>", color='dimgray', lw=1.2, mutation_scale=10))

    f_low = freq_sweep[0]
    y_low = vth_mode1[0]
    f_high = freq_sweep[-4]
    y_high1 = vth_mode1[-4]
    y_high2 = vth_mode2[-4]

    plot_inset([0.05, 0.08, 0.18, 0.18], f_low, 'up', 'darkblue', f_low, y_low, (0.14, 0.26))
    plot_inset([0.63, 0.36, 0.18, 0.18], f_high, 'up', 'darkblue', f_high, y_high1, (0.72, 0.36))
    plot_inset([0.75, 0.74, 0.18, 0.18], f_high, 'down', 'darkred', f_high, y_high2, (0.82, 0.74))

    plt.tight_layout()
    plt.savefig("exp3_spice_vs_theory_divergence_insets.png", dpi=300)
    print("[OK] Saved 'exp3_spice_vs_theory_divergence_insets.png'")
    plt.show()