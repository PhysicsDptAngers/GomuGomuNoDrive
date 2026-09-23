#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <algorithm>

// ==============================================================================
// OSCILLOSCOPE DATA STRUCTURE
// ==============================================================================
struct ScopeData {
    std::atomic<float> peakInPlus{0.0f};
    std::atomic<float> peakInMinus{0.0f};
    
    static constexpr int bufferSize = 512;
    std::array<float, bufferSize> bufferIn{};
    std::array<float, bufferSize> bufferOut{};
    std::atomic<int> writeIndex{0};
};

// ==============================================================================
// ADAA & MATH UTILITIES
// ==============================================================================
struct AdaaResult {
    double E;       // The anti-aliased exponential value
    double dE_dx;   // The exact derivative for the Jacobian matrix
};

// ==============================================================================
// CORE DSP CLASS: RubberZener (Dual Topology ODE Solver)
// ==============================================================================
class RubberZener {
public:
    RubberZener() = default;

    /**
     * @brief Resets the state-space memory of the ODE solver.
     * Call this when playback starts or is interrupted.
     */
    void reset() {
        for (int i = 0; i < 4; ++i) {
            X_up[i] = 0.0;
            X_dn[i] = 0.0;
        }
    }

    /**
     * @brief Processes a single audio sample through the dual-branch Rubber Zener ODE.
     * * @param vin      Input voltage sample
     * @param R_in     Global input series resistance (fixed to 10k)
     * @param dt       Time step (1.0 / SampleRate)
     * @param alpha_up Potentiometer position for the UP branch (0.0 to 1.0)
     * @param R_k_up   Series Knee resistor for UP branch
     * @param R_p_up   Pinch resistor for UP branch
     * @param C_up     Frequency-dependent capacitor for UP branch
     * @param pos_up   Capacitor position UP branch: 0 = Par. R_up, 1 = Par. R_low
     * @param alpha_dn Potentiometer position for the DOWN branch (0.0 to 1.0)
     * @param R_k_dn   Series Knee resistor for DOWN branch
     * @param R_p_dn   Pinch resistor for DOWN branch
     * @param C_dn     Frequency-dependent capacitor for DOWN branch
     * @param pos_dn   Capacitor position DOWN branch: 0 = Par. R_up, 1 = Par. R_low
     * @return double  The processed output voltage sample
     */
    double processSample(double vin, double R_in, double dt, 
                         double alpha_up, double R_k_up, double R_p_up, double C_up, int pos_up,
                         double alpha_dn, double R_k_dn, double R_p_dn, double C_dn, int pos_dn);

private:
    // --- STATE SPACE VECTORS ---
    // X[0] = V_up (Voltage across R_up)
    // X[1] = V_be (Voltage Base-Emitter)
    // X[2] = V_bc (Voltage Base-Collector)
    // X[3] = V_d  (Voltage across the Schottky Diode)
    double X_up[4] = {0.0, 0.0, 0.0, 0.0};
    double X_dn[4] = {0.0, 0.0, 0.0, 0.0};
    
    // --- PHYSICAL CONSTANTS (Ebers-Moll & Shockley) ---
    static constexpr double V_T  = 0.026;       // Thermal voltage at 25C
    static constexpr double I_S  = 7.049e-15;   // BJT Saturation current (BC550)
    static constexpr double B_F  = 493.2;       // Forward Beta
    static constexpr double B_R  = 2.886;       // Reverse Beta (critical for hard clipping)
    static constexpr double I_SD = 21e-9;       // Schottky Diode Saturation current (BAT54)
    static constexpr double R_D  = 1.0;         // Schottky intrinsic series resistance

    // --- INTERNAL METHODS ---
    AdaaResult adaa_exp(double v, double vp, double Vt);
    bool solveLinearSystem4x4(const double J[4][4], const double F[4], double dX[4]);
    void solve_ebers_moll_step(double X[4], double vin, double dt, double C, 
                               double R_up, double R_low, double R_p, double R_s, int cap_pos);
};

// ==============================================================================
// JUCE AUDIO PROCESSOR
// ==============================================================================
class GomuGomuNoDrive final : public juce::AudioProcessor
{
public:
    GomuGomuNoDrive();
    ~GomuGomuNoDrive() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    ScopeData scopeData; 

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameters();

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    RubberZener clippers[2];

    // Global Input
    std::atomic<float>* inGainParam = nullptr;

    // Positive Branch (UP) Parameters
    std::atomic<float>* threshPlusParam = nullptr;  // Maps to Alpha UP
    std::atomic<float>* capPlusParam = nullptr;
    std::atomic<float>* pinchPlusParam = nullptr;
    std::atomic<float>* kneePlusParam = nullptr;    // Maps to R_k UP
    std::atomic<float>* transPlusParam = nullptr;   // Unused for now (forced BJT)
    std::atomic<float>* capPosPlusParam = nullptr;

    // Negative Branch (DOWN) Parameters
    std::atomic<float>* threshMinusParam = nullptr; // Maps to Alpha DN
    std::atomic<float>* capMinusParam = nullptr;
    std::atomic<float>* pinchMinusParam = nullptr;
    std::atomic<float>* kneeMinusParam = nullptr;   // Maps to R_k DN
    std::atomic<float>* transMinusParam = nullptr;  // Unused for now
    std::atomic<float>* capPosMinusParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GomuGomuNoDrive)
};
