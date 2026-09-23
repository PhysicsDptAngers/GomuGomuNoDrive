#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
===============================================================================
JAES FIGURE GENERATOR: DYNAMIC REACTIVE MODES & RESIDUAL BENCHMARK
===============================================================================
Author: Matthieu Loumaigne
Date: September 2026

Description:
Generates publication-quality figures formatted specifically for the Journal 
of the Audio Engineering Society (JAES single-column standard: 3.45 in / 87 mm width).

This script compares the two dynamic reactive topologies against physical hardware:
- (a) Mode 2: Transient turn-on overshoot (capacitor in lower branch, C || R_low)
- (b) Mode 1: Dynamic high-frequency clamp (capacitor in upper branch, C || R_up)

Layout Architecture:
- Uses a two-tier nested GridSpec layout:
  * Two main vertical blocks (outer GridSpec with generous hspace to avoid collision).
  * Each block contains a primary waveform plot (3:1 height ratio) and an attached 
    instantaneous residual sub-panel (1:1 height ratio) sharing the same time axis.
- Intermediate x-axis tick labels are silenced to prevent visual overlap.
===============================================================================
"""

import os
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator

# =============================================================================
# 1. JAES PUBLICATION TYPOGRAPHY & STYLING CONFIGURATION
# =============================================================================
plt.rcParams.update({
    "font.family": "serif",
    "font.size": 8,
    "axes.labelsize": 8,
    "legend.fontsize": 6.8,
    "xtick.labelsize": 7,
    "ytick.labelsize": 7,
    "lines.linewidth": 1.1,
    "figure.dpi": 300,
})

# Input paths corresponding to the CSV data exported by the global fitting engine
csv_mode2 = "export_curves_for_paper/fit_AC_1000Hz_32p1nF_Rgamma_15p6K.csv"
csv_mode1 = "export_curves_for_paper/fit_AC_1000Hz_32p1nF_Rpinch2.47k_Rgamma_36p6K.csv"

# Verify file availability before proceeding
if not (os.path.exists(csv_mode2) and os.path.exists(csv_mode1)):
    print("[ERROR] Exported CSV measurement files not found.")
    print("Please run the global extraction script first to generate ./export_curves_for_paper/")
    exit(1)

# Load measurement and model dataframes
df_m2 = pd.read_csv(csv_mode2)
df_m1 = pd.read_csv(csv_mode1)

# =============================================================================
# 2. CANVAS & NESTED GRIDSPEC SETUP (PREVENTING AXIS OVERLAP)
# =============================================================================
# Standard JAES single-column width: 3.45 inches (87 mm), height: 5.0 inches
fig = plt.figure(figsize=(3.45, 5.0))

# Outer grid: two separate rows for Mode 2 (top) and Mode 1 (bottom)
outer = fig.add_gridspec(2, 1, height_ratios=[1, 1], hspace=0.42)

# Sub-grid block (a): Main waveform + attached residual plot
gs_a = outer[0].subgridspec(2, 1, height_ratios=[3.0, 1.0], hspace=0.08)
ax_t2 = fig.add_subplot(gs_a[0])
ax_r2 = fig.add_subplot(gs_a[1], sharex=ax_t2)

# Sub-grid block (b): Main waveform + attached residual plot
gs_b = outer[1].subgridspec(2, 1, height_ratios=[3.0, 1.0], hspace=0.08)
ax_t1 = fig.add_subplot(gs_b[0])
ax_r1 = fig.add_subplot(gs_b[1], sharex=ax_t1)

# Suppress intermediate time labels to eliminate collision with bottom sub-axes
ax_t2.tick_params(labelbottom=False)
ax_t1.tick_params(labelbottom=False)

# =============================================================================
# 3. PANEL (a): MODE 2 DYNAMIC RESPONSE - TRANSIENT OVERSHOOT (C || R_low)
# =============================================================================
# Filter first two complete cycles (2.0 ms at 1 kHz fundamental frequency)
mask2 = df_m2["time_ms"] <= 2.0
t2 = df_m2["time_ms"][mask2]
vin2 = df_m2["Vin_V"][mask2]
vmeas2 = df_m2["Vout_meas_V"][mask2]
vsim2 = df_m2["Vout_model_V"][mask2]
res2 = df_m2["residual_mV"][mask2]

# Top trace: Voltage signals
ax_t2.plot(t2, vin2, "k:", alpha=0.35, label=r"$V_{\mathrm{in}}$")
ax_t2.plot(t2[::2], vmeas2[::2], "b.", ms=2.5, alpha=0.5, label="Hardware")
ax_t2.plot(t2, vsim2, "r-", label="Model (Mode 2)")
ax_t2.set_ylabel(r"$V_{\mathrm{out}}$ (V)")
ax_t2.set_title(r"(a) Mode 2: Transient overshoot ($C \parallel R_{\mathrm{low}}$)", fontsize=8, fontweight="bold", pad=4)
ax_t2.grid(True, ls=":", alpha=0.5)
ax_t2.legend(loc="lower right", frameon=True, framealpha=0.9, borderpad=0.25, handletextpad=0.4)

# Bottom trace: Instantaneous residual error (V_meas - V_model in mV)
ax_r2.plot(t2, res2, "k-", lw=0.5)
ax_r2.axhline(0, color="r", ls="--", lw=0.5)
ax_r2.set_ylabel("Res. (mV)")

# Adaptive symmetric y-limits protecting against outlier peak truncation
r2_lim = max(float(np.percentile(np.abs(res2), 99.5) * 1.3), 30.0)
ax_r2.set_ylim(-r2_lim, r2_lim)
ax_r2.yaxis.set_major_locator(MaxNLocator(nbins=3, prune="both"))
ax_r2.grid(True, ls=":", alpha=0.5)

# =============================================================================
# 4. PANEL (b): MODE 1 DYNAMIC RESPONSE - HIGH-FREQUENCY CLAMP (C || R_up)
# =============================================================================
# Filter first two complete cycles (2.0 ms at 1 kHz fundamental frequency)
mask1 = df_m1["time_ms"] <= 2.0
t1 = df_m1["time_ms"][mask1]
vin1 = df_m1["Vin_V"][mask1]
vmeas1 = df_m1["Vout_meas_V"][mask1]
vsim1 = df_m1["Vout_model_V"][mask1]
res1 = df_m1["residual_mV"][mask1]

# Top trace: Voltage signals
ax_t1.plot(t1, vin1, "k:", alpha=0.35, label=r"$V_{\mathrm{in}}$")
ax_t1.plot(t1[::2], vmeas1[::2], "b.", ms=2.5, alpha=0.5, label="Hardware")
ax_t1.plot(t1, vsim1, "r-", label="Model (Mode 1)")
ax_t1.set_ylabel(r"$V_{\mathrm{out}}$ (V)")
ax_t1.set_title(r"(b) Mode 1: High-frequency clamp ($C \parallel R_{\mathrm{up}}$)", fontsize=8, fontweight="bold", pad=4)
ax_t1.grid(True, ls=":", alpha=0.5)
ax_t1.legend(loc="lower right", frameon=True, framealpha=0.9, borderpad=0.25, handletextpad=0.4)

# Bottom trace: Instantaneous residual error (V_meas - V_model in mV)
ax_r1.plot(t1, res1, "k-", lw=0.5)
ax_r1.axhline(0, color="r", ls="--", lw=0.5)
ax_r1.set_ylabel("Res. (mV)")
ax_r1.set_xlabel("Time (ms)")

# Adaptive symmetric y-limits protecting against outlier peak truncation
r1_lim = max(float(np.percentile(np.abs(res1), 99.5) * 1.3), 30.0)
ax_r1.set_ylim(-r1_lim, r1_lim)
ax_r1.yaxis.set_major_locator(MaxNLocator(nbins=3, prune="both"))
ax_r1.grid(True, ls=":", alpha=0.5)

# =============================================================================
# 5. VECTOR & RASTER EXPORT
# =============================================================================
plt.savefig("fig_jaes_reactive_modes.pdf", bbox_inches="tight")
plt.savefig("fig_jaes_reactive_modes.png", dpi=300, bbox_inches="tight")
print("[SUCCESS] Publication figure generated without label overlap: fig_jaes_reactive_modes.pdf / .png")