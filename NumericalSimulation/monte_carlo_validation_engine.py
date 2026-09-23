#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Wed Sep 23 11:31:42 2026

@author: matth
"""

#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
===============================================================================
RUBBER ZENER DIODE - MULTIPROCESSING MONTE CARLO VALIDATION ENGINE
===============================================================================
Authors: Dominique Guichaoua & Matthieu Loumaigne
Date: 2026

Executes parallel Monte Carlo sweeps validating the numerical Ebers-Moll ODE twin
against golden-reference PySpice simulations.
Fully powered by the unified rubber_zener_core engine.
===============================================================================
"""

import os
import sys
import numpy as np
import concurrent.futures
import warnings
warnings.filterwarnings("ignore")

# Import de l'API unifiée
from rubber_zener_core import (
    simulate_spice_transient,
    simulate_ode_transient,
    CONSTANTS
)

# =============================================================================
# CONFIGURATION GLOBALE DU BENCHMARK MONTE CARLO
# =============================================================================
N_TESTS = 5000                  # Nombre de tirages aléatoires
TEST_MEMORYLESS = False          # True : régime statique C=0 | False : régime dynamique C in [1, 220] nF
OVS_FACTOR = 2                  # Facteur de sur-échantillonnage de l'ODE (1, 2, 4, 8)
USE_ADAA = False                # False : oversampling standard | True : ADAA d'ordre 1
EXACT_CROSS_DISCHARGE = False   # False : solveur 4D découplé (temps réel) | True : solveur 8D complet
LINE_SEARCH = True              # Backtracking Line Search pour sécuriser les points extrêmes

FS_INTERNAL = 48000 * 1         # Fréquence d'échantillonnage de référence (96 kHz)
SIM_TIME = 0.01                 # Durée totale de chaque simulation (10 ms)
STEADY_STATE_START_MS = 1.0     # Fenêtre initiale ignorée (suppression des transitoires/ringing)

NUM_WORKERS = max(1, 4)         # Processus parallèles (threads CPU)
V_IN_MIN = 1.0                  # Amplitude crête minimale (V)
V_IN_MAX = 7.0                  # Amplitude crête maximale (V)
OUTPUT_FILE = "monte_carlo_results.txt"

# =============================================================================
# MÉTRIQUE DE DISCORDANCE NRMSE
# =============================================================================
def compute_nrmse(y_ref, y_test):
    """Calcule le Normalized Root Mean Square Error (%) entre référence et modèle."""
    rmse = np.sqrt(np.mean((y_ref - y_test)**2))
    std_ref = np.std(y_ref)
    if std_ref < 1e-6:
        return 0.0
    return (rmse / std_ref) * 100.0

# =============================================================================
# TÂCHE UNITAIRE DU WORKER PARALLÈLE
# =============================================================================
def simulate_worker(test_id):
    # Tirages aléatoires des composants passifs et positions
    alpha_up = np.random.uniform(0.0, 1.0)
    R_k_up   = np.random.uniform(0.0, 10.0)      # en kOhm
    R_p_up   = np.random.uniform(1.0, 5000.0)    # en Ohm
    
    alpha_dn = np.random.uniform(0.0, 1.0)
    R_k_dn   = np.random.uniform(0.0, 10.0)      # en kOhm
    R_p_dn   = np.random.uniform(1.0, 5000.0)    # en Ohm
    
    pos_up   = np.random.choice(['up', 'down'])
    pos_dn   = np.random.choice(['up', 'down'])
    
    if TEST_MEMORYLESS:
        C_up = 0.0
        C_dn = 0.0
    else:
        C_up = np.random.uniform(1.0, 220.0)    # en nF
        C_dn = np.random.uniform(1.0, 220.0)
        
    v_in_peak = np.random.uniform(V_IN_MIN, V_IN_MAX)   
    freq      = np.random.uniform(50.0, 5000.0)         
    
    # Conversions d'unités conformes à l'API de rubber_zener_core
    r_k_up_ohm = R_k_up * 1e3
    r_k_dn_ohm = R_k_dn * 1e3
    c_up_farad = C_up * 1e-9
    c_dn_farad = C_dn * 1e-9
    
    try:
        # 1. Simulation de référence PySpice
        # La marge (+ 2.0 / FS_INTERNAL) garantit que la grille temporelle SPICE couvre t_ode jusqu'au bout
        num_periods_sim = (SIM_TIME + 2.0 / FS_INTERNAL) * freq
        t_sp, _, v_sp = simulate_spice_transient(
            v_in_peak=v_in_peak, freq_hz=freq, num_periods=num_periods_sim,
            alpha_up=alpha_up, r_k_up=r_k_up_ohm, r_p_up=R_p_up, c_up=c_up_farad, pos_up=pos_up,
            alpha_dn=alpha_dn, r_k_dn=r_k_dn_ohm, r_p_dn=R_p_dn, c_dn=c_dn_farad, pos_dn=pos_dn
        )

        # 2. Simulation du jumeau numérique ODE
        n_samples = int(SIM_TIME * FS_INTERNAL)
        t_ode = np.linspace(0.0, SIM_TIME, n_samples, endpoint=False)
        Vin_ode = v_in_peak * np.sin(2.0 * np.pi * freq * t_ode)
        
        v_ode = simulate_ode_transient(
            Vin_ode, FS_INTERNAL,
            alpha_up=alpha_up, r_k_up=r_k_up_ohm, r_p_up=R_p_up, c_up=c_up_farad, pos_up=pos_up,
            alpha_dn=alpha_dn, r_k_dn=r_k_dn_ohm, r_p_dn=R_p_dn, c_dn=c_dn_farad, pos_dn=pos_dn,
            ovs=OVS_FACTOR,
            use_adaa=USE_ADAA,
            exact_cross_discharge=EXACT_CROSS_DISCHARGE,
            line_search=LINE_SEARCH
        )
        
        # 3. Ré-échantillonnage linéaire de SPICE sur la grille de calcul fixe
        v_sp_interp = np.interp(t_ode, t_sp, v_sp)
        
        # 4. Rogne du transitoire initial pour évaluer le régime permanent pur
        crop_time_s = max(0.001, STEADY_STATE_START_MS / 1000.0) 
        crop_idx = int(crop_time_s * FS_INTERNAL) 
        err = compute_nrmse(v_sp_interp[crop_idx:], v_ode[crop_idx:])
        
        result_str = (f"{alpha_up:.4f}\t{R_k_up:.4f}\t{R_p_up:.1f}\t{C_up:.1f}\t{pos_up}\t"
                      f"{alpha_dn:.4f}\t{R_k_dn:.4f}\t{R_p_dn:.1f}\t{C_dn:.1f}\t{pos_dn}\t"
                      f"{v_in_peak:.2f}\t{freq:.1f}\t{err:.6f}\n")
                      
        return (test_id, err, result_str, freq, v_in_peak, None)
        
    except Exception as e:
        return (test_id, None, None, freq, v_in_peak, str(e))

# =============================================================================
# EXÉCUTION DU BANC MULTIPROCESSUS
# =============================================================================
def run_monte_carlo():
    mode_str = "MEMORYLESS (C=0)" if TEST_MEMORYLESS else "DYNAMIC (C variable)"
    mode_cross = "EXACT 8D (Cross-Discharge)" if EXACT_CROSS_DISCHARGE else "DECOUPLED 4D (Linear RC)"
    mode_dsp = f"ADAA + OVS {OVS_FACTOR}x" if USE_ADAA else f"Standard OVS {OVS_FACTOR}x"

    print("=" * 80)
    print(" RUBBER ZENER - MULTIPROCESSING MONTE CARLO VALIDATION")
    print(f" Circuit Mode:    {mode_str}")
    print(f" Solver Topology: {mode_cross} | {mode_dsp}")
    print(f" Line Search:     {'ENABLED' if LINE_SEARCH else 'DISABLED'}")
    print(f" Tests:           {N_TESTS} | Workers: {NUM_WORKERS}")
    print("=" * 80)

    with open(OUTPUT_FILE, 'w') as f:
        f.write("AlphaU\tRkU(kOhm)\tRpU(Ohm)\tCu(nF)\tPosU\tAlphaD\tRkD(kOhm)\tRpD(Ohm)\tCd(nF)\tPosD\tVinPeak(V)\tFreq(Hz)\tNRMSE(%)\n")

    nrmse_results = []
    completed = 0
    
    with concurrent.futures.ProcessPoolExecutor(max_workers=NUM_WORKERS) as executor:
        futures = {executor.submit(simulate_worker, i): i for i in range(N_TESTS)}
        
        for future in concurrent.futures.as_completed(futures):
            test_id, err, result_str, freq, v_in_peak, exception = future.result()
            completed += 1
            
            if exception:
                sys.stdout.write(f"\r[!] Simulation échouée à l'itération {test_id}: {exception[:30]}...".ljust(80))
                sys.stdout.flush()
                continue
                
            nrmse_results.append(err)
            
            with open(OUTPUT_FILE, 'a') as f:
                f.write(result_str)
                
            sys.stdout.write(f"\r[Iter {completed}/{N_TESTS}] Freq: {freq:4.0f}Hz | Vin: {v_in_peak:4.1f}V | NRMSE: {err:6.3f}%   ")
            sys.stdout.flush()

    if len(nrmse_results) > 0:
        mean_err = np.mean(nrmse_results)
        ste = np.std(nrmse_results) / np.sqrt(len(nrmse_results))
        max_err = np.max(nrmse_results)
        
        print("\n\n" + "=" * 80)
        print(" STATISTIQUES FINALES :")
        print(f" NRMSE Moyen :           {mean_err:.4f} %")
        print(f" Erreur Standard (STE) : {ste:.6f} %")
        print(f" NRMSE Maximum :         {max_err:.4f} %")
        print(f" Données sauvegardées dans '{OUTPUT_FILE}'")
        print("=" * 80)

if __name__ == "__main__":
    run_monte_carlo()