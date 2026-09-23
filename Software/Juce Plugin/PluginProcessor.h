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
struct ScopeData {
    std::atomic<float> peakInVoltsPlus{0.0f};
    std::atomic<float> peakInVoltsMinus{0.0f};
    
    static constexpr int bufferSize = 512;
    std::array<float, bufferSize> bufferIn{};
    std::array<float, bufferSize> bufferOut{};
    std::atomic<int> writeIndex{0};
};

// ==============================================================================
// RUBBER ZENER NON-LINEAR SOLVER ENGINE
// ==============================================================================
class RubberZener {
public:
    RubberZener() = default;

    void reset() {
        for (int i = 0; i < 4; ++i) {
            X_up[i] = 0.0;
            X_dn[i] = 0.0;
        }
    }

    double processSample (double vin, double R_in, double dt, 
                          double alpha_up, double R_k_up, double R_p_up, double C_up, int pos_up,
                          double alpha_dn, double R_k_dn, double R_p_dn, double C_dn, int pos_dn);

private:
    double X_up[4] = {0.0, 0.0, 0.0, 0.0};
    double X_dn[4] = {0.0, 0.0, 0.0, 0.0};
    
    static constexpr double V_T  = 0.026;
    static constexpr double I_S  = 7.049e-15;
    static constexpr double B_F  = 493.2;
    static constexpr double B_R  = 2.886;
    static constexpr double I_SD = 21e-9;
    static constexpr double R_D  = 1.0;

    bool solveLinearSystem4x4 (const double J[4][4], const double F[4], double dX[4]);

    void solve_ebers_moll_step (double X[4], double vin, double dt, double C, 
                                double R_up, double R_low, double R_p, double R_s, int cap_pos);
};

// ==============================================================================
// JUCE AUDIO PROCESSOR MAIN CLASS
// ==============================================================================
class GomuGomuNoDrive final : public juce::AudioProcessor
{
public:
    enum VoicingMode {
        Voicing_TS = 0,
        Voicing_RAT,
        Voicing_Muff,
        Voicing_Lab
    };

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

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    RubberZener clippers[2];

    float dcBlockerX[2] { 0.0f, 0.0f };
    float dcBlockerY[2] { 0.0f, 0.0f };

    // Linear graphic EQ filter cascades (Mode Lab / Custom)
    std::array<std::array<juce::dsp::IIR::Filter<float>, numEqBands>, 2> preEqFilters;
    std::array<std::array<juce::dsp::IIR::Filter<float>, numEqBands>, 2> postEqFilters;

    // Voicing biquads
    std::array<juce::dsp::IIR::Filter<float>, 2> preVoicingFilters1;
    std::array<juce::dsp::IIR::Filter<float>, 2> preVoicingFilters2;
    std::array<juce::dsp::IIR::Filter<float>, 2> postVoicingFilters1;
    std::array<juce::dsp::IIR::Filter<float>, 2> postVoicingFilters2;
    std::array<juce::dsp::IIR::Filter<float>, 2> toneFilters;

    double lastSampleRate = 0.0;
    std::array<float, numEqBands> lastPreGains {};
    std::array<float, numEqBands> lastPostGains {};

    int lastVoicing = -1;
    float lastTone = -1.0f;
    float lastDrive = -1.0f;

    juce::dsp::Convolution cabConvolution;
    std::atomic<bool> hasValidIR { false };
    std::atomic<int> currentLoadedIRIndex { -1 };

    std::atomic<float>* inGainParam = nullptr;
    std::atomic<float>* outGainParam = nullptr;
    std::atomic<float>* voicingParam = nullptr;
    std::atomic<float>* toneParam = nullptr;

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
