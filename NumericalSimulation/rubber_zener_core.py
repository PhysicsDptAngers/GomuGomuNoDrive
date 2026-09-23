#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
===============================================================================
RUBBER ZENER CORE ENGINE - UNIFIED SPICE & EBERS-MOLL SOLVER API
===============================================================================
Authors: Dominique Guichaoua & Matthieu Loumaigne
Date: 2026

Unified modeling library providing:
  1. Automated PySpice netlist generation and simulation (DC and Transient)
  2. 4D / 8D Implicit Newton-Raphson ODE solver (Backward Euler)
  3. Real-time oversampling (OVS) and First-Order ADAA options
  4. Exact cross-coupled non-linear capacitor discharge (8D mode for edge cases)
  5. Backtracking Line Search option for extreme parameter configurations
  6. Signal processing and harmonic analysis utilities
===============================================================================
"""

import warnings
warnings.filterwarnings("ignore")

import numpy as np
import scipy.signal as signal
from scipy.interpolate import interp1d

from PySpice.Spice.Netlist import Circuit, SubCircuitFactory
from PySpice.Unit import *
import PySpice.Spice.NgSpice.Shared as Shared

# =============================================================================
# 1. SPYDER IDE ANTI-CRASH SINGLETON LOCK FOR NGSPICE
# =============================================================================
if not hasattr(Shared.NgSpiceShared, '_patched_for_spyder'):
    def patched_exec_command(self, command):
        self._error_in_stderr = False
        rc = self._ngspice_shared.ngSpice_Command(command.encode('utf8'))
        if rc:
            raise Shared.NgSpiceCommandError(f"Command '{command}' failed")
    
    def patched_get_version(self):
        self._ngspice_version = 42 
        self._has_xspice = True
        self._has_cider = False
        
    Shared.NgSpiceShared.exec_command = patched_exec_command
    Shared.NgSpiceShared._get_version = patched_get_version
    Shared.NgSpiceShared._patched_for_spyder = True

# =============================================================================
# 2. PHYSICAL CONSTANTS & COMPONENT SPECS
# =============================================================================
CONSTANTS = {
    'Vt': 0.026,          # Thermal voltage at 25 °C (V)
    'Is': 7.049e-15,      # BJT reverse saturation current (BC550C)
    'Bf': 493.2,          # Forward Beta
    'Br': 2.886,          # Reverse Beta
    'IsD': 21.0e-9,       # BAT54 Schottky saturation current
    'Rd': 1.0,            # BAT54 series resistance (Ohms)
    'R_pot': 50.0e3,      # Potentiometer nominal track resistance (50 kOhms)
    'R_T1': 1.0e3,        # Top stopper resistance (1 kOhm)
    'R_T2': 4.7e3,        # Bottom stopper resistance (4.7 kOhms)
    'R_in': 10.0e3        # Standard input shunt divider resistance (10 kOhms)
}

def get_pot_resistances(alpha, R_pot=CONSTANTS['R_pot'], R_T1=CONSTANTS['R_T1'], R_T2=CONSTANTS['R_T2']):
    """Returns (R_up, R_low) based on potentiometer wiper alpha in [0, 1]."""
    R_up = (1.0 - alpha) * R_pot + R_T1
    R_low = alpha * R_pot + R_T2
    return R_up, R_low

def compute_gamma(alpha, C_F=0.0, freq_hz=1000.0, mode='up', R_pot=CONSTANTS['R_pot'], R_T1=CONSTANTS['R_T1'], R_T2=CONSTANTS['R_T2']):
    """Computes the theoretical multiplier factor |gamma(omega)|."""
    R_up, R_low = get_pot_resistances(alpha, R_pot, R_T1, R_T2)
    omega = 2.0 * np.pi * freq_hz
    if mode == 'up':
        Z_up = R_up if C_F <= 0 else R_up / (1.0 + 1j * omega * R_up * C_F)
        return np.abs(1.0 + Z_up / R_low)
    else:
        Z_low = R_low if C_F <= 0 else R_low / (1.0 + 1j * omega * R_low * C_F)
        return np.abs(1.0 + R_up / Z_low)

# =============================================================================
# 3. PYSPICE SUB-CIRCUITS & TRANSIENT/DC ENGINES
# =============================================================================
class RubberZenerSubcircuit(SubCircuitFactory):
    """Encapsulates a full one-way Rubber Zener branch with diode and knee resistor."""
    NODES = ('pos', 'neg')
    def __init__(self, name, alpha=0.5, r_k=0.0, r_p=0.0, c_val=0.0, cap_pos='up'):
        self.NAME = name
        super().__init__()
        
        R_up, R_low = get_pot_resistances(alpha)
        
        self.Diode('D', 'pos', 'node_d', model='BAT54')
        self.R('Rk', 'node_d', 'node_vn', max(float(r_k), 1e-3)@u_Ohm)
        self.R('Rp', 'node_vn', 'node_coll', max(float(r_p), 1e-3)@u_Ohm)
        
        self.R('Rup', 'node_vn', 'node_ctrl', float(R_up)@u_Ohm)
        self.R('Rlow', 'node_ctrl', 'neg', float(R_low)@u_Ohm)
        
        if float(c_val) > 0.0:
            if cap_pos == 'up':
                self.C('C', 'node_vn', 'node_cap', float(c_val)@u_F)
                self.R('esr', 'node_cap', 'node_ctrl', 1@u_mOhm)
            else:
                self.C('C', 'node_ctrl', 'node_cap', float(c_val)@u_F)
                self.R('esr', 'node_cap', 'neg', 1@u_mOhm)
                
        self.BJT('Q', 'node_coll', 'node_ctrl', 'neg', model='BC550')

def _build_pyspice_circuit(alpha_up, r_k_up, r_p_up, c_up, pos_up,
                           alpha_dn, r_k_dn, r_p_dn, c_dn, pos_dn, R_in=CONSTANTS['R_in']):
    """Constructs the standard antiparallel back-to-back circuit."""
    circuit = Circuit('Rubber Zener Antiparallel Shunt Clipper')
    
    circuit.model('BC550', 'npn', IS=7.049e-15, BF=493.2, VAF=23.89, IKF=0.1542,
                  BR=2.886, CJC=5.5e-12, CJE=11.5e-12, TR=10e-9, TF=420.3e-12)
    circuit.model('BAT54', 'D', IS=21e-9, RS=1.0, N=1.0, CJO=12e-12)
    
    circuit.R('in', 'node_in', 'node_out', float(R_in)@u_Ohm)
    
    # UP Branch (Positive clipping)
    circuit.subcircuit(RubberZenerSubcircuit('rz_up', alpha=alpha_up, r_k=r_k_up, 
                                            r_p=r_p_up, c_val=c_up, cap_pos=pos_up))
    circuit.X('UP', 'rz_up', 'node_out', circuit.gnd)
    
    # DN Branch (Negative clipping)
    circuit.subcircuit(RubberZenerSubcircuit('rz_dn', alpha=alpha_dn, r_k=r_k_dn, 
                                            r_p=r_p_dn, c_val=c_dn, cap_pos=pos_dn))
    circuit.X('DN', 'rz_dn', circuit.gnd, 'node_out')
    
    return circuit

def simulate_spice_dc(v_max=15.0, step=0.05,
                      alpha_up=0.5, r_k_up=0.0, r_p_up=0.0,
                      alpha_dn=None, r_k_dn=None, r_p_dn=None, R_in=CONSTANTS['R_in']):
    """Executes a DC sweep in PySpice. Returns (V_in, V_out, I_in_mA)."""
    if alpha_dn is None: alpha_dn = alpha_up
    if r_k_dn is None:   r_k_dn = r_k_up
    if r_p_dn is None:   r_p_dn = r_p_up
    
    circuit = _build_pyspice_circuit(alpha_up, r_k_up, r_p_up, 0.0, 'up',
                                     alpha_dn, r_k_dn, r_p_dn, 0.0, 'up', R_in=R_in)
    circuit.VoltageSource('input', 'node_in', circuit.gnd, 0@u_V)
    
    sim = circuit.simulator(temperature=25)
    res = sim.dc(Vinput=slice(-float(v_max), float(v_max), float(step)))
    
    vin = np.array(res.node_in)
    vout = np.array(res.node_out)
    iin_mA = (vin - vout) / (R_in / 1000.0)
    return vin, vout, iin_mA

def simulate_spice_transient(v_in_peak=10.0, freq_hz=150.0, num_periods=3.0,
                             alpha_up=0.5, r_k_up=0.0, r_p_up=0.0, c_up=0.0, pos_up='up',
                             alpha_dn=None, r_k_dn=None, r_p_dn=None, c_dn=None, pos_dn=None,
                             R_in=CONSTANTS['R_in']):
    """Executes an AC transient simulation in PySpice. Returns (t_sec, V_in, V_out)."""
    if alpha_dn is None: alpha_dn = alpha_up
    if r_k_dn is None:   r_k_dn = r_k_up
    if r_p_dn is None:   r_p_dn = r_p_up
    if c_dn is None:     c_dn = c_up
    if pos_dn is None:   pos_dn = pos_up
    
    circuit = _build_pyspice_circuit(alpha_up, r_k_up, r_p_up, c_up, pos_up,
                                     alpha_dn, r_k_dn, r_p_dn, c_dn, pos_dn, R_in=R_in)
    
    circuit.SinusoidalVoltageSource('input', 'node_in', circuit.gnd,
                                    amplitude=float(v_in_peak)@u_V, frequency=float(freq_hz)@u_Hz)
    
    sim = circuit.simulator(temperature=25)
    period = 1.0 / freq_hz
    end_time = num_periods * period
    step_time = period / 1000.0
    
    res = sim.transient(step_time=step_time@u_s, end_time=end_time@u_s)
    return np.array(res.time), np.array(res.node_in), np.array(res.node_out)

# =============================================================================
# 4. NUMERICAL EBERS-MOLL ODE SOLVER (WITH ADAA, OVS & BACKTRACKING)
# =============================================================================
def _adaa_exp(v, vp, Vt):
    """First-order Antiderivative Antialiasing (ADAA) evaluation of exp(v/Vt)."""
    x = np.clip(v / Vt, -100.0, 80.0)
    xp = np.clip(vp / Vt, -100.0, 80.0)
    dx = x - xp
    
    if abs(dx) > 1e-4:
        ex = np.exp(x)
        exp_p = np.exp(xp)
        E = (ex - exp_p) / dx
        dE_dx = (ex * dx - (ex - exp_p)) / (dx * dx)
    else:
        exp_p = np.exp(xp)
        E = exp_p * (1.0 + dx / 2.0 + (dx * dx) / 6.0)
        dE_dx = exp_p * (0.5 + dx / 3.0)
        
    return E, dE_dx / Vt

def solve_ebers_moll_step(X_prev, vin, dt, C, R_up, R_low, R_p, R_s, 
                             cap_pos='up', use_adaa=False, line_search=False):
    """
    Solves one discrete Backward Euler step of the 4D implicit system:
    X = [V_up, V_be, V_bc, V_d]^T
    Optional Backtracking Line Search guarantees convergence across steep cliffs.
    """
    Vt = CONSTANTS['Vt']
    Is = CONSTANTS['Is']
    Bf = CONSTANTS['Bf']
    Br = CONSTANTS['Br']
    IsD = CONSTANTS['IsD']
    
    X = np.copy(X_prev)
    final_step = np.zeros(4)
    
    for _ in range(50):
        Vup, Vbe, Vbc, Vd = X
        
        if use_adaa:
            evbe, devbe = _adaa_exp(Vbe, X_prev[1], Vt)
            evbc, devbc = _adaa_exp(Vbc, X_prev[2], Vt)
            evd,  devd  = _adaa_exp(Vd,  X_prev[3], Vt)
        else:
            evbe = np.exp(np.clip(Vbe / Vt, -100.0, 80.0))
            evbc = np.exp(np.clip(Vbc / Vt, -100.0, 80.0))
            evd  = np.exp(np.clip(Vd  / Vt, -100.0, 80.0))
            devbe = evbe / Vt
            devbc = evbc / Vt
            devd  = evd  / Vt
            
        ic = Is * (evbe - evbc) - (Is / Br) * (evbc - 1.0)
        ib = (Is / Bf) * (evbe - 1.0) + (Is / Br) * (evbc - 1.0)
        iD = IsD * (evd - 1.0)
        
        dic_dvbe = Is * devbe
        dic_dvbc = -Is * devbc - (Is / Br) * devbc
        dib_dvbe = (Is / Bf) * devbe
        dib_dvbc = (Is / Br) * devbc
        diD_dvd  = IsD * devd
        
        # F1: Collector KVL
        f1 = Vbc - R_p * ic + Vup
        df1_dvup = 1.0
        df1_dvbe = -R_p * dic_dvbe
        df1_dvbc = 1.0 - R_p * dic_dvbc
        df1_dvd  = 0.0
        
        # F2: Base node KCL (Differential state equation)
        if C > 0.0:
            if cap_pos == 'up':
                f2 = C * (Vup - X_prev[0]) / dt - Vbe / R_low - ib + Vup / R_up
                df2_dvup = C / dt + 1.0 / R_up
                df2_dvbe = -1.0 / R_low - dib_dvbe
                df2_dvbc = -dib_dvbc
                df2_dvd  = 0.0
            else:
                f2 = C * (Vbe - X_prev[1]) / dt + Vbe / R_low + ib - Vup / R_up
                df2_dvup = -1.0 / R_up
                df2_dvbe = C / dt + 1.0 / R_low + dib_dvbe
                df2_dvbc = dib_dvbc
                df2_dvd  = 0.0
        else:
            f2 = -Vbe / R_low - ib + Vup / R_up
            df2_dvup = 1.0 / R_up
            df2_dvbe = -1.0 / R_low - dib_dvbe
            df2_dvbc = -dib_dvbc
            df2_dvd  = 0.0
            
        # F3: Total Current Conservation
        if cap_pos == 'up':
            f3 = iD - ic - ib - Vbe / R_low
            df3_dvup = 0.0
            df3_dvbe = -dic_dvbe - dib_dvbe - 1.0 / R_low
            df3_dvbc = -dic_dvbc - dib_dvbc
            df3_dvd  = diD_dvd
        else:
            f3 = iD - ic - Vup / R_up
            df3_dvup = -1.0 / R_up
            df3_dvbe = -dic_dvbe
            df3_dvbc = -dic_dvbc
            df3_dvd  = diD_dvd
            
        # F4: Global Input KVL
        f4 = vin - R_s * iD - Vd - Vup - Vbe
        df4_dvup = -1.0
        df4_dvbe = -1.0
        df4_dvbc = 0.0
        df4_dvd  = -R_s * diD_dvd - 1.0
        
        F = np.array([f1, f2, f3, f4])
        J = np.array([
            [df1_dvup, df1_dvbe, df1_dvbc, df1_dvd],
            [df2_dvup, df2_dvbe, df2_dvbc, df2_dvd],
            [df3_dvup, df3_dvbe, df3_dvbc, df3_dvd],
            [df4_dvup, df4_dvbe, df4_dvbc, df4_dvd]
        ])
        
        try:
            dX = np.linalg.solve(J, F)
        except np.linalg.LinAlgError:
            dX = F * 0.01

        if line_search:
            # Backtracking Line Search to ensure energy-norm reduction
            norm_F = np.linalg.norm(F)
            alpha_ls = 1.0
            for _ls in range(5):
                step = np.clip(alpha_ls * dX, -1.0, 1.0)
                X_cand = X - step
                X_cand[1] = min(X_cand[1], 0.9)
                X_cand[2] = min(X_cand[2], 0.9)
                X_cand[3] = min(X_cand[3], 0.9)

                Vup_c, Vbe_c, Vbc_c, Vd_c = X_cand
                if use_adaa:
                    evbe_c, _ = _adaa_exp(Vbe_c, X_prev[1], Vt)
                    evbc_c, _ = _adaa_exp(Vbc_c, X_prev[2], Vt)
                    evd_c,  _ = _adaa_exp(Vd_c,  X_prev[3], Vt)
                else:
                    evbe_c = np.exp(np.clip(Vbe_c / Vt, -100.0, 80.0))
                    evbc_c = np.exp(np.clip(Vbc_c / Vt, -100.0, 80.0))
                    evd_c  = np.exp(np.clip(Vd_c  / Vt, -100.0, 80.0))

                ic_c = Is * (evbe_c - evbc_c) - (Is / Br) * (evbc_c - 1.0)
                ib_c = (Is / Bf) * (evbe_c - 1.0) + (Is / Br) * (evbc_c - 1.0)
                iD_c = IsD * (evd_c - 1.0)

                f1_c = Vbc_c - R_p * ic_c + Vup_c
                if C > 0.0:
                    if cap_pos == 'up':
                        f2_c = C * (Vup_c - X_prev[0]) / dt - Vbe_c / R_low - ib_c + Vup_c / R_up
                    else:
                        f2_c = C * (Vbe_c - X_prev[1]) / dt + Vbe_c / R_low + ib_c - Vup_c / R_up
                else:
                    f2_c = -Vbe_c / R_low - ib_c + Vup_c / R_up

                if cap_pos == 'up':
                    f3_c = iD_c - ic_c - ib_c - Vbe_c / R_low
                else:
                    f3_c = iD_c - ic_c - Vup_c / R_up

                f4_c = vin - R_s * iD_c - Vd_c - Vup_c - Vbe_c
                F_cand = np.array([f1_c, f2_c, f3_c, f4_c])

                if np.linalg.norm(F_cand) < norm_F or alpha_ls < 0.05:
                    X = X_cand
                    final_step = step
                    break
                alpha_ls *= 0.5
        else:
            final_step = np.clip(dX, -1.0, 1.0)
            X -= final_step
            X[1] = min(X[1], 0.9)
            X[2] = min(X[2], 0.9)
            X[3] = min(X[3], 0.9)
            
        if np.max(np.abs(final_step)) < 1e-6:
            break
            
    return X

def simulate_ode_dc(vin_array,
                    alpha_up=0.5, r_k_up=0.0, r_p_up=0.0,
                    alpha_dn=None, r_k_dn=None, r_p_dn=None,
                    pos_up='up', pos_dn='up', R_in=CONSTANTS['R_in']):
    """Calculates the static DC transfer characteristic using the memoryless ODE solver."""
    if alpha_dn is None: alpha_dn = alpha_up
    if r_k_dn is None:   r_k_dn = r_k_up
    if r_p_dn is None:   r_p_dn = r_p_up
    
    R_up_U, R_low_U = get_pot_resistances(alpha_up)
    R_s_U = R_in + r_k_up + CONSTANTS['Rd']
    
    R_up_D, R_low_D = get_pot_resistances(alpha_dn)
    R_s_D = R_in + r_k_dn + CONSTANTS['Rd']
    
    V_out = np.zeros_like(vin_array)
    X_up = np.zeros(4)
    X_dn = np.zeros(4)
    
    for n, vin in enumerate(vin_array):
        if vin >= 0.0:
            X_up = solve_ebers_moll_step(X_up, vin, 1.0, 0.0, R_up_U, R_low_U, r_p_up, R_s_U, pos_up, False)
            evd = np.exp(np.clip(X_up[3] / CONSTANTS['Vt'], -100.0, 80.0))
            iD = CONSTANTS['IsD'] * (evd - 1.0)
            V_out[n] = vin - R_in * iD
        else:
            X_dn = solve_ebers_moll_step(X_dn, -vin, 1.0, 0.0, R_up_D, R_low_D, r_p_dn, R_s_D, pos_dn, False)
            evd = np.exp(np.clip(X_dn[3] / CONSTANTS['Vt'], -100.0, 80.0))
            iD = CONSTANTS['IsD'] * (evd - 1.0)
            V_out[n] = vin + R_in * iD
            
    iin_mA = (vin_array - V_out) / (R_in / 1000.0)
    return V_out, iin_mA

def simulate_ode_transient(Vin, Fs,
                           alpha_up=0.5, r_k_up=0.0, r_p_up=0.0, c_up=0.0, pos_up='up',
                           alpha_dn=None, r_k_dn=None, r_p_dn=None, c_dn=None, pos_dn=None,
                           R_in=CONSTANTS['R_in'], ovs=1, use_adaa=False,
                           exact_cross_discharge=False, line_search=False):
    """
    Dynamic transient solver for continuous/discrete audio waveforms.
    Supports:
      - Arbitrary oversampling (ovs >= 1)
      - First-Order ADAA (use_adaa=True)
      - Fast decoupled 4D mode (exact_cross_discharge=False, default)
      - Full exact 8D cross-coupled non-linear discharge mode (exact_cross_discharge=True)
      - Backtracking line search (line_search=True)
    """
    if alpha_dn is None: alpha_dn = alpha_up
    if r_k_dn is None:   r_k_dn = r_k_up
    if r_p_dn is None:   r_p_dn = r_p_up
    if c_dn is None:     c_dn = c_up
    if pos_dn is None:   pos_dn = pos_up
    
    R_up_U, R_low_U = get_pot_resistances(alpha_up)
    R_s_U = R_in + r_k_up + CONSTANTS['Rd']
    
    R_up_D, R_low_D = get_pot_resistances(alpha_dn)
    R_s_D = R_in + r_k_dn + CONSTANTS['Rd']
    
    # 1. Multirate Upsampling
    if ovs > 1:
        Vin_proc = signal.resample_poly(Vin, ovs, 1)
    else:
        Vin_proc = Vin
        
    dt = 1.0 / (Fs * ovs)
    V_out_proc = np.zeros_like(Vin_proc)
    X_up = np.zeros(4)
    X_dn = np.zeros(4)
    
    for n, vin in enumerate(Vin_proc):
        if vin >= 0.0:
            # Active UP Branch
            X_up_new = solve_ebers_moll_step(
                X_up, vin, dt, c_up, R_up_U, R_low_U, r_p_up, R_s_U,
                cap_pos=pos_up, use_adaa=use_adaa, line_search=line_search
            )
            if use_adaa:
                evd, _ = _adaa_exp(X_up_new[3], X_up[3], CONSTANTS['Vt'])
            else:
                evd = np.exp(np.clip(X_up_new[3] / CONSTANTS['Vt'], -100.0, 80.0))
            iD = CONSTANTS['IsD'] * (evd - 1.0)
            vout_sample = vin - R_in * iD
            V_out_proc[n] = vout_sample
            X_up = X_up_new
            
            # Inactive DN Branch handling
            if exact_cross_discharge:
                # Exact 8D: Drain capacitor actively through reverse output voltage
                X_dn = solve_ebers_moll_step(
                    X_dn, -vout_sample, dt, c_dn, R_up_D, R_low_D, r_p_dn, R_s_D,
                    cap_pos=pos_dn, use_adaa=use_adaa, line_search=line_search
                )
            else:
                # Fast 4D: Autonomous linear discharge through bridge
                if c_dn > 0.0:
                    R_bleed = R_up_D if pos_dn == 'up' else R_low_D
                    X_dn[0 if pos_dn == 'up' else 1] *= np.exp(-dt / (R_bleed * c_dn))
                X_dn[2:] = 0.0
            
        else:
            # Active DN Branch
            X_dn_new = solve_ebers_moll_step(
                X_dn, -vin, dt, c_dn, R_up_D, R_low_D, r_p_dn, R_s_D,
                cap_pos=pos_dn, use_adaa=use_adaa, line_search=line_search
            )
            if use_adaa:
                evd, _ = _adaa_exp(X_dn_new[3], X_dn[3], CONSTANTS['Vt'])
            else:
                evd = np.exp(np.clip(X_dn_new[3] / CONSTANTS['Vt'], -100.0, 80.0))
            iD = CONSTANTS['IsD'] * (evd - 1.0)
            vout_sample = vin + R_in * iD
            V_out_proc[n] = vout_sample
            X_dn = X_dn_new
            
            # Inactive UP Branch handling
            if exact_cross_discharge:
                # Exact 8D: Drain capacitor actively through reverse output voltage
                X_up = solve_ebers_moll_step(
                    X_up, vout_sample, dt, c_up, R_up_U, R_low_U, r_p_up, R_s_U,
                    cap_pos=pos_up, use_adaa=use_adaa, line_search=line_search
                )
            else:
                # Fast 4D: Autonomous linear discharge
                if c_up > 0.0:
                    R_bleed = R_up_U if pos_up == 'up' else R_low_U
                    X_up[0 if pos_up == 'up' else 1] *= np.exp(-dt / (R_bleed * c_up))
                X_up[2:] = 0.0
            
    # 2. Multirate Downsampling
    if ovs > 1:
        V_out = signal.resample_poly(V_out_proc, 1, ovs)
        if len(V_out) > len(Vin):
            V_out = V_out[:len(Vin)]
        elif len(V_out) < len(Vin):
            V_out = np.pad(V_out, (0, len(Vin) - len(V_out)), 'edge')
    else:
        V_out = V_out_proc
        
    return V_out

# =============================================================================
# 5. SIGNAL PROCESSING & HARMONIC UTILITIES
# =============================================================================
def extract_steady_state(t, v, freq_hz, num_periods=5, n_pts=2048):
    """Interpolates the last N cycles onto a strictly uniform grid for leak-free FFT."""
    period = 1.0 / freq_hz
    t_start = t[-1] - num_periods * period
    t_uniform = np.linspace(t_start, t[-1], n_pts, endpoint=False)
    v_uniform = interp1d(t, v, kind='cubic')(t_uniform)
    return t_uniform, v_uniform

def compute_leak_free_fft(t_uniform, v_uniform, freq_hz):
    """Evaluates FFT magnitude spectrum normalized to the fundamental peak."""
    N = len(v_uniform)
    dt = t_uniform[1] - t_uniform[0]
    yf = np.fft.rfft(v_uniform) * (2.0 / N)
    xf = np.fft.rfftfreq(N, dt)
    mag_dB = 20.0 * np.log10(np.abs(yf) + 1e-9)
    idx_f0 = np.argmin(np.abs(xf - freq_hz))
    mag_norm = mag_dB - mag_dB[idx_f0]
    return xf, np.clip(mag_norm, -100.0, 5.0)