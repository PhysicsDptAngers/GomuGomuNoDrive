import("stdfaust.lib");

// --- 1. IMPORT C++ FFI ---
rubber_L = ffunction(float process_rubber_L(float, float, float, float, float, float, float, float, float, float, float, float), "rubber_wrapper.h", "");
rubber_R = ffunction(float process_rubber_R(float, float, float, float, float, float, float, float, float, float, float, float), "rubber_wrapper.h", "");

// --- 2. PARAMÈTRES (Interface Brute) ---
drive = hslider("Drive Input", 0.5, 0.0, 1.0, 0.01);

alpha_up = hslider("Threshold +", 2.0, 0.1, 5.0, 0.01) / 5.0;
cap_up   = max(hslider("Capacitor + (nF)", 100.0, 0.0, 1000.0, 1.0) * 1e-9, 1e-15);
pinch_up = max(hslider("Pinch + (Ohms)", 1000.0, 0.0, 10000.0, 10.0), 1.0);
knee_up  = max(hslider("Knee R_k + (Ohms)", 10.0, 0.0, 10000.0, 1.0), 0.0);
pos_up   = checkbox("Cap Pos + (0=R_up, 1=R_low)");

alpha_dn = hslider("Threshold -", 2.0, 0.1, 5.0, 0.01) / 5.0;
cap_dn   = max(hslider("Capacitor - (nF)", 100.0, 0.0, 1000.0, 1.0) * 1e-9, 1e-15);
pinch_dn = max(hslider("Pinch - (Ohms)", 1000.0, 0.0, 10000.0, 10.0), 1.0);
knee_dn  = max(hslider("Knee R_k - (Ohms)", 10.0, 0.0, 10000.0, 1.0), 0.0);
pos_dn   = checkbox("Cap Pos - (0=R_up, 1=R_low)");

// --- 3. MISE À L'ÉCHELLE DES SIGNAUX ---
P_gain   = 50000.0;
R_up_div = 4700.0 + (1.0 - drive) * P_gain;
R_dn_div = 4700.0 + drive * P_gain;
G_in     = R_dn_div / (R_up_div + R_dn_div);

toVoltsScale   = 0.1 * (1.0 + (drive * 74.0));
nominalCeiling = 3.0;

dt = 1.0 / ma.SR; // ma.SR s'adapte automatiquement si FAUST oversample

// --- 4. ROUTING AUDIO STÉRÉO ---
process_L(vin) = vin * G_in * toVoltsScale
               : rubber_L(dt, alpha_up, knee_up, pinch_up, cap_up, pos_up, alpha_dn, knee_dn, pinch_dn, cap_dn, pos_dn)
               : _ / nominalCeiling;

process_R(vin) = vin * G_in * toVoltsScale
               : rubber_R(dt, alpha_up, knee_up, pinch_up, cap_up, pos_up, alpha_dn, knee_dn, pinch_dn, cap_dn, pos_dn)
               : _ / nominalCeiling;

process = process_L, process_R;
