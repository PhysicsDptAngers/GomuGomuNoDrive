#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <cmath>
#include <algorithm>
#include "BinaryData.h"

// ==============================================================================
// OSCILLOSCOPE DATA SHARING BUFFER
// ==============================================================================
/**
 * Lock-free circular witness buffer used to stream input and output waveforms 
 * from the real-time audio thread to the GUI editor/oscilloscope.
 */
struct ScopeData {
    std::atomic<float> peakInPlus{0.0f};
    std::atomic<float> peakInMinus{0.0f};
    
    static constexpr int bufferSize = 512;
    std::array<float, bufferSize> bufferIn{};
    std::array<float, bufferSize> bufferOut{};
    std::atomic<int> writeIndex{0};
};

// ==============================================================================
// RUBBER ZENER NON-LINEAR SOLVER ENGINE
// ==============================================================================
/**
 * Physical model of the Rubber Zener shunt clipping dipole.
 * Employs an implicit 4D Newton-Raphson solver derived from the continuous-time
 * Ebers-Moll BJT transport equations and Shockley diode characteristics.
 * Discretized via Backward Euler, operating under multirate polyphase oversampling.
 */
class RubberZener {
public:
    RubberZener() = default;

    /**
     * Resets the internal state-space memory variables to zero.
     */
    void reset() {
        for (int i = 0; i < 4; ++i) {
            X_up[i] = 0.0;
            X_dn[i] = 0.0;
        }
    }

    /**
     * Processes a single audio sample through the antiparallel dipole.
     * 
     * @param vin       Instantaneous input voltage in Volts.
     * @param R_in      Thevenin driving resistance (typically 10 kOhms).
     * @param dt        Discretized time step (1.0 / (Fs * OVS)).
     * @param alpha_up  Positive branch potentiometer wiper position [0.0, 1.0].
     * @param R_k_up    Positive branch series knee resistor in Ohms.
     * @param R_p_up    Positive branch collector saturation plateau resistor in Ohms.
     * @param C_up      Positive branch reactive state capacitor in Farads.
     * @param pos_up    Capacitor position: 0 = parallel to R_up, 1 = parallel to R_low.
     * @param alpha_dn  Negative branch potentiometer wiper position [0.0, 1.0].
     * @param R_k_dn    Negative branch series knee resistor in Ohms.
     * @param R_p_dn    Negative branch collector saturation plateau resistor in Ohms.
     * @param C_dn      Negative branch reactive state capacitor in Farads.
     * @param pos_dn    Capacitor position: 0 = parallel to R_up, 1 = parallel to R_low.
     * @return Clamped instantaneous output voltage across the shunt dipole.
     */
    double processSample (double vin, double R_in, double dt, 
                          double alpha_up, double R_k_up, double R_p_up, double C_up, int pos_up,
                          double alpha_dn, double R_k_dn, double R_p_dn, double C_dn, int pos_dn);

private:
    // State vectors: X = [V_up, V_be, V_bc, V_d]^T
    double X_up[4] = {0.0, 0.0, 0.0, 0.0};
    double X_dn[4] = {0.0, 0.0, 0.0, 0.0};
    
    // Physical Semiconductor Constants
    static constexpr double V_T  = 0.026;        // Thermal voltage at 25°C (26 mV)
    static constexpr double I_S  = 7.049e-15;    // BC550C BJT transport saturation current
    static constexpr double B_F  = 493.2;        // Forward common-emitter current gain
    static constexpr double B_R  = 2.886;        // Reverse current gain (governs deep saturation)
    static constexpr double I_SD = 21e-9;        // BAT54 Schottky diode saturation current
    static constexpr double R_D  = 1.0;          // Schottky intrinsic bulk series resistance

    /**
     * Solves the 4x4 linear algebraic system J * dX = F using Gaussian elimination 
     * with partial pivoting.
     */
    bool solveLinearSystem4x4 (const double J[4][4], const double F[4], double dX[4]);

    /**
     * Core implicit 4D Newton-Raphson iteration loop for one time step.
     */
    void solve_ebers_moll_step (double X[4], double vin, double dt, double C, 
                                double R_up, double R_low, double R_p, double R_s, int cap_pos);
};

// ==============================================================================
// JUCE AUDIO PROCESSOR MAIN CLASS
// ==============================================================================
class GomuGomuNoDrive final : public juce::AudioProcessor
{
public:
    static constexpr size_t numEqBands = 12;
    static constexpr std::array<float, numEqBands> eqFrequencies = {
        60.0f, 100.0f, 160.0f, 250.0f, 400.0f, 630.0f,
        1000.0f, 1600.0f, 2500.0f, 3500.0f, 5000.0f, 8000.0f
    };

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

    void loadBundledIR (int index);
    bool isIRLoaded() const noexcept { return hasValidIR.load(); }

    juce::AudioProcessorValueTreeState apvts;
    ScopeData scopeData; 

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    void updateFilters();

    // High-performance polyphase multirate oversampler (eliminates ADAA overhead)
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    
    // Per-channel Rubber Zener nonlinear solvers
    RubberZener clippers[2];

    // Hardware DC-Blocker state variables (15 Hz High-pass)
    float dcBlockerX[2] { 0.0f, 0.0f };
    float dcBlockerY[2] { 0.0f, 0.0f };

    // Linear graphic EQ filter cascades
    std::array<std::array<juce::dsp::IIR::Filter<float>, numEqBands>, 2> preEqFilters;
    std::array<std::array<juce::dsp::IIR::Filter<float>, numEqBands>, 2> postEqFilters;

    double lastSampleRate = 0.0;
    std::array<float, numEqBands> lastPreGains {};
    std::array<float, numEqBands> lastPostGains {};

    // Cabinet simulation
    juce::dsp::Convolution cabConvolution;
    std::atomic<bool> hasValidIR { false };
    std::atomic<int> currentLoadedIRIndex { -1 };

    // Cached APVTS raw parameter pointers
    std::atomic<float>* inGainParam = nullptr;
    std::atomic<float>* outGainParam = nullptr;

    std::atomic<float>* threshPlusParam = nullptr;
    std::atomic<float>* capPlusParam = nullptr;
    std::atomic<float>* pinchPlusParam = nullptr;
    std::atomic<float>* kneePlusParam = nullptr;
    std::atomic<float>* transPlusParam = nullptr;
    std::atomic<float>* capPosPlusParam = nullptr;

    std::atomic<float>* threshMinusParam = nullptr;
    std::atomic<float>* capMinusParam = nullptr;
    std::atomic<float>* pinchMinusParam = nullptr;
    std::atomic<float>* kneeMinusParam = nullptr;
    std::atomic<float>* transMinusParam = nullptr;
    std::atomic<float>* capPosMinusParam = nullptr;

    std::atomic<float>* cabEnableParam = nullptr;
    std::atomic<float>* cabSelectParam = nullptr;

    std::array<std::atomic<float>*, numEqBands> preEqParams {};
    std::array<std::atomic<float>*, numEqBands> postEqParams {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GomuGomuNoDrive)
};
