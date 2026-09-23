#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// RUBBER ZENER CORE IMPLEMENTATION (POLARIZED 4D NEWTON-RAPHSON SOLVER)
// ==============================================================================

bool RubberZener::solveLinearSystem4x4 (const double J[4][4], const double F[4], double dX[4]) {
    double A[4][5];
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) A[i][j] = J[i][j];
        A[i][4] = F[i];
    }
    
    for (int i = 0; i < 4; ++i) {
        int pivot = i;
        for (int j = i + 1; j < 4; ++j) {
            if (std::abs (A[j][i]) > std::abs (A[pivot][i])) pivot = j;
        }
        if (std::abs (A[pivot][i]) < 1e-12) return false;
        
        if (pivot != i) {
            for (int k = 0; k < 5; ++k) std::swap (A[i][k], A[pivot][k]);
        }
        
        for (int j = i + 1; j < 4; ++j) {
            double factor = A[j][i] / A[i][i];
            for (int k = i; k < 5; ++k) A[j][k] -= factor * A[i][k];
        }
    }
    
    for (int i = 3; i >= 0; --i) {
        dX[i] = A[i][4];
        for (int j = i + 1; j < 4; ++j) dX[i] -= A[i][j] * dX[j];
        dX[i] /= A[i][i];
    }
    return true;
}

void RubberZener::solve_ebers_moll_step (double X[4], double vin, double dt, double C, 
                                        double R_up, double R_low, double R_p, double R_s, int cap_pos) 
{
    const double X_prev[4] = { X[0], X[1], X[2], X[3] };
    
    for (int iter = 0; iter < 50; ++iter) {
        const double Vup = X[0];
        const double Vbe = X[1];
        const double Vbc = X[2];
        const double Vd  = X[3];
        
        const double x_be = juce::jlimit (-100.0, 80.0, Vbe / V_T);
        const double x_bc = juce::jlimit (-100.0, 80.0, Vbc / V_T);
        const double x_d  = juce::jlimit (-100.0, 80.0, Vd  / V_T);

        const double exp_be = std::exp (x_be);
        const double exp_bc = std::exp (x_bc);
        const double exp_d  = std::exp (x_d);

        const double dexp_dvbe = exp_be / V_T;
        const double dexp_dvbc = exp_bc / V_T;
        const double dexp_dvd  = exp_d  / V_T;
        
        const double ic = I_S * (exp_be - exp_bc) - (I_S / B_R) * (exp_bc - 1.0);
        const double ib = (I_S / B_F) * (exp_be - 1.0) + (I_S / B_R) * (exp_bc - 1.0);
        const double iD = I_SD * (exp_d - 1.0);
        
        const double dic_dvbe = I_S * dexp_dvbe;
        const double dic_dvbc = -I_S * dexp_dvbc - (I_S / B_R) * dexp_dvbc;
        const double dib_dvbe = (I_S / B_F) * dexp_dvbe;
        const double dib_dvbc = (I_S / B_R) * dexp_dvbc;
        const double diD_dvd  = I_SD * dexp_dvd;
        
        const double f1 = Vbc - R_p * ic + Vup;
        const double df1_dvup = 1.0;
        const double df1_dvbe = -R_p * dic_dvbe;
        const double df1_dvbc = 1.0 - R_p * dic_dvbc;
        const double df1_dvd  = 0.0;
        
        double f2 = 0.0, df2_dvup = 0.0, df2_dvbe = 0.0, df2_dvbc = 0.0;
        if (C > 1e-12) {
            if (cap_pos == 0) {
                f2 = C * (Vup - X_prev[0]) / dt - Vbe / R_low - ib + Vup / R_up;
                df2_dvup = C / dt + 1.0 / R_up;
                df2_dvbe = -1.0 / R_low - dib_dvbe;
                df2_dvbc = -dib_dvbc;
            } else {
                f2 = C * (Vbe - X_prev[1]) / dt + Vbe / R_low + ib - Vup / R_up;
                df2_dvup = -1.0 / R_up;
                df2_dvbe = C / dt + 1.0 / R_low + dib_dvbe;
                df2_dvbc = dib_dvbc;
            }
        } else {
            f2 = -Vbe / R_low - ib + Vup / R_up;
            df2_dvup = 1.0 / R_up;
            df2_dvbe = -1.0 / R_low - dib_dvbe;
            df2_dvbc = -dib_dvbc;
        }
        const double df2_dvd = 0.0;

        double f3 = 0.0, df3_dvup = 0.0, df3_dvbe = 0.0, df3_dvbc = 0.0;
        if (cap_pos == 0) {
            f3 = iD - ic - ib - Vbe / R_low;
            df3_dvup = 0.0;
            df3_dvbe = -dic_dvbe - dib_dvbe - 1.0 / R_low;
            df3_dvbc = -dic_dvbc - dib_dvbc;
        } else {
            f3 = iD - ic - Vup / R_up;
            df3_dvup = -1.0 / R_up;
            df3_dvbe = -dic_dvbe;
            df3_dvbc = -dic_dvbc;
        }
        const double df3_dvd = diD_dvd;
        
        const double f4 = vin - R_s * iD - Vd - Vup - Vbe;
        const double df4_dvup = -1.0;
        const double df4_dvbe = -1.0;
        const double df4_dvbc = 0.0;
        const double df4_dvd  = -R_s * diD_dvd - 1.0;
        
        const double F[4] = { f1, f2, f3, f4 };
        const double J[4][4] = {
            { df1_dvup, df1_dvbe, df1_dvbc, df1_dvd },
            { df2_dvup, df2_dvbe, df2_dvbc, df2_dvd },
            { df3_dvup, df3_dvbe, df3_dvbc, df3_dvd },
            { df4_dvup, df4_dvbe, df4_dvbc, df4_dvd }
        };
        
        double dX[4] = { 0.0, 0.0, 0.0, 0.0 };
        if (!solveLinearSystem4x4 (J, F, dX)) {
            for (int k = 0; k < 4; ++k) dX[k] = F[k] * 0.01; 
        }
        
        constexpr double max_step = 1.0;
        for (int k = 0; k < 4; ++k) {
            const double step = juce::jlimit (-max_step, max_step, dX[k]);
            X[k] -= step;
        }
        
        X[1] = std::min (X[1], 0.9);
        X[2] = std::min (X[2], 0.9);
        X[3] = std::min (X[3], 0.9);
        
        double max_err = 0.0;
        for (int k = 0; k < 4; ++k) {
            if (std::abs (dX[k]) > max_err) max_err = std::abs (dX[k]);
        }
        if (max_err < 1e-6) break;
    }
}

double RubberZener::processSample (double vin, double R_in, double dt, 
                                  double alpha_up, double R_k_up, double R_p_up, double C_up, int pos_up,
                                  double alpha_dn, double R_k_dn, double R_p_dn, double C_dn, int pos_dn) 
{
    const double R_up_U  = (1.0 - alpha_up) * 50000.0 + 1000.0;
    const double R_low_U = alpha_up * 50000.0 + 4700.0;
    const double R_s_U   = R_in + R_k_up + R_D;

    const double R_up_D  = (1.0 - alpha_dn) * 50000.0 + 1000.0;
    const double R_low_D = alpha_dn * 50000.0 + 4700.0;
    const double R_s_D   = R_in + R_k_dn + R_D;

    double v_out = vin;

    if (vin >= 0.0) {
        solve_ebers_moll_step (X_up, vin, dt, C_up, R_up_U, R_low_U, R_p_up, R_s_U, pos_up);
        const double x_d = juce::jlimit (-100.0, 80.0, X_up[3] / V_T);
        const double iD  = I_SD * (std::exp (x_d) - 1.0);
        v_out = vin - R_in * iD;

        if (C_dn > 1e-12) {
            const double R_bleed = (pos_dn == 0) ? R_up_D : R_low_D;
            const int state_idx  = (pos_dn == 0) ? 0 : 1;
            X_dn[state_idx] *= std::exp (-dt / (R_bleed * C_dn));
        } else {
            X_dn[0] = 0.0;
            X_dn[1] = 0.0;
        }
        X_dn[2] = 0.0; 
        X_dn[3] = 0.0; 
        if (pos_dn == 0) X_dn[1] = 0.0; else X_dn[0] = 0.0;
    } 
    else {
        solve_ebers_moll_step (X_dn, -vin, dt, C_dn, R_up_D, R_low_D, R_p_dn, R_s_D, pos_dn);
        const double x_d = juce::jlimit (-100.0, 80.0, X_dn[3] / V_T);
        const double iD  = I_SD * (std::exp (x_d) - 1.0);
        v_out = vin + R_in * iD;

        if (C_up > 1e-12) {
            const double R_bleed = (pos_up == 0) ? R_up_U : R_low_U;
            const int state_idx  = (pos_up == 0) ? 0 : 1;
            X_up[state_idx] *= std::exp (-dt / (R_bleed * C_up));
        } else {
            X_up[0] = 0.0;
            X_up[1] = 0.0;
        }
        X_up[2] = 0.0; 
        X_up[3] = 0.0; 
        if (pos_up == 0) X_up[1] = 0.0; else X_up[0] = 0.0;
    }

    return v_out;
}

// ==============================================================================
// JUCE PLUGIN MAIN WRAPPER
// ==============================================================================

GomuGomuNoDrive::GomuGomuNoDrive()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameters()),
      cabConvolution (juce::dsp::Convolution::NonUniform { 128 })
{
    inGainParam      = apvts.getRawParameterValue ("GAIN");
    outGainParam     = apvts.getRawParameterValue ("OUT_GAIN");
    voicingParam     = apvts.getRawParameterValue ("VOICING");
    toneParam        = apvts.getRawParameterValue ("TONE");
    
    threshPlusParam  = apvts.getRawParameterValue ("THRESH_P"); 
    capPlusParam     = apvts.getRawParameterValue ("CAP_P");
    pinchPlusParam   = apvts.getRawParameterValue ("PINCH_P");
    kneePlusParam    = apvts.getRawParameterValue ("KNEE_P"); 
    transPlusParam   = apvts.getRawParameterValue ("TRANS_P");
    capPosPlusParam  = apvts.getRawParameterValue ("CAP_POS_P");

    threshMinusParam = apvts.getRawParameterValue ("THRESH_M"); 
    capMinusParam    = apvts.getRawParameterValue ("CAP_M");
    pinchMinusParam  = apvts.getRawParameterValue ("PINCH_M");
    kneeMinusParam   = apvts.getRawParameterValue ("KNEE_M");
    transMinusParam  = apvts.getRawParameterValue ("TRANS_M");
    capPosMinusParam = apvts.getRawParameterValue ("CAP_POS_M");

    cabEnableParam   = apvts.getRawParameterValue ("CAB_ENABLE");
    cabSelectParam   = apvts.getRawParameterValue ("CAB_SELECT");

    for (size_t i = 0; i < numEqBands; ++i) {
        preEqParams[i]  = apvts.getRawParameterValue ("PRE_EQ_" + juce::String (i));
        postEqParams[i] = apvts.getRawParameterValue ("POST_EQ_" + juce::String (i));
        lastPreGains[i] = 0.0f;
        lastPostGains[i] = 0.0f;
    }
}

GomuGomuNoDrive::~GomuGomuNoDrive() {}

juce::AudioProcessorValueTreeState::ParameterLayout GomuGomuNoDrive::createParameters() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
    params.push_back (std::make_unique<juce::AudioParameterFloat>("GAIN", "Drive Input", 0.0f, 1.0f, 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("OUT_GAIN", "Output Level (dB)", -24.0f, 12.0f, 0.0f));

    juce::StringArray voicingChoices = { "TS-VOICE", "RAT-VOICE", "MUFF-VOICE", "LAB / CUSTOM" };
    params.push_back (std::make_unique<juce::AudioParameterChoice>("VOICING", "Voicing Mode", voicingChoices, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("TONE", "Tone", 0.0f, 1.0f, 0.5f));

    juce::StringArray transChoices = { "BJT", "MOSFET" };
    juce::StringArray capPosChoices = { "Parallel R_up", "Parallel R_low" };

    params.push_back (std::make_unique<juce::AudioParameterFloat>("THRESH_P", "Alpha Pot +", 0.0f, 1.0f, 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("CAP_P", "Capacitor + (nF)", 0.0f, 1000.0f, 100.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("PINCH_P", "Pinch + (Ohms)", 1.0f, 5000.0f, 1000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("KNEE_P", "R_k + (Ohms)", 0.0f, 10000.0f, 10.0f));
    params.push_back (std::make_unique<juce::AudioParameterChoice>("TRANS_P", "Transistor +", transChoices, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice>("CAP_POS_P", "Cap Pos +", capPosChoices, 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat>("THRESH_M", "Alpha Pot -", 0.0f, 1.0f, 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("CAP_M", "Capacitor - (nF)", 0.0f, 1000.0f, 100.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("PINCH_M", "Pinch - (Ohms)", 1.0f, 5000.0f, 1000.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat>("KNEE_M", "R_k - (Ohms)", 0.0f, 10000.0f, 10.0f));
    params.push_back (std::make_unique<juce::AudioParameterChoice>("TRANS_M", "Transistor -", transChoices, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice>("CAP_POS_M", "Cap Pos -", capPosChoices, 0));

    for (size_t i = 0; i < numEqBands; ++i) {
        juce::String id = "PRE_EQ_" + juce::String (i);
        juce::String name = "Pre EQ " + juce::String (static_cast<int>(eqFrequencies[i])) + " Hz";
        params.push_back (std::make_unique<juce::AudioParameterFloat>(id, name, -15.0f, 15.0f, 0.0f));
    }

    for (size_t i = 0; i < numEqBands; ++i) {
        juce::String id = "POST_EQ_" + juce::String (i);
        juce::String name = "Post EQ " + juce::String (static_cast<int>(eqFrequencies[i])) + " Hz";
        params.push_back (std::make_unique<juce::AudioParameterFloat>(id, name, -15.0f, 15.0f, 0.0f));
    }

    params.push_back (std::make_unique<juce::AudioParameterBool>("CAB_ENABLE", "Cabinet IR Enable", false));

    juce::StringArray cabChoices;
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        juce::String name = juce::String::fromUTF8 (BinaryData::originalFilenames[i]);
        if (name.endsWithIgnoreCase (".wav"))
            name = name.dropLastCharacters (4);
        cabChoices.add (name);
    }
    if (cabChoices.isEmpty()) cabChoices.add ("Default Cab");
    
    params.push_back (std::make_unique<juce::AudioParameterChoice>("CAB_SELECT", "Cabinet Model", cabChoices, 0));
    return { params.begin(), params.end() };
}

void GomuGomuNoDrive::loadBundledIR (int index)
{
    if (index < 0 || index >= BinaryData::namedResourceListSize)
        return;

    int dataSize = 0;
    const char* data = BinaryData::getNamedResource (BinaryData::namedResourceList[index], dataSize);

    if (data != nullptr && dataSize > 0)
    {
        cabConvolution.loadImpulseResponse (
            data, static_cast<size_t>(dataSize),
            juce::dsp::Convolution::Stereo::no,
            juce::dsp::Convolution::Trim::yes,
            0,
            juce::dsp::Convolution::Normalise::yes
        );
        hasValidIR.store (true);
        currentLoadedIRIndex.store (index);
    }
}

void GomuGomuNoDrive::updateFilters() {
    double sr = getSampleRate();
    if (sr <= 0.0) return;

    bool srChanged = (sr != lastSampleRate);
    lastSampleRate = sr;

    int currentVoicing = (voicingParam != nullptr) ? static_cast<int>(voicingParam->load()) : 0;
    float currentTone   = (toneParam != nullptr) ? toneParam->load() : 0.5f;
    float currentDrive  = (inGainParam != nullptr) ? inGainParam->load() : 0.5f;

    bool voicingChanged = (currentVoicing != lastVoicing) 
                       || (std::abs (currentTone - lastTone) > 0.005f) 
                       || (std::abs (currentDrive - lastDrive) > 0.01f);

    if (srChanged || voicingChanged) {
        lastVoicing = currentVoicing;
        lastTone    = currentTone;
        lastDrive   = currentDrive;

        if (currentVoicing == Voicing_TS) {
            // TS-VOICE: Filtre 1er ordre doux à 160 Hz (ne massacre pas les 82 Hz de la corde Mi grave)
            auto pre1 = juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sr, 160.0f);
            auto pre2 = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 850.0f, 1.0f, juce::Decibels::decibelsToGain (5.0f));
            
            auto post1 = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, 5800.0f, 0.707f);
            auto post2 = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, 120.0f, 0.707f, juce::Decibels::decibelsToGain (2.5f));
            
            float toneDb = -8.0f + currentTone * 16.0f;
            auto toneC = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 2500.0f, 0.707f, juce::Decibels::decibelsToGain (toneDb));

            for (size_t ch = 0; ch < 2; ++ch) {
                preVoicingFilters1[ch].coefficients = pre1;
                preVoicingFilters2[ch].coefficients = pre2;
                postVoicingFilters1[ch].coefficients = post1;
                postVoicingFilters2[ch].coefficients = post2;
                toneFilters[ch].coefficients = toneC;
            }
        }
        else if (currentVoicing == Voicing_RAT) {
            // RAT-VOICE: Coupe-bas 1er ordre 220 Hz pour assainir l'attaque, +6 dB de thump à 100 Hz en sortie
            auto pre1 = juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sr, 220.0f);
            auto pre2 = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 2200.0f, 1.3f, juce::Decibels::decibelsToGain (4.5f));
            
            auto post1 = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, 7200.0f, 0.65f);
            auto post2 = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 100.0f, 1.8f, juce::Decibels::decibelsToGain (6.0f)); // THUMP PALM-MUTE

            float toneCutoff = 1800.0f * std::pow (5.5f, currentTone); // 1.8 kHz -> 9.9 kHz
            auto toneC = juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, toneCutoff, 0.707f);

            for (size_t ch = 0; ch < 2; ++ch) {
                preVoicingFilters1[ch].coefficients = pre1;
                preVoicingFilters2[ch].coefficients = pre2;
                postVoicingFilters1[ch].coefficients = post1;
                postVoicingFilters2[ch].coefficients = post2;
                toneFilters[ch].coefficients = toneC;
            }
        }
        else if (currentVoicing == Voicing_Muff) {
            // MUFF-VOICE: Assise basse à 80 Hz, scoop à 450 Hz, boost massif de résonance à 90 Hz
            auto pre1 = juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sr, 80.0f);
            auto pre2 = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 280.0f, 1.0f, juce::Decibels::decibelsToGain (3.0f));
            
            auto post1 = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 450.0f, 0.9f, juce::Decibels::decibelsToGain (-6.5f)); // SCOOP
            auto post2 = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, 90.0f, 1.5f, juce::Decibels::decibelsToGain (6.5f));  // BASS BODY

            float toneDb = -10.0f + currentTone * 20.0f;
            auto toneC = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 1400.0f, 0.707f, juce::Decibels::decibelsToGain (toneDb));

            for (size_t ch = 0; ch < 2; ++ch) {
                preVoicingFilters1[ch].coefficients = pre1;
                preVoicingFilters2[ch].coefficients = pre2;
                postVoicingFilters1[ch].coefficients = post1;
                postVoicingFilters2[ch].coefficients = post2;
                toneFilters[ch].coefficients = toneC;
            }
        }
    }

    if (currentVoicing == Voicing_Lab) {
        constexpr float qFactor = 1.4f;
        for (size_t b = 0; b < numEqBands; ++b) {
            float preGainDb = (preEqParams[b] != nullptr) ? preEqParams[b]->load() : 0.0f;
            if (srChanged || preGainDb != lastPreGains[b] || preEqFilters[0][b].coefficients == nullptr) {
                lastPreGains[b] = preGainDb;
                float preGainLin = juce::Decibels::decibelsToGain (preGainDb);
                auto preCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, eqFrequencies[b], qFactor, preGainLin);
                preEqFilters[0][b].coefficients = preCoeffs;
                preEqFilters[1][b].coefficients = preCoeffs;
            }

            float postGainDb = (postEqParams[b] != nullptr) ? postEqParams[b]->load() : 0.0f;
            if (srChanged || postGainDb != lastPostGains[b] || postEqFilters[0][b].coefficients == nullptr) {
                lastPostGains[b] = postGainDb;
                float postGainLin = juce::Decibels::decibelsToGain (postGainDb);
                auto postCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, eqFrequencies[b], qFactor, postGainLin);
                postEqFilters[0][b].coefficients = postCoeffs;
                postEqFilters[1][b].coefficients = postCoeffs;
            }
        }
    }
}

void GomuGomuNoDrive::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    size_t numChannels = static_cast<size_t>(juce::jmax (1, getTotalNumOutputChannels()));
    
    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        numChannels, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
    oversampler->initProcessing (static_cast<size_t>(samplesPerBlock));
    oversampler->reset();
    
    clippers[0].reset();
    clippers[1].reset();

    dcBlockerX[0] = dcBlockerX[1] = 0.0f;
    dcBlockerY[0] = dcBlockerY[1] = 0.0f;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(numChannels);
    cabConvolution.prepare (spec);
    cabConvolution.reset();

    lastSampleRate = 0.0;
    lastVoicing = -1;

    juce::dsp::ProcessSpec filterSpec;
    filterSpec.sampleRate = sampleRate;
    filterSpec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    filterSpec.numChannels = 1;

    for (size_t ch = 0; ch < 2; ++ch) {
        preVoicingFilters1[ch].prepare (filterSpec);
        preVoicingFilters1[ch].reset();
        preVoicingFilters2[ch].prepare (filterSpec);
        preVoicingFilters2[ch].reset();
        postVoicingFilters1[ch].prepare (filterSpec);
        postVoicingFilters1[ch].reset();
        postVoicingFilters2[ch].prepare (filterSpec);
        postVoicingFilters2[ch].reset();
        toneFilters[ch].prepare (filterSpec);
        toneFilters[ch].reset();

        for (size_t b = 0; b < numEqBands; ++b) {
            preEqFilters[ch][b].prepare (filterSpec);
            preEqFilters[ch][b].reset();
            postEqFilters[ch][b].prepare (filterSpec);
            postEqFilters[ch][b].reset();
        }
    }

    updateFilters();

    int targetIR = (cabSelectParam != nullptr) ? static_cast<int>(cabSelectParam->load()) : 0;
    loadBundledIR (targetIR);
}

void GomuGomuNoDrive::releaseResources() {
    oversampler.reset();
    cabConvolution.reset();
}

bool GomuGomuNoDrive::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() && 
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet()) return false;
    return true;
}

void GomuGomuNoDrive::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);

    int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() == 0 || numSamples == 0) return;
    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, numSamples);

    updateFilters();

    if (cabSelectParam != nullptr) {
        int selectedIR = static_cast<int>(cabSelectParam->load());
        if (selectedIR != currentLoadedIRIndex.load()) {
            loadBundledIR (selectedIR);
        }
    }

    // 1. Étage de Drive dynamique (plage de +0 dB à +42 dB)
    double alpha_in = (inGainParam != nullptr) ? static_cast<double>(inGainParam->load()) : 0.5;
    double driveDb = alpha_in * 42.0; 
    double toVoltsScale = 0.5 * juce::Decibels::decibelsToGain (driveDb);

    // 1b. DC-Blocker matériel (15 Hz)
    double sr = getSampleRate();
    float R_dc = (sr > 0.0) ? static_cast<float>(1.0 - (juce::MathConstants<double>::twoPi * 15.0 / sr)) : 0.998f;
    int maxChannels = std::min (buffer.getNumChannels(), 2);

    for (int ch = 0; ch < maxChannels; ++ch) {
        float* chData = buffer.getWritePointer (ch);
        float x1 = dcBlockerX[ch];
        float y1 = dcBlockerY[ch];
        for (int i = 0; i < numSamples; ++i) {
            float inSample = chData[i];
            float outSample = inSample - x1 + R_dc * y1;
            x1 = inSample;
            y1 = outSample;
            chData[i] = outSample;
        }
        dcBlockerX[ch] = x1;
        dcBlockerY[ch] = y1;
    }

    // Capture Input Signal Witness
    int startIdx = scopeData.writeIndex.load (std::memory_order_relaxed);
    int wIdx = startIdx;
    for (int i = 0; i < numSamples; ++i) {
        float inS = buffer.getSample (0, i);
        scopeData.bufferIn[static_cast<size_t>(wIdx)] = inS;
        wIdx = (wIdx + 1) % ScopeData::bufferSize;
    }

    // 2. Pre-Saturation Filtering
    int currentVoicing = (voicingParam != nullptr) ? static_cast<int>(voicingParam->load()) : 0;
    for (int ch = 0; ch < maxChannels; ++ch) {
        float* chData = buffer.getWritePointer (ch);
        if (currentVoicing == Voicing_Lab) {
            for (int i = 0; i < numSamples; ++i) {
                float s = chData[i];
                for (size_t b = 0; b < numEqBands; ++b) {
                    s = preEqFilters[static_cast<size_t>(ch)][b].processSample (s);
                }
                chData[i] = s;
            }
        } else {
            for (int i = 0; i < numSamples; ++i) {
                float s = chData[i];
                s = preVoicingFilters1[static_cast<size_t>(ch)].processSample (s);
                s = preVoicingFilters2[static_cast<size_t>(ch)].processSample (s);
                chData[i] = s;
            }
        }
    }

    // 3. Polyphase Half-Band Oversampling (2x)
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::AudioBlock<float> osBlock;
    if (oversampler != nullptr) osBlock = oversampler->processSamplesUp (block);
    else osBlock = block;

    double sampleRateOS = getSampleRate();
    if (oversampler != nullptr) sampleRateOS *= oversampler->getOversamplingFactor();
    double dt = 1.0 / sampleRateOS;

    double R_in_Thevenin = 10000.0;

    double alpha_up = (threshPlusParam != nullptr) ? juce::jlimit (0.0, 1.0, static_cast<double>(*threshPlusParam)) : 0.5; 
    double R_k_up   = (kneePlusParam != nullptr)   ? std::max (static_cast<double>(*kneePlusParam), 0.0) : 10.0;
    double R_p_up   = (pinchPlusParam != nullptr)  ? std::max (static_cast<double>(*pinchPlusParam), 1.0) : 1000.0;
    double C_up     = (capPlusParam != nullptr)    ? std::max (static_cast<double>(*capPlusParam) * 1e-9, 1e-15) : 1e-7;
    int pos_up      = (capPosPlusParam != nullptr) ? static_cast<int>(*capPosPlusParam) : 0;

    double alpha_dn = (threshMinusParam != nullptr) ? juce::jlimit (0.0, 1.0, static_cast<double>(*threshMinusParam)) : 0.5; 
    double R_k_dn   = (kneeMinusParam != nullptr)   ? std::max (static_cast<double>(*kneeMinusParam), 0.0) : 10.0;
    double R_p_dn   = (pinchMinusParam != nullptr)  ? std::max (static_cast<double>(*pinchMinusParam), 1.0) : 1000.0;
    double C_dn     = (capMinusParam != nullptr)    ? std::max (static_cast<double>(*capMinusParam) * 1e-9, 1e-15) : 1e-7;
    int pos_dn      = (capPosMinusParam != nullptr) ? static_cast<int>(*capPosMinusParam) : 0;

    // 4. Boucle non-linéaire Rubber Zener & mesure de tension crête réelle
    float pPlusV = 0.0f, pMinusV = 0.0f;

    for (size_t ch = 0; ch < osBlock.getNumChannels(); ++ch) {
        float* data = osBlock.getChannelPointer (ch);
        if (ch >= 2) continue; 
        for (size_t i = 0; i < osBlock.getNumSamples(); ++i) {
            double vin_volts = static_cast<double>(data[i]) * toVoltsScale; 
            
            if (ch == 0) {
                if (vin_volts > pPlusV) pPlusV = static_cast<float>(vin_volts);
                if (vin_volts < pMinusV) pMinusV = static_cast<float>(vin_volts);
            }

            double vout_volts = clippers[ch].processSample (
                vin_volts, R_in_Thevenin, dt,
                alpha_up, R_k_up, R_p_up, C_up, pos_up,
                alpha_dn, R_k_dn, R_p_dn, C_dn, pos_dn
            );
            
            constexpr double nominalCeilingVolts = 1.8; 
            data[i] = static_cast<float>(vout_volts / nominalCeilingVolts);
        }
    }

    // 5. Polyphase Half-Band Oversampling (2x Decimation)
    if (oversampler != nullptr) oversampler->processSamplesDown (block);

    // 6. Post-Saturation Filtering & Tone Shaping
    for (int ch = 0; ch < maxChannels; ++ch) {
        float* chData = buffer.getWritePointer (ch);
        if (currentVoicing == Voicing_Lab) {
            for (int i = 0; i < numSamples; ++i) {
                float s = chData[i];
                for (size_t b = 0; b < numEqBands; ++b) {
                    s = postEqFilters[static_cast<size_t>(ch)][b].processSample (s);
                }
                chData[i] = s;
            }
        } else {
            for (int i = 0; i < numSamples; ++i) {
                float s = chData[i];
                s = postVoicingFilters1[static_cast<size_t>(ch)].processSample (s);
                s = postVoicingFilters2[static_cast<size_t>(ch)].processSample (s);
                s = toneFilters[static_cast<size_t>(ch)].processSample (s);
                chData[i] = s;
            }
        }
    }

    // Capture Distorted Signal (Pre-Cab & Pre-Master Volume)
    wIdx = startIdx; 
    for (int i = 0; i < numSamples; ++i) {
        scopeData.bufferOut[static_cast<size_t>(wIdx)] = buffer.getSample (0, i);
        wIdx = (wIdx + 1) % ScopeData::bufferSize;
    }

    // 7. Speaker Cabinet Convolution
    bool cabEnabled = (cabEnableParam != nullptr && cabEnableParam->load() > 0.5f);
    if (cabEnabled && hasValidIR.load()) {
        cabConvolution.process (juce::dsp::ProcessContextReplacing<float>(block));
    }

    // 8. Master Output Volume
    float outGainLin = (outGainParam != nullptr) ? juce::Decibels::decibelsToGain (outGainParam->load()) : 1.0f;
    buffer.applyGain (outGainLin);

    scopeData.writeIndex.store (wIdx, std::memory_order_release);
    scopeData.peakInVoltsPlus.store (pPlusV, std::memory_order_relaxed);
    scopeData.peakInVoltsMinus.store (pMinusV, std::memory_order_relaxed);
}

const juce::String GomuGomuNoDrive::getName() const { return JucePlugin_Name; }
bool GomuGomuNoDrive::acceptsMidi() const { return false; }
bool GomuGomuNoDrive::producesMidi() const { return false; }
bool GomuGomuNoDrive::isMidiEffect() const { return false; }
double GomuGomuNoDrive::getTailLengthSeconds() const { return 0.0; }
int GomuGomuNoDrive::getNumPrograms() { return 1; }
int GomuGomuNoDrive::getCurrentProgram() { return 0; }
void GomuGomuNoDrive::setCurrentProgram (int index) { juce::ignoreUnused (index); }
const juce::String GomuGomuNoDrive::getProgramName (int index) { juce::ignoreUnused (index); return {}; }
void GomuGomuNoDrive::changeProgramName (int index, const juce::String& newName) { juce::ignoreUnused (index, newName); }

bool GomuGomuNoDrive::hasEditor() const { return true; }
juce::AudioProcessorEditor* GomuGomuNoDrive::createEditor() { return new GomuGomuNoDriveEditor (*this); }

void GomuGomuNoDrive::getStateInformation (juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void GomuGomuNoDrive::setStateInformation (const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType())) {
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
        if (cabSelectParam != nullptr) {
            loadBundledIR (static_cast<int>(cabSelectParam->load()));
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GomuGomuNoDrive(); }
