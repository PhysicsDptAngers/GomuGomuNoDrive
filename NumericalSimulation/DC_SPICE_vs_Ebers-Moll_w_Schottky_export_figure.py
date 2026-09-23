#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
===============================================================================
RUBBER ZENER DIODE - DC STATIC CHARACTERIZATION & AC MAPPING
===============================================================================
Authors: Dominique Guichaoua & Matthieu Loumaigne
Date: 2026

Generates the 3-column composite figure (Mode 1, Mode 2, Mode 3) comparing 
the reference simulation against the continuous Ebers-Moll nodal model.
Allows selecting the reference simulation engine: SPICE or discrete ODE solver.
===============================================================================
"""

import warnings
warnings.filterwarnings("ignore")

import numpy as np
import matplotlib.pyplot as plt
from scipy.interpolate import interp1d
from scipy.optimize import fsolve
import lmfit

from rubber_zener_core import (
    CONSTANTS, get_pot_resistances,
    simulate_spice_dc, simulate_spice_transient,
    simulate_ode_dc, simulate_ode_transient
)

# =============================================================================
# CHOIX DU MOTEUR DE RÉFÉRENCE
# =============================================================================
ENGINE = 'SPICE'     # 'SPICE' ou 'ODE'
OVS = 2              # Facteur OVS (si ENGINE='ODE')
USE_ADAA = False     # ADAA d'ordre 1 (si ENGINE='ODE')

# =============================================================================
# MODÈLE ANALYTIQUE CONTINU EBERS-MOLL (NODAL DC)
# =============================================================================
def generate_dc_solutions(Is, n, beta_F, beta_R, Is_D, n_D, Rd, R_up, R_low, R_k, R_p):
    Vt_BJT = CONSTANTS['Vt'] * n
    Vt_Diode = CONSTANTS['Vt'] * n_D

    VN_sweep = np.linspace(0.0, 15.0, 250)
    I_tot_list, V_out_list, V_in_list = [], [], []
    last_Vb, last_Vc = 0.0, 0.0

    for VN in VN_sweep:
        if R_p < 1e-3:
            def obj_0(Vb_arr):
                Vb = Vb_arr[0]
                eb = np.exp(np.clip(Vb / Vt_BJT, -100.0, 80.0))
                ebc = np.exp(np.clip((Vb - VN) / Vt_BJT, -100.0, 80.0))
                ib = (Is / beta_F) * (eb - 1.0) + (Is / beta_R) * (ebc - 1.0)
                return [(VN - Vb) / R_up - Vb / R_low - ib]

            last_Vb = fsolve(obj_0, [last_Vb])[0]
            eb = np.exp(np.clip(last_Vb / Vt_BJT, -100.0, 80.0))
            ebc = np.exp(np.clip((last_Vb - VN) / Vt_BJT, -100.0, 80.0))
            ic = Is * (eb - ebc) - (Is / beta_R) * (ebc - 1.0)
            Itot = (VN - last_Vb) / R_up + ic
            last_Vc = VN
        else:
            def obj_Rp(vars_vec):
                Vb, Vc = vars_vec
                eb = np.exp(np.clip(Vb / Vt_BJT, -100.0, 80.0))
                ebc = np.exp(np.clip((Vb - Vc) / Vt_BJT, -100.0, 80.0))
                ib = (Is / beta_F) * (eb - 1.0) + (Is / beta_R) * (ebc - 1.0)
                ic = Is * (eb - ebc) - (Is / beta_R) * (ebc - 1.0)
                eq1 = (VN - Vb) / R_up - Vb / R_low - ib
                eq2 = (VN - Vc) / R_p - ic
                return [eq1, eq2]

            last_Vb, last_Vc = fsolve(obj_Rp, [last_Vb, last_Vc])
            Itot = (VN - last_Vb) / R_up + (VN - last_Vc) / R_p

        Itot = max(0.0, Itot)
        V_diode = n_D * Vt_Diode * np.log1p(Itot / Is_D) + Rd * Itot
        Vout = V_diode + R_k * Itot + VN
        Vin = Vout + CONSTANTS['R_in'] * Itot

        V_out_list.append(Vout)
        I_tot_list.append(Itot)
        V_in_list.append(Vin)

    return np.array(V_in_list), np.array(V_out_list), np.array(I_tot_list)

def solve_analytical_iv(V_out_target, Is, n, beta_F, beta_R, Is_D, n_D, Rd, R_up, R_low, R_k, R_p):
    _, V_out_arr, I_tot_arr = generate_dc_solutions(Is, n, beta_F, beta_R, Is_D, n_D, Rd, R_up, R_low, R_k, R_p)
    V_out_unique, unique_idx = np.unique(V_out_arr, return_index=True)
    I_tot_unique = I_tot_arr[unique_idx]
    sort_idx = np.argsort(V_out_unique)
    interp_func = interp1d(
        V_out_unique[sort_idx], I_tot_unique[sort_idx],
        kind="cubic", bounds_error=False, fill_value=(0.0, I_tot_unique[sort_idx][-1])
    )
    return interp_func(V_out_target)

def compute_analytical_transfer_curve(v_in_transient, Is, n, beta_F, beta_R, Is_D, n_D, Rd, R_up, R_low, R_k, R_p):
    V_in_arr, V_out_arr, _ = generate_dc_solutions(Is, n, beta_F, beta_R, Is_D, n_D, Rd, R_up, R_low, R_k, R_p)
    V_in_ext = np.append(V_in_arr, 100.0)
    V_out_ext = np.append(V_out_arr, V_out_arr[-1] + (100.0 - V_in_arr[-1]) * 1e-6)
    transfer_func = interp1d(V_in_ext, V_out_ext, kind="linear", bounds_error=False, fill_value="extrapolate")
    return np.sign(v_in_transient) * transfer_func(np.abs(v_in_transient))

def perform_fit(v_ref, i_ref, alpha, r_k, r_p):
    R_up, R_low = get_pot_resistances(alpha)
    def residual(params):
        i_model = solve_analytical_iv(
            v_ref, params["Is"].value, params["n"].value, params["beta_F"].value,
            params["beta_R"].value, params["Is_D"].value, params["n_D"].value,
            params["Rd"].value, R_up, R_low, r_k, r_p
        )
        return (i_ref - i_model) * 1000.0

    p = lmfit.Parameters()
    p.add("Is", value=CONSTANTS['Is'], vary=False)
    p.add("n", value=1.0, vary=False)
    p.add("beta_F", value=CONSTANTS['Bf'], vary=False)
    p.add("beta_R", value=CONSTANTS['Br'], vary=False)
    p.add("Is_D", value=CONSTANTS['IsD'], vary=False)
    p.add("n_D", value=1.0, vary=False)
    p.add("Rd", value=CONSTANTS['Rd'], min=0.0, max=50.0, vary=True)
    return lmfit.minimize(residual, p)

# =============================================================================
# EXÉCUTION DU TRACÉ
# =============================================================================
def plot_mode_column(ax_main, ax_res, ax_tr, letter, title, param_list, param_name,
                     a_vals, rk_vals, rp_vals, file_handle):
    colors = ["darkblue", "crimson", "forestgreen"]
    y_max_limit = None
    x_max_plot = 0.0

    for i, (val, alpha, rk, rp) in enumerate(zip(param_list, a_vals, rk_vals, rp_vals)):
        # 1. Acquisition DC selon moteur choisi
        if ENGINE == 'SPICE':
            v_in_raw, v_ref, i_mA = simulate_spice_dc(v_max=15.0, step=0.05, alpha_up=alpha, r_k_up=rk, r_p_up=rp)
            mask_pos = (v_in_raw >= 0.0)
            v_ref, i_ref = v_ref[mask_pos], i_mA[mask_pos] / 1000.0
        else:
            v_in_sweep = np.linspace(0.0, 15.0, 300)
            v_ref, i_mA = simulate_ode_dc(v_in_sweep, alpha_up=alpha, r_k_up=rk, r_p_up=rp)
            i_ref = i_mA / 1000.0

        mask_fit = i_ref > 1e-6
        if np.sum(mask_fit) == 0:
            continue

        fit_result = perform_fit(v_ref[mask_fit], i_ref[mask_fit], alpha, rk, rp)
        file_handle.write(f"FIT [{letter}] {title} | {param_name}={val} | Engine: {ENGINE}\n")
        file_handle.write(lmfit.fit_report(fit_result) + "\n\n")

        if i == 0:
            y_max_limit = np.max(i_ref) * 1000.0 * 1.1
            x_max_plot = np.max(v_ref)

        v_smooth = np.linspace(0.0, x_max_plot, 200)
        R_up, R_low = get_pot_resistances(alpha)
        i_mod_smooth = solve_analytical_iv(
            v_smooth, fit_result.params["Is"].value, fit_result.params["n"].value,
            fit_result.params["beta_F"].value, fit_result.params["beta_R"].value,
            fit_result.params["Is_D"].value, fit_result.params["n_D"].value,
            fit_result.params["Rd"].value, R_up, R_low, rk, rp
        )

        lbl = f"{ENGINE}: {param_name}={val}"
        ax_main.plot(v_ref, i_ref * 1000.0, color=colors[i], lw=2.2, alpha=0.6, label=lbl)
        ax_main.plot(v_smooth, i_mod_smooth * 1000.0, color=colors[i], ls="--", lw=1.4, label=f"Model: {param_name}={val}")

        # Résidu et validation AC sur la 2e courbe médiane
        if i == 1:
            i_mod_aligned = solve_analytical_iv(
                v_ref, fit_result.params["Is"].value, fit_result.params["n"].value,
                fit_result.params["beta_F"].value, fit_result.params["beta_R"].value,
                fit_result.params["Is_D"].value, fit_result.params["n_D"].value,
                fit_result.params["Rd"].value, R_up, R_low, rk, rp
            )
            ax_res.plot(v_ref, (i_ref - i_mod_aligned) * 1000.0, color=colors[i], lw=1.4, label=f"Residual ({val})")

            # Validation AC
            if ENGINE == 'SPICE':
                t_tr, v_in_tr, v_out_tr = simulate_spice_transient(
                    v_in_peak=10.0, freq_hz=1000.0, num_periods=5.0,
                    alpha_up=alpha, r_k_up=rk, r_p_up=rp
                )
                t_mask = t_tr >= (2.0 / 1000.0)
                t_tr, v_in_tr, v_out_tr = t_tr[t_mask] - t_tr[t_mask][0], v_in_tr[t_mask], v_out_tr[t_mask]
            else:
                Fs_ac = 96000
                t_tr = np.linspace(0.0, 3.0 / 1000.0, int((3.0 / 1000.0) * Fs_ac), endpoint=False)
                v_in_tr = 10.0 * np.sin(2.0 * np.pi * 1000.0 * t_tr)
                v_out_tr = simulate_ode_transient(
                    v_in_tr, Fs_ac, alpha_up=alpha, r_k_up=rk, r_p_up=rp,
                    ovs=OVS, use_adaa=USE_ADAA
                )

            v_out_analytical = compute_analytical_transfer_curve(
                v_in_tr, fit_result.params["Is"].value, fit_result.params["n"].value,
                fit_result.params["beta_F"].value, fit_result.params["beta_R"].value,
                fit_result.params["Is_D"].value, fit_result.params["n_D"].value,
                fit_result.params["Rd"].value, R_up, R_low, rk, rp
            )

            ax_tr.plot(t_tr * 1000.0, v_in_tr, "k:", alpha=0.6, label=r"$V_{\mathrm{in}}$")
            ax_tr.plot(t_tr * 1000.0, v_out_tr, color=colors[i], lw=2.4, alpha=0.5, label=f"{ENGINE} ($V_{{\mathrm{{out}}}}$)")
            ax_tr.plot(t_tr * 1000.0, v_out_analytical, color=colors[i], ls="--", lw=1.4, label="Mapped Model")

            ax_tr.set_title(f"AC Validation (({letter}) {title} | {param_name}={val})", fontsize=7.5)
            ax_tr.set_xlabel("Time (ms)", fontsize=7.5)
            ax_tr.set_ylabel("Amplitude (V)", fontsize=7.5)
            ax_tr.legend(fontsize=6.5, loc="upper right", framealpha=0.9)
            ax_tr.grid(True, ls=":", alpha=0.5)
            ax_tr.set_xlim(0.0, 2.0)
            ax_tr.set_ylim(-11.0, 11.0)

    ax_main.set_title(f"({letter}) {title}", fontsize=8.5, fontweight="bold", pad=5)
    ax_main.set_ylabel(r"Current $I_{\mathrm{in}}$ (mA)", fontsize=7.5)
    ax_main.legend(fontsize=6.5, loc="upper left", framealpha=0.9)
    ax_main.grid(True, ls=":", alpha=0.5)
    ax_main.set_xlim(0.0, x_max_plot)
    if y_max_limit:
        ax_main.set_ylim(0.0, y_max_limit)

    ax_res.set_ylabel("Error (mA)", fontsize=7.5)
    ax_res.set_xlabel(r"Output Voltage $V_{\mathrm{out}}$ (V)", fontsize=7.5)
    ax_res.axhline(0, color="black", lw=0.8, ls="--")
    ax_res.legend(fontsize=6.5, loc="lower right", framealpha=0.9)
    ax_res.grid(True, ls=":", alpha=0.5)

if __name__ == "__main__":
    print(f"Executing DC Static Characterization using ENGINE = '{ENGINE}' (OVS={OVS}, ADAA={USE_ADAA})...")

    modes_configs = [
        {"letter": "A", "title": r"Mode 1: Threshold Scaling ($\gamma$ translation)",
         "param_name": "Pot", "param_list": ["Min", "Mid", "Max"],
         "a_vals": [0.05, 0.3, 0.8], "rk_vals": [0.0, 0.0, 0.0], "rp_vals": [0.0, 0.0, 0.0]},
        {"letter": "B", "title": r"Mode 2: Soft-Knee Transition ($R_{k}$ compression)",
         "param_name": r"$R_k$", "param_list": [r"0 $\Omega$", r"1 k$\Omega$", r"10 k$\Omega$"],
         "a_vals": [0.3, 0.3, 0.3], "rk_vals": [0.0, 1000.0, 10000.0], "rp_vals": [0.0, 0.0, 0.0]},
        {"letter": "C", "title": r"Mode 3: Hard Saturation Ceiling ($R_{p}$ pinch-off)",
         "param_name": r"$R_p$", "param_list": [r"5 k$\Omega$", r"10 k$\Omega$", r"15 k$\Omega$"],
         "a_vals": [0.25, 0.25, 0.25], "rk_vals": [0.0, 0.0, 0.0], "rp_vals": [5000.0, 10000.0, 15000.0]},
    ]

    fig = plt.figure(figsize=(15.5, 8.0))
    outer_grid = fig.add_gridspec(2, 3, height_ratios=[4.0, 2.3], hspace=0.34, wspace=0.24,
                                   left=0.06, right=0.98, top=0.95, bottom=0.07)

    with open("fit_results.txt", "w") as f_out:
        for col, m in enumerate(modes_configs):
            gs_upper = outer_grid[0, col].subgridspec(2, 1, height_ratios=[3.0, 1.0], hspace=0.08)
            ax_main = fig.add_subplot(gs_upper[0])
            ax_res = fig.add_subplot(gs_upper[1], sharex=ax_main)
            ax_tr = fig.add_subplot(outer_grid[1, col])
            ax_main.tick_params(labelbottom=False)

            plot_mode_column(ax_main, ax_res, ax_tr, m["letter"], m["title"],
                             m["param_list"], m["param_name"], m["a_vals"],
                             m["rk_vals"], m["rp_vals"], f_out)

    fig.savefig("rubber_zener_dc_ac_composite.png", dpi=300)
    fig.savefig("rubber_zener_dc_ac_composite.pdf", bbox_inches="tight")
    print("[OK] Saved 'rubber_zener_dc_ac_composite.png' & '.pdf'")
    plt.show()