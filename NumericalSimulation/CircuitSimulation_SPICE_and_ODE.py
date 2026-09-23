#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Wed Sep 23 11:13:59 2026

@author: matth
"""

#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
===============================================================================
RUBBER ZENER DIODE - INTERACTIVE DASHBOARD: SPICE vs. EBERS-MOLL SOLVER
===============================================================================
Authors: Dominique Guichaoua & Matthieu Loumaigne
Date: 2026

Interactive Matplotlib GUI comparing raw PySpice transient/DC simulations
against the unified Ebers-Moll Newton-Raphson ODE solver (with OVS & ADAA options).
===============================================================================
"""

import numpy as np
import matplotlib.pyplot as plt
from matplotlib.widgets import Slider, RadioButtons

# Import unified simulation core
from rubber_zener_core import (
    simulate_spice_dc, simulate_spice_transient,
    simulate_ode_dc, simulate_ode_transient
)

if __name__ == "__main__":
    Fs = 96000  # Base simulation sample rate (Hz)
    
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 8.5))
    plt.subplots_adjust(bottom=0.52)

    # Panel 1: DC Transfer Curve
    line_dc_sp,  = ax1.plot([], [], 'k-',  lw=4, alpha=0.3, label='PySpice (Reference)')
    line_dc_ode, = ax1.plot([], [], 'r--', lw=1.8, label='Ebers-Moll ODE')
    ax1.set_title("DC Transfer Characteristic (I-V Curve)", fontweight='bold')
    ax1.set_xlabel(r"Output Voltage $V_{\mathrm{out}}$ (V)")
    ax1.set_ylabel(r"Input Current $I_{\mathrm{in}}$ (mA)")
    ax1.grid(True, ls=':', alpha=0.6)
    ax1.axhline(0, color='black', lw=0.8)
    ax1.axvline(0, color='black', lw=0.8)
    ax1.legend(loc='upper left')

    # Panel 2: Transient Time-Domain Response
    line_tr_in,  = ax2.plot([], [], 'k:',  alpha=0.4, label=r'Input $V_{\mathrm{in}}$')
    line_tr_sp,  = ax2.plot([], [], 'k-',  lw=4, alpha=0.3, label='PySpice Transient')
    line_tr_ode, = ax2.plot([], [], 'r--', lw=1.8, label='ODE Solver')
    ax2.set_title("Transient Response (State-Space Memory)", fontweight='bold')
    ax2.set_xlabel("Time (ms)")
    ax2.set_ylabel("Voltage (V)")
    ax2.grid(True, ls=':', alpha=0.6)
    ax2.legend(loc='upper right')

    # --- WIDGETS LAYOUT ---
    col1, col2 = 0.05, 0.48
    sl_w, h = 0.28, 0.02

    # Global Controls
    s_vin = Slider(plt.axes([0.25, 0.44, 0.35, h]), 'Input Peak (V)', 1.0, 25.0, valinit=10.0, facecolor='lightgray')

    # Column 1: POSITIVE (UP) BRANCH
    plt.figtext(0.14, 0.40, "--- UP BRANCH (Positive Half-Wave) ---", fontweight="bold", color="crimson")
    s_alpha_up = Slider(plt.axes([col1, 0.35, sl_w, h]), 'Alpha Pot', 0.0, 1.0, valinit=0.5, facecolor='crimson')
    s_Rk_up    = Slider(plt.axes([col1, 0.30, sl_w, h]), 'R_knee (Ω)', 0.0, 5000.0, valinit=10.0, facecolor='crimson')
    s_Rp_up    = Slider(plt.axes([col1, 0.25, sl_w, h]), 'R_pinch (Ω)', 0.0, 5000.0, valinit=1000.0, facecolor='crimson')
    s_C_up     = Slider(plt.axes([col1, 0.20, sl_w, h]), 'C_freq (nF)', 0.0, 500.0, valinit=100.0, facecolor='crimson')
    
    ax_radio_up = plt.axes([col1 + sl_w + 0.015, 0.20, 0.10, 0.08], facecolor='mistyrose')
    radio_cap_up = RadioButtons(ax_radio_up, ('Par. R_up', 'Par. R_low'))

    # Column 2: NEGATIVE (DOWN) BRANCH
    plt.figtext(0.57, 0.40, "--- DOWN BRANCH (Negative Half-Wave) ---", fontweight="bold", color="royalblue")
    s_alpha_dn = Slider(plt.axes([col2, 0.35, sl_w, h]), 'Alpha Pot', 0.0, 1.0, valinit=0.5, facecolor='royalblue')
    s_Rk_dn    = Slider(plt.axes([col2, 0.30, sl_w, h]), 'R_knee (Ω)', 0.0, 5000.0, valinit=10.0, facecolor='royalblue')
    s_Rp_dn    = Slider(plt.axes([col2, 0.25, sl_w, h]), 'R_pinch (Ω)', 0.0, 5000.0, valinit=1000.0, facecolor='royalblue')
    s_C_dn     = Slider(plt.axes([col2, 0.20, sl_w, h]), 'C_freq (nF)', 0.0, 500.0, valinit=100.0, facecolor='royalblue')

    ax_radio_dn = plt.axes([col2 + sl_w + 0.015, 0.20, 0.10, 0.08], facecolor='aliceblue')
    radio_cap_dn = RadioButtons(ax_radio_dn, ('Par. R_up', 'Par. R_low'))

    # Column 3: DSP EMULATION MODE
    ax_mode = plt.axes([0.88, 0.20, 0.10, 0.18], facecolor='whitesmoke')
    radio_mode = RadioButtons(ax_mode, ('OVS 1x', 'OVS 2x', 'OVS 2x + ADAA', 'OVS 4x'))

    def update(val):
        pos_up = 'up' if 'R_up' in radio_cap_up.value_selected else 'down'
        pos_dn = 'up' if 'R_up' in radio_cap_dn.value_selected else 'down'
        
        # 1. SPICE Simulation
        v_limit = max(5.0, s_vin.val * 1.15)
        _, vout_sp_dc, i_sp_dc = simulate_spice_dc(
            v_max=v_limit, step=0.05,
            alpha_up=s_alpha_up.val, r_k_up=s_Rk_up.val, r_p_up=s_Rp_up.val,
            alpha_dn=s_alpha_dn.val, r_k_dn=s_Rk_dn.val, r_p_dn=s_Rp_dn.val
        )
        
        t_sp, vin_sp_tr, vout_sp_tr = simulate_spice_transient(
            v_in_peak=s_vin.val, freq_hz=150.0, num_periods=3.0,
            alpha_up=s_alpha_up.val, r_k_up=s_Rk_up.val, r_p_up=s_Rp_up.val, c_up=s_C_up.val*1e-9, pos_up=pos_up,
            alpha_dn=s_alpha_dn.val, r_k_dn=s_Rk_dn.val, r_p_dn=s_Rp_dn.val, c_dn=s_C_dn.val*1e-9, pos_dn=pos_dn
        )
        
        # 2. ODE Solver Simulation
        vin_ode_dc = np.linspace(-v_limit, v_limit, 400)
        vout_ode_dc, i_ode_dc = simulate_ode_dc(
            vin_ode_dc,
            alpha_up=s_alpha_up.val, r_k_up=s_Rk_up.val, r_p_up=s_Rp_up.val,
            alpha_dn=s_alpha_dn.val, r_k_dn=s_Rk_dn.val, r_p_dn=s_Rp_dn.val,
            pos_up=pos_up, pos_dn=pos_dn
        )
        
        # Determine DSP configuration
        mode = radio_mode.value_selected
        if mode == 'OVS 1x':
            ovs_val, use_adaa = 1, False
        elif mode == 'OVS 2x':
            ovs_val, use_adaa = 2, False
        elif mode == 'OVS 2x + ADAA':
            ovs_val, use_adaa = 2, True
        else: # OVS 4x
            ovs_val, use_adaa = 4, False
            
        t_ode = np.linspace(0.0, 3.0 / 150.0, int((3.0 / 150.0) * Fs), endpoint=False)
        vin_ode_tr = s_vin.val * np.sin(2.0 * np.pi * 150.0 * t_ode)
        vout_ode_tr = simulate_ode_transient(
            vin_ode_tr, Fs,
            alpha_up=s_alpha_up.val, r_k_up=s_Rk_up.val, r_p_up=s_Rp_up.val, c_up=s_C_up.val*1e-9, pos_up=pos_up,
            alpha_dn=s_alpha_dn.val, r_k_dn=s_Rk_dn.val, r_p_dn=s_Rp_dn.val, c_dn=s_C_dn.val*1e-9, pos_dn=pos_dn,
            ovs=ovs_val, use_adaa=use_adaa
        )
        
        # 3. Update Plots
        line_dc_sp.set_data(vout_sp_dc, i_sp_dc)
        line_dc_ode.set_data(vout_ode_dc, i_ode_dc)
        ax1.set_xlim(np.min(vout_sp_dc) - 0.5, np.max(vout_sp_dc) + 0.5)
        ax1.set_ylim(np.min(i_sp_dc) * 1.1, np.max(i_sp_dc) * 1.1)
        
        t_sp_ms = t_sp * 1000.0
        line_tr_in.set_data(t_sp_ms, vin_sp_tr)
        line_tr_sp.set_data(t_sp_ms, vout_sp_tr)
        line_tr_ode.set_data(t_ode * 1000.0, vout_ode_tr)
        line_tr_ode.set_label(f'ODE ({mode})')
        ax2.legend(loc='upper right')
        ax2.set_xlim(0.0, (3.0 / 150.0) * 1000.0)
        ax2.set_ylim(-s_vin.val * 1.05, s_vin.val * 1.05)
        
        fig.canvas.draw_idle()

    # Bind Events
    sliders = [s_vin, s_alpha_up, s_Rk_up, s_Rp_up, s_C_up, s_alpha_dn, s_Rk_dn, s_Rp_dn, s_C_dn]
    for s in sliders:
        s.on_changed(update)
    radio_cap_up.on_clicked(update)
    radio_cap_dn.on_clicked(update)
    radio_mode.on_clicked(update)

    update(None)
    plt.show()