#pragma once
#include "PluginProcessor.h" 

// Instances statiques pour gérer la stéréo
static RubberZener clipper_L;
static RubberZener clipper_R;

extern "C" {
    float process_rubber_L(float vin, float dt, float alpha_up, float R_k_up, float R_p_up, float C_up, float pos_up, float alpha_dn, float R_k_dn, float R_p_dn, float C_dn, float pos_dn) {
        return clipper_L.processSample(vin, 10000.0, dt, alpha_up, R_k_up, R_p_up, C_up, (int)pos_up, alpha_dn, R_k_dn, R_p_dn, C_dn, (int)pos_dn);
    }

    float process_rubber_R(float vin, float dt, float alpha_up, float R_k_up, float R_p_up, float C_up, float pos_up, float alpha_dn, float R_k_dn, float R_p_dn, float C_dn, float pos_dn) {
        return clipper_R.processSample(vin, 10000.0, dt, alpha_up, R_k_up, R_p_up, C_up, (int)pos_up, alpha_dn, R_k_dn, R_p_dn, C_dn, (int)pos_dn);
    }
}
