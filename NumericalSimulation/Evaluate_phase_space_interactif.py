#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
===============================================================================
RUBBER ZENER DIODE - PHASE SPACE EXPLORATION & RESIDUAL AUTOPSY
===============================================================================
Authors: Dominique Guichaoua & Matthieu Loumaigne
Date: 2026

Analyzes Monte Carlo batch results (monte_carlo_results.txt):
  1. Identifies the primary physical parameters driving prediction errors 
     via DecisionTreeRegressor feature importance.
  2. Generates an interactive Seaborn pairplot of the critical phase space.
  3. Clicking on any operating point triggers an on-the-fly transient autopsy 
     comparing SPICE and the Ebers-Moll ODE solver.
===============================================================================
"""

import warnings
warnings.filterwarnings("ignore")

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
from sklearn.tree import DecisionTreeRegressor

# Import du jumeau numérique unifié
from rubber_zener_core import (
    simulate_spice_transient,
    simulate_ode_transient,
    CONSTANTS
)

# =============================================================================
# 1. CONFIGURATION DU MOTEUR DE RECALCUL INTERACTIF
# =============================================================================
FILE_NAME = "monte_carlo_results.txt"
ERROR_THRESHOLD = 2.0           # Seuil d'alerte en % NRMSE

# Paramètres de simulation pour l'autopsie interactive
FS_INTERNAL = 48000 * 2         # Fréquence nominale (96 kHz)
SIM_TIME = 0.01                 # Durée du transitoire (10 ms)
OVS_FACTOR = 2                  # Facteur OVS (1, 2, 4, 8)
USE_ADAA = False                # Active/désactive l'ADAA d'ordre 1
EXACT_CROSS_DISCHARGE = False   # False : solveur 4D temps réel | True : solveur 8D complet
LINE_SEARCH = True              # Backtracking Line Search actif pour les cas limites

# =============================================================================
# 2. CHARGEMENT ET PRÉPARATION DES DONNÉES
# =============================================================================
print(f"Chargement des résultats depuis '{FILE_NAME}'...")
try:
    df = pd.read_csv(FILE_NAME, sep='\t')
except FileNotFoundError:
    print(f"Erreur : fichier '{FILE_NAME}' introuvable. Lance mc_validation en premier.")
    exit(1)

# Encodage binaire des positions de condensateur
df['PosU_bin'] = df['PosU'].map({'up': 1, 'down': 0})
df['PosD_bin'] = df['PosD'].map({'up': 1, 'down': 0})

features = [
    'AlphaU', 'RkU(kOhm)', 'RpU(Ohm)', 'Cu(nF)', 'PosU_bin',
    'AlphaD', 'RkD(kOhm)', 'RpD(Ohm)', 'Cd(nF)', 'PosD_bin',
    'VinPeak(V)', 'Freq(Hz)'
]

# Filtrage des lignes valides
df = df.dropna(subset=features + ['NRMSE(%)'])
X = df[features]
y = df['NRMSE(%)']

# =============================================================================
# 3. ANALYSE STATISTIQUE PAR ARBRE DE DÉCISION
# =============================================================================
print("\n--- ANALYSE DE SENSIBILITÉ (ARBRE DE DÉCISION) ---")
tree = DecisionTreeRegressor(max_depth=3, random_state=42)
tree.fit(X, y)

importances = pd.Series(tree.feature_importances_, index=features).sort_values(ascending=False)
print("\nTop 3 des facteurs influençant l'erreur (Feature Importance) :")
print(importances.head(3).to_string())

top_features = importances.head(3).index.tolist()

# =============================================================================
# 4. MOTEUR DE RECALCUL INTERACTIF (À LA DEMANDE)
# =============================================================================
def recalculate_and_plot(row):
    alpha_up = row['AlphaU']
    R_k_up   = row['RkU(kOhm)'] * 1e3   # Conversion kOhm -> Ohm
    R_p_up   = row['RpU(Ohm)']
    C_up     = row['Cu(nF)'] * 1e-9     # Conversion nF -> F
    pos_up   = row['PosU']

    alpha_dn = row['AlphaD']
    R_k_dn   = row['RkD(kOhm)'] * 1e3   # Conversion kOhm -> Ohm
    R_p_dn   = row['RpD(Ohm)']
    C_dn     = row['Cd(nF)'] * 1e-9     # Conversion nF -> F
    pos_dn   = row['PosD']

    v_in_peak = row['VinPeak(V)']
    freq      = row['Freq(Hz)']

    print(f"\n[Simulation] Vin = {v_in_peak:.2f} V | Freq = {freq:.0f} Hz | OVS = {OVS_FACTOR}x | 8D = {EXACT_CROSS_DISCHARGE}...")

    # 1. Simulation SPICE de référence
    num_periods_sim = (SIM_TIME + 2.0 / FS_INTERNAL) * freq
    t_sp, _, v_sp = simulate_spice_transient(
        v_in_peak=v_in_peak, freq_hz=freq, num_periods=num_periods_sim,
        alpha_up=alpha_up, r_k_up=R_k_up, r_p_up=R_p_up, c_up=C_up, pos_up=pos_up,
        alpha_dn=alpha_dn, r_k_dn=R_k_dn, r_p_dn=R_p_dn, c_dn=C_dn, pos_dn=pos_dn
    )
    t_sp_ms = t_sp * 1000.0

    # 2. Simulation du jumeau numérique ODE
    n_samples = int(SIM_TIME * FS_INTERNAL)
    t_ode_s = np.linspace(0.0, SIM_TIME, n_samples, endpoint=False)
    t_ode_ms = t_ode_s * 1000.0
    Vin_ode = v_in_peak * np.sin(2.0 * np.pi * freq * t_ode_s)

    v_ode = simulate_ode_transient(
        Vin_ode, FS_INTERNAL,
        alpha_up=alpha_up, r_k_up=R_k_up, r_p_up=R_p_up, c_up=C_up, pos_up=pos_up,
        alpha_dn=alpha_dn, r_k_dn=R_k_dn, r_p_dn=R_p_dn, c_dn=C_dn, pos_dn=pos_dn,
        ovs=OVS_FACTOR,
        use_adaa=USE_ADAA,
        exact_cross_discharge=EXACT_CROSS_DISCHARGE,
        line_search=LINE_SEARCH
    )

    # 3. Calcul de l'écart instantané
    v_sp_interp = np.interp(t_ode_s, t_sp, v_sp)
    residual = v_sp_interp - v_ode

    # Recalcul direct du NRMSE sur la fenêtre affichée
    crop_idx = int(0.001 * FS_INTERNAL)  # ignore 1 ms de marge
    rmse = np.sqrt(np.mean(residual[crop_idx:]**2))
    std_sp = np.std(v_sp_interp[crop_idx:])
    nrmse_live = (rmse / std_sp) * 100.0 if std_sp > 1e-6 else 0.0

    # 4. Tracé comparatif détaillé
    fig_sim, (ax_sig, ax_res) = plt.subplots(
        2, 1, figsize=(10, 7.5), gridspec_kw={'height_ratios': [2.8, 1.2]}
    )
    fig_sim.canvas.manager.set_window_title(f"Autopsie : Vin={v_in_peak:.1f}V, {freq:.0f}Hz")

    ax_sig.plot(t_sp_ms, v_sp, 'k-', lw=3, alpha=0.35, label='SPICE (Référence)')
    lbl_ode = f"ODE ({OVS_FACTOR}x, {'8D Exact' if EXACT_CROSS_DISCHARGE else '4D Découplé'})"
    ax_sig.plot(t_ode_ms, v_ode, 'r--', lw=1.6, label=lbl_ode)

    ax_sig.set_title(
        f"NRMSE fichier = {row['NRMSE(%)']:.2f}% | NRMSE recalculé = {nrmse_live:.2f}%\n"
        f"Paramètres : Freq={freq:.0f} Hz | Vin={v_in_peak:.1f} V | "
        f"Cu={row['Cu(nF)']:.1f} nF ({row['PosU']}) | Cd={row['Cd(nF)']:.1f} nF ({row['PosD']})",
        fontsize=10.5, fontweight='bold'
    )
    ax_sig.set_ylabel("Tension (V)")
    ax_sig.set_xlim(0.0, SIM_TIME * 1000.0)
    ax_sig.grid(True, ls=':', alpha=0.6)
    ax_sig.legend(loc='upper right', framealpha=0.9)

    ax_res.plot(t_ode_ms, residual * 1000.0, color='royalblue', lw=1.2, label=r'Résidu $V_{\mathrm{SPICE}} - V_{\mathrm{ODE}}$')
    ax_res.axhline(0, color='black', lw=0.8, ls='--')
    ax_res.set_xlabel("Temps (ms)")
    ax_res.set_ylabel("Erreur (mV)")
    ax_res.set_xlim(0.0, SIM_TIME * 1000.0)
    ax_res.grid(True, ls=':', alpha=0.6)
    ax_res.legend(loc='lower right', framealpha=0.9)

    plt.tight_layout()
    plt.show(block=False)

def on_click(event):
    """Interception des clics sur la matrice de nuages de points."""
    if event.inaxes is None or event.button != 1:
        return

    ax = event.inaxes
    x_col = ax.get_xlabel()
    y_col = ax.get_ylabel()

    if x_col not in df.columns or y_col not in df.columns or x_col == y_col:
        return

    # Normalisation pour distance euclidienne non biaisée
    x_data = df[x_col]
    y_data = df[y_col]
    dx = (x_data - event.xdata) / (x_data.max() - x_data.min() + 1e-9)
    dy = (y_data - event.ydata) / (y_data.max() - y_data.min() + 1e-9)

    best_idx = (dx**2 + dy**2).idxmin()
    row = df.loc[best_idx]

    print(f"\n[+] Point ciblé : Index {best_idx} | NRMSE initial = {row['NRMSE(%)']:.2f}%")
    recalculate_and_plot(row)

# =============================================================================
# 5. MATRICE DE CORRÉLATION INTERACTIVE (SEABORN)
# =============================================================================
sns.set_theme(style="whitegrid", context="paper")
df['Statut'] = np.where(df['NRMSE(%)'] > ERROR_THRESHOLD, f'Erreur > {ERROR_THRESHOLD}%', f'Erreur < {ERROR_THRESHOLD}%')
palette_status = {f'Erreur < {ERROR_THRESHOLD}%': 'lightgrey', f'Erreur > {ERROR_THRESHOLD}%': '#d62728'}

print(f"\nGénération de la matrice de corrélation sur les variables : {top_features}...")
scatter_fig = sns.pairplot(
    df, vars=top_features, hue='Statut', palette=palette_status,
    plot_kws={'alpha': 0.65, 's': 22, 'picker': True}, corner=True, height=2.8
)
scatter_fig.fig.suptitle(
    "Exploration de l'espace des phases — Cliquez sur un point rouge pour lancer l'autopsie temporelle",
    y=1.02, fontweight='bold', fontsize=11, color='#d62728'
)

# Connexion de l'événement de clic à la figure
scatter_fig.fig.canvas.mpl_connect('button_press_event', on_click)

plt.show()