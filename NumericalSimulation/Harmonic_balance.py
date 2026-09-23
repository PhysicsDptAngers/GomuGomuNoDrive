#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Wed Sep 23 11:20:54 2026

@author: matth
"""

#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
===============================================================================
RUBBER ZENER DIODE - HARMONIC BALANCE & EVEN/ODD CONTROL
===============================================================================
Authors: Dominique Guichaoua & Matthieu Loumaigne
Date: 2026

Simulates steady-state harmonic generation under symmetric and asymmetric clipping.
Generates: Time domain (a), FFT spectra (b), and 2D contour map inset of H2/H3.
Allows selecting between SPICE and the discrete-time ODE engine.
===============================================================================
"""

import warnings
warnings.filterwarnings("ignore")

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.patheffects as pe
from matplotlib.ticker import FormatStrFormatter

from rubber_zener_core import (
    simulate_spice_transient, simulate_ode_transient,
    extract_steady_state, compute_leak_free_fft
)

# =============================================================================
# CHOIX DU MOTEUR DE SIMULATION
# =============================================================================
ENGINE = 'SPICE'     # 'SPICE' ou 'ODE'
OVS = 2              # Facteur OVS (si ENGINE='ODE')
USE_ADAA = False     # ADAA d'ordre 1 (si ENGINE='ODE')

# =============================================================================
# WRAPPER TRANSIENT UNIFIÉ
# =============================================================================
def run_simulation(v_in_amp, freq_hz, a_up, a_dn, num_periods=15):
    if ENGINE == 'SPICE':
        t, vin, vout = simulate_spice_transient(
            v_in_peak=v_in_amp, freq_hz=freq_hz, num_periods=num_periods,
            alpha_up=a_up, alpha_dn=a_dn, r_p_up=10.0, r_p_dn=10.0
        )
        return t, vin, vout
    else:
        Fs = 96000
        total_time = num_periods / freq_hz
        t = np.linspace(0.0, total_time, int(total_time * Fs), endpoint=False)
        vin = v_in_amp * np.sin(2.0 * np.pi * freq_hz * t)
        vout = simulate_ode_transient(
            vin, Fs, alpha_up=a_up, alpha_dn=a_dn, r_p_up=10.0, r_p_dn=10.0,
            ovs=OVS, use_adaa=USE_ADAA
        )
        return t, vin, vout

def get_h2_h3_ratio(t, vout, freq_hz, num_periods=4):
    t_uni, v_uni = extract_steady_state(t, vout, freq_hz, num_periods=num_periods, n_pts=2048)
    yf = np.abs(np.fft.rfft(v_uni)) * (2.0 / len(v_uni))
    h2 = yf[2 * num_periods]
    h3 = yf[3 * num_periods]
    ratio_dB = 20.0 * np.log10(max(h2, 1e-6) / max(h3, 1e-6))
    return np.clip(ratio_dB, -60.0, 4.0)

# =============================================================================
# EXÉCUTION DU CALCUL & PLOT
# =============================================================================
if __name__ == "__main__":
    FREQ = 1000.0
    VIN_AMP = 7.0
    N_DISP = 2
    N_FFT = 5

    alpha_sym = (0.25, 0.25)
    alpha_asym = (0.35, 0.0)

    print(f"Running Harmonic Balance using ENGINE = '{ENGINE}' (OVS={OVS}, ADAA={USE_ADAA})...")
    
    t_s, vin_s, vout_s = run_simulation(VIN_AMP, FREQ, alpha_sym[0], alpha_sym[1], num_periods=16)
    t_a, vin_a, vout_a = run_simulation(VIN_AMP, FREQ, alpha_asym[0], alpha_asym[1], num_periods=16)

    # Extraction temporelle
    t_uni_s, v_uni_s = extract_steady_state(t_s, vout_s, FREQ, num_periods=N_DISP, n_pts=2048)
    t_uni_a, v_uni_a = extract_steady_state(t_a, vout_a, FREQ, num_periods=N_DISP, n_pts=2048)
    _, vin_uni = extract_steady_state(t_s, vin_s, FREQ, num_periods=N_DISP, n_pts=2048)

    # Extraction spectrale
    t_fft_s, v_fft_s = extract_steady_state(t_s, vout_s, FREQ, num_periods=N_FFT, n_pts=4096)
    t_fft_a, v_fft_a = extract_steady_state(t_a, vout_a, FREQ, num_periods=N_FFT, n_pts=4096)
    xf_s, mag_s = compute_leak_free_fft(t_fft_s, v_fft_s, FREQ)
    xf_a, mag_a = compute_leak_free_fft(t_fft_a, v_fft_a, FREQ)

    # Cartographie 2D
    print("Computing 2D H2/H3 parameter space map...")
    alpha_min, alpha_max = 0.10, 0.40
    N_GRID = 13
    alphas = np.linspace(alpha_min, alpha_max, N_GRID)
    A_UP, A_DN = np.meshgrid(alphas, alphas)
    H2_H3_grid = np.zeros_like(A_UP)

    for i in range(N_GRID):
        for j in range(N_GRID):
            t_cell, _, v_cell = run_simulation(VIN_AMP, FREQ, A_UP[i, j], A_DN[i, j], num_periods=8)
            H2_H3_grid[i, j] = get_h2_h3_ratio(t_cell, v_cell, FREQ, num_periods=4)

    # Tracé
    plt.rcParams.update({'font.size': 11, 'axes.labelsize': 12, 'axes.titlesize': 13,
                         'legend.fontsize': 10.5, 'lines.linewidth': 2.0})

    fig, (ax_time, ax_fft) = plt.subplots(2, 1, figsize=(8.0, 7.2), gridspec_kw={'height_ratios': [1, 1.35]})
    plt.subplots_adjust(left=0.10, right=0.96, top=0.95, bottom=0.08, hspace=0.36)

    # Panel (a)
    t_ms = (t_uni_s - t_uni_s[0]) * 1000.0
    ax_time.plot(t_ms, vin_uni, color='gray', ls=':', lw=1.6, alpha=0.8, label=r'$V_{\mathrm{in}}$')
    ax_time.plot(t_ms, v_uni_s, color='black', lw=2.4, label='Symmetric')
    ax_time.plot(t_ms, v_uni_a, color='#C8102E', ls='--', lw=2.4, label='Asymmetric')
    ax_time.set_xlim(0.0, (N_DISP / FREQ) * 1000.0)
    ax_time.set_ylim(-VIN_AMP * 1.15, VIN_AMP * 1.15)
    ax_time.set_xlabel('Time (ms)')
    ax_time.set_ylabel(r'$V_{\mathrm{out}}$ (V)')
    ax_time.grid(True, ls=':', alpha=0.7)
    ax_time.legend(loc='upper right', framealpha=0.95)
    ax_time.set_title(f'(a) Steady-state waveforms ({FREQ/1000:.1f} kHz) [{ENGINE}]', loc='left', fontweight='bold')

    # Panel (b)
    ax_fft.fill_between(xf_s / 1000.0, -80.0, mag_s, color='black', alpha=0.10)
    h_idx_s = [np.argmin(np.abs(xf_s - k * FREQ)) for k in range(1, 8)]
    ax_fft.plot(xf_s / 1000.0, mag_s, color='black', lw=2.4, ls='-', label='Symmetric')
    ax_fft.plot(range(1, 8), mag_s[h_idx_s], 'o', color='black', markersize=6.5)

    h_idx_a = [np.argmin(np.abs(xf_a - k * FREQ)) for k in range(1, 8)]
    ax_fft.plot(xf_a / 1000.0, mag_a, color='#C8102E', lw=2.2, ls='--', label='Asymmetric')
    ax_fft.plot(range(1, 8), mag_a[h_idx_a], 's', color='#C8102E', markersize=6.0)

    ax_fft.set_xlim(0.0, 7.0)
    ax_fft.set_ylim(-85.0, 10.0)
    ax_fft.set_xlabel('Frequency (kHz)')
    ax_fft.set_ylabel('Magnitude (dB)')
    ax_fft.grid(True, ls=':', alpha=0.7)
    ax_fft.legend(loc='lower left', framealpha=0.95)
    ax_fft.set_title('(b) Harmonic spectra and even/odd harmonic balance', loc='left', fontweight='bold')

    h2_val = mag_a[h_idx_a[1]]
    ax_fft.annotate(fr'$H_2$ (${h2_val:.1f}\,\mathrm{{dB}}$)', xy=(2.0, h2_val), xytext=(2.15, -18),
                    arrowprops=dict(arrowstyle='->', lw=1.8, color='#C8102E'),
                    fontsize=10.5, color='#C8102E', fontweight='bold')
    ax_fft.annotate(r'No $H_2$ ($<-80\,\mathrm{dB}$)', xy=(2.0, -78.0), xytext=(2.15, -60),
                    arrowprops=dict(arrowstyle='->', lw=1.8, color='black'), fontsize=10.5)

    # Inset
    halo = [pe.withStroke(linewidth=3.0, foreground='white')]
    ax_ins = ax_fft.inset_axes([0.58, 0.38, 0.38, 0.58])
    ax_ins.patch.set_facecolor('white')
    cp = ax_ins.contourf(A_UP, A_DN, H2_H3_grid, levels=np.linspace(-60, 0, 13), cmap='RdYlBu_r', extend='both')
    ax_ins.plot([alpha_min, alpha_max], [alpha_min, alpha_max], 'k--', lw=1.5)
    ax_ins.plot(alpha_sym[0], alpha_sym[1], 'o', color='black', markersize=6.5, markeredgecolor='white')
    ax_ins.plot(alpha_asym[0], alpha_asym[1], 's', color='#C8102E', markersize=6.5, markeredgecolor='white')
    ax_ins.set_xlim(alpha_min, alpha_max)
    ax_ins.set_ylim(alpha_min, alpha_max)
    ax_ins.tick_params(direction='in', which='both', labelsize=9, pad=-16)
    for lbl in ax_ins.get_xticklabels() + ax_ins.get_yticklabels():
        lbl.set_path_effects(halo)
        lbl.set_fontweight('bold')

    txt_x = ax_ins.set_xlabel(r'$\alpha_{\mathrm{UP}}$', fontsize=13, labelpad=2)
    txt_y = ax_ins.set_ylabel(r'$\alpha_{\mathrm{DN}}$', fontsize=13, labelpad=2)
    txt_t = ax_ins.set_title(r'$H_2 / H_3\ \mathrm{(dB)}$', fontsize=12, pad=4, fontweight='bold')
    txt_x.set_path_effects(halo); txt_y.set_path_effects(halo); txt_t.set_path_effects(halo)

    cax = ax_ins.inset_axes([1.04, 0.0, 0.08, 1.0])
    cbar = fig.colorbar(cp, cax=cax)
    cbar.ax.tick_params(labelsize=8.5)
    for t_lbl in cbar.ax.get_yticklabels():
        t_lbl.set_path_effects(halo)

    plt.savefig('exp1_harmonic_balance_composite.png', dpi=300)
    plt.savefig('exp1_harmonic_balance_composite.pdf')
    print("[OK] Saved 'exp1_harmonic_balance_composite.png' & '.pdf'")
    plt.show()