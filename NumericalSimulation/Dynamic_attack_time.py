

#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
===============================================================================
RUBBER ZENER DIODE - TRANSIENT SAG & DYNAMIC ATTACK TIME (COMPOSITE)
===============================================================================
Authors: Dominique Guichaoua & Matthieu Loumaigne
Date: 2026

Simulates tone burst attack and cycle-averaged DC offset drift (<V_out>).
Evaluates settling time constant tau across the guitar register (50 Hz to 5 kHz).
Allows toggling between SPICE and the discrete Ebers-Moll ODE engine.
===============================================================================
"""

import warnings
warnings.filterwarnings("ignore")

import numpy as np
import matplotlib.pyplot as plt
from scipy.interpolate import interp1d

from rubber_zener_core import (
    simulate_spice_transient, simulate_ode_transient
)

# =============================================================================
# CHOIX DU MOTEUR DE SIMULATION
# =============================================================================
ENGINE = 'SPICE'     # 'SPICE' ou 'ODE'
OVS = 2              # Facteur OVS (si ENGINE='ODE')
USE_ADAA = False     # ADAA d'ordre 1 (si ENGINE='ODE')

# =============================================================================
# WRAPPER SIMULATION SALVE TRANSIENT
# =============================================================================
def simulate_burst(freq_hz, c_val_nF, a_up=0.2, a_dn=0.8, v_in=5.0, duration=0.06):
    num_periods = max(duration * freq_hz, 25.0)
    if ENGINE == 'SPICE':
        t, _, vout = simulate_spice_transient(
            v_in_peak=v_in, freq_hz=freq_hz, num_periods=num_periods,
            alpha_up=a_up, alpha_dn=a_dn, c_up=c_val_nF * 1e-9, c_dn=c_val_nF * 1e-9,
            pos_up='up', pos_dn='up', r_p_up=10.0, r_p_dn=10.0
        )
        return t, vout
    else:
        Fs = 96000
        total_time = num_periods / freq_hz
        t = np.linspace(0.0, total_time, int(total_time * Fs), endpoint=False)
        vin = v_in * np.sin(2.0 * np.pi * freq_hz * t)
        vout = simulate_ode_transient(
            vin, Fs, alpha_up=a_up, alpha_dn=a_dn,
            c_up=c_val_nF * 1e-9, c_dn=c_val_nF * 1e-9,
            pos_up='up', pos_dn='up', r_p_up=10.0, r_p_dn=10.0,
            ovs=OVS, use_adaa=USE_ADAA
        )
        return t, vout

def extract_running_dc(t_raw, v_raw, freq_hz, max_time_s=0.04):
    dt = 1e-6
    t_uni = np.arange(0.0, min(max_time_s, t_raw[-1]), dt)
    v_uni = interp1d(t_raw, v_raw, kind='cubic')(t_uni)

    pts_per_cycle = int((1.0 / freq_hz) / dt)
    num_cycles = len(t_uni) // pts_per_cycle

    v_reshaped = v_uni[:num_cycles * pts_per_cycle].reshape(num_cycles, pts_per_cycle)
    v_dc = np.mean(v_reshaped, axis=1)
    t_dc_ms = (np.arange(num_cycles) + 0.5) * (1.0 / freq_hz) * 1000.0
    return t_uni * 1000.0, v_uni, t_dc_ms, v_dc

def extract_tau(t_raw, v_raw, freq_hz):
    dt = min(1e-6, (1.0 / freq_hz) / 200.0)
    t_uni = np.arange(0.0, t_raw[-1], dt)
    v_uni = interp1d(t_raw, v_raw, kind='cubic')(t_uni)

    pts_per_cycle = int((1.0 / freq_hz) / dt)
    num_cycles = len(t_uni) // pts_per_cycle
    if num_cycles < 5:
        return 0.0

    v_reshaped = v_uni[:num_cycles * pts_per_cycle].reshape(num_cycles, pts_per_cycle)
    v_dc = np.mean(v_reshaped, axis=1)
    t_dc_ms = (np.arange(num_cycles) + 0.5) * (1.0 / freq_hz) * 1000.0

    v_final = np.mean(v_dc[-max(1, int(num_cycles * 0.2)):])
    v_target = 0.632 * v_final

    for i in range(len(v_dc)):
        if v_dc[i] >= v_target:
            if i == 0:
                return v_target / (v_dc[0] / t_dc_ms[0])
            slope = (v_dc[i] - v_dc[i - 1]) / (t_dc_ms[i] - t_dc_ms[i - 1])
            return t_dc_ms[i - 1] + (v_target - v_dc[i - 1]) / slope

    return t_dc_ms[-1]

# =============================================================================
# EXÉCUTION DU CALCUL & PLOT
# =============================================================================
if __name__ == "__main__":
    FREQ_BURST = 1000.0
    C_MAIN = 100.0
    T_WINDOW_MS = 40.0

    print(f"Simulating attack envelope using ENGINE = '{ENGINE}' (OVS={OVS}, ADAA={USE_ADAA})...")
    t_raw, v_raw = simulate_burst(FREQ_BURST, C_MAIN, duration=0.06)
    t_wave_ms, v_wave, t_dc_ms, v_dc = extract_running_dc(t_raw, v_raw, FREQ_BURST, max_time_s=T_WINDOW_MS / 1000.0)

    print("Sweeping settling time constant tau across frequency...")
    freqs_sweep = np.logspace(np.log10(50.0), np.log10(5000.0), 12)
    capacitors = [22.0, 100.0]
    tau_results = {c: [] for c in capacitors}

    for c in capacitors:
        print(f" -> C = {c:.0f} nF...")
        for f in freqs_sweep:
            t_f, v_f = simulate_burst(f, c, duration=0.10)
            tau_results[c].append(extract_tau(t_f, v_f, f))

    # Tracé
    plt.rcParams.update({'font.size': 11, 'axes.labelsize': 13, 'axes.titlesize': 13,
                         'legend.fontsize': 10.5, 'lines.linewidth': 2.0})

    fig, ax = plt.subplots(figsize=(8.5, 5.2))
    plt.subplots_adjust(left=0.10, right=0.96, top=0.92, bottom=0.12)

    ax.plot(t_wave_ms, v_wave, color='#2b5c8f', lw=1.2, alpha=0.55, label=r'Output Waveform $V_{\mathrm{out}}(t)$')
    ax.plot(t_dc_ms, v_dc, color='#d95f02', lw=2.6, label=r'Running DC Baseline $\langle V_{\mathrm{out}} \rangle_T$')

    ax.set_xlim(0.0, T_WINDOW_MS)
    ax.set_ylim(-1.5, 3.4)
    ax.set_xticks(np.linspace(0.0, T_WINDOW_MS, 9))
    ax.set_yticks([-1.0, 0.0, 1.0, 2.0, 3.0])
    ax.set_xlabel('Time (ms)')
    ax.set_ylabel(r'$V_{\mathrm{out}}$ (V)')
    ax.grid(True, ls=':', alpha=0.6)
    ax.legend(loc='upper left', framealpha=0.95)

    # Inset
    ax_ins = ax.inset_axes([0.50, 0.52, 0.46, 0.42])
    ax_ins.patch.set_facecolor('#ffffff')
    ax_ins.plot(freqs_sweep, tau_results[22.0], 'o-', color='crimson', lw=1.8, markersize=5.5, label=r'$C_{\gamma} = 22\,\mathrm{nF}$')
    ax_ins.plot(freqs_sweep, tau_results[100.0], 's-', color='darkblue', lw=1.8, markersize=5.5, label=r'$C_{\gamma} = 100\,\mathrm{nF}$')
    ax_ins.set_xscale('log')
    ax_ins.set_xlim(45, 5500)
    ax_ins.set_ylim(0, max(tau_results[100.0]) * 1.25)
    ax_ins.set_xlabel('Frequency (Hz)', fontsize=10, labelpad=2)
    ax_ins.set_ylabel(r'Attack Time $\tau$ (ms)', fontsize=10, labelpad=2)
    ax_ins.set_title(r'Sag Time Constant $\tau(f)$', fontsize=10.5, pad=4, fontweight='bold')
    ax_ins.tick_params(labelsize=9, pad=3)
    ax_ins.grid(True, which="both", ls=':', alpha=0.6)
    ax_ins.legend(fontsize=8.5, loc='upper right', framealpha=0.9)

    plt.savefig('exp4_transient_sag_composite.png', dpi=300)
    plt.savefig('exp4_transient_sag_composite.pdf')
    print("[OK] Saved 'exp4_transient_sag_composite.png' & '.pdf'")
    plt.show()