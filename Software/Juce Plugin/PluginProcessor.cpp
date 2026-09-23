#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// RUBBER ZENER CORE IMPLEMENTATION (POLARIZED 4D NEWTON-RAPHSON SOLVER)
// ==============================================================================

/**
 * Solves a 4x4 linear algebraic system (J * dX = F) using Gaussian elimination 
 * with partial row pivoting to prevent numerical instability.
 */
bool RubberZener::solveLinearSystem4x4 (const double J[4][4], const double F[4], double dX[4]) {
    double A[4][5];
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) A[i][j] = J[i][j];
        A[i][4] = F[i];
    }
    
    // Forward elimination with row pivoting
    for (int i = 0; i < 4; ++i) {
        int pivot = i;
        for (int j = i + 1; j < 4; ++j) {
            if (std::abs (A[j][i]) > std::abs (A[pivot][i])) pivot = j;
        }
        if (std::abs (A[pivot][i]) < 1e-12) return false; // Singular matrix
        
        if (pivot != i) {
            for (int k = 0; k < 5; ++k) std::swap (A[i][k], A[pivot][k]);
        }
        
        for (int j = i + 1; j < 4; ++j) {
            double factor = A[j][i] / A[i][i];
            for (int k = i; k < 5; ++k) A[j][k] -= factor * A[i][k];
        }
    }
    
    // Back-substitution
    for (int i = 3; i >= 0; --i) {
        dX[i] = A[i][4];
        for (int j = i + 1; j < 4; ++j) dX[i] -= A[i][j] * dX[j];
        dX[i] /= A[i][i];
    }
    return true;
}

/**
 * Resolves the 4 nonlinear differential-algebraic equations of the conducting branch.
 * 
 * Unlike ADAA, evaluation relies on direct point-wise exponential transport:
 * exp(v / Vt) and its exact continuous derivative exp(v / Vt) / Vt.
 * This completely avoids division-by-zero singularities and eliminates the 
 * wideband noise floor injected by zero-crossing boundary discontinuities.
 */
void RubberZener::solve_ebers_moll_step (double X[4], double vin, double dt, double C, 
                                        double R_up, double R_low, double R_p, double R_s, int cap_pos) 
{
    // Snapshot of previous time step state vector for Backward Euler discretization
    const double X_prev[4] = { X[0], X[1], X[2], X[3] };
    
    for (int iter = 0; iter < 50; ++iter) {
        const double Vup = X[0];
        const double Vbe = X[1];
        const double Vbc = X[2];
        const double Vd  = X[3];
        
        // --- 1. Evaluate Semiconductor Exponential Junctions ---
        // Voltages are clipped to prevent floating-point overflow during early iterations
        const double x_be = juce::jlimit (-100.0, 80.0, Vbe / V_T);
        const double x_bc = juce::jlimit (-100.0, 80.0, Vbc / V_T);
        const double x_d  = juce::jlimit (-100.0, 80.0, Vd  / V_T);

        const double exp_be = std::exp (x_be);
        const double exp_bc = std::exp (x_bc);
        const double exp_d  = std::exp (x_d);

        // Exact analytical derivatives: d(exp(v/Vt))/dv = exp(v/Vt) / Vt
        const double dexp_dvbe = exp_be / V_T;
        const double dexp_dvbc = exp_bc / V_T;
        const double dexp_dvd  = exp_d  / V_T;
        
        // --- 2. Macroscopic Currents (Ebers-Moll Model + Shockley Diode) ---
        const double ic = I_S * (exp_be - exp_bc) - (I_S / B_R) * (exp_bc - 1.0);
        const double ib = (I_S / B_F) * (exp_be - 1.0) + (I_S / B_R) * (exp_bc - 1.0);
        const double iD = I_SD * (exp_d - 1.0);
        
        // Partial derivatives with respect to state variables
        const double dic_dvbe = I_S * dexp_dvbe;
        const double dic_dvbc = -I_S * dexp_dvbc - (I_S / B_R) * dexp_dvbc;
        const double dib_dvbe = (I_S / B_F) * dexp_dvbe;
        const double dib_dvbc = (I_S / B_R) * dexp_dvbc;
        const double diD_dvd  = I_SD * dexp_dvd;
        
        // --- 3. Evaluate System Residues F(X) and Jacobian J(X) ---

        // F1: Collector KVL across internal saturation resistor R_p
        const double f1 = Vbc - R_p * ic + Vup;
        const double df1_dvup = 1.0;
        const double df1_dvbe = -R_p * dic_dvbe;
        const double df1_dvbc = 1.0 - R_p * dic_dvbc;
        const double df1_dvd  = 0.0;
        
        // F2: Base node KCL (containing reactive Backward Euler state derivative)
        double f2 = 0.0, df2_dvup = 0.0, df2_dvbe = 0.0, df2_dvbc = 0.0;
        if (C > 1e-12) {
            if (cap_pos == 0) {
                // Mode 1: C is placed in parallel with R_up (state variable is V_up)
                f2 = C * (Vup - X_prev[0]) / dt - Vbe / R_low - ib + Vup / R_up;
                df2_dvup = C / dt + 1.0 / R_up;
                df2_dvbe = -1.0 / R_low - dib_dvbe;
                df2_dvbc = -dib_dvbc;
            } else {
                // Mode 2: C is placed in parallel with R_low (state variable is V_be)
                f2 = C * (Vbe - X_prev[1]) / dt + Vbe / R_low + ib - Vup / R_up;
                df2_dvup = -1.0 / R_up;
                df2_dvbe = C / dt + 1.0 / R_low + dib_dvbe;
                df2_dvbc = dib_dvbc;
            }
        } else {
            // Memoryless algebraic configuration (C = 0)
            f2 = -Vbe / R_low - ib + Vup / R_up;
            df2_dvup = 1.0 / R_up;
            df2_dvbe = -1.0 / R_low - dib_dvbe;
            df2_dvbc = -dib_dvbc;
        }
        const double df2_dvd = 0.0;

        // F3: Dipole input node current conservation
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
        
        // F4: Global input loop KVL
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
        
        // --- 4. Matrix Inversion & Newton Step ---
        double dX[4] = { 0.0, 0.0, 0.0, 0.0 };
        if (!solveLinearSystem4x4 (J, F, dX)) {
            for (int k = 0; k < 4; ++k) dX[k] = F[k] * 0.01; 
        }
        
        // Step damping to prevent wild oscillations across steep gradients
        constexpr double max_step = 1.0;
        for (int k = 0; k < 4; ++k) {
            const double step = juce::jlimit (-max_step, max_step, dX[k]);
            X[k] -= step;
        }
        
        // Physical clamping: silicon PN junctions cannot exceed ~0.9 V forward bias
        X[1] = std::min (X[1], 0.9);
        X[2] = std::min (X[2], 0.9);
        X[3] = std::min (X[3], 0.9);
        
        // Convergence check (infinity norm of update vector)
        double max_err = 0.0;
        for (int k = 0; k < 4; ++k) {
            if (std::abs (dX[k]) > max_err) max_err = std::abs (dX[k]);
        }
        if (max_err < 1e-6) break;
    }
}

/**
 * Top-level sample processing routine handling mutual antiparallel branch decoupling.
 */
double RubberZener::processSample (double vin, double R_in, double dt, 
                                  double alpha_up, double R_k_up, double R_p_up, double C_up, int pos_up,
                                  double alpha_dn, double R_k_dn, double R_p_dn, double C_dn, int pos_dn) 
{
    // Potentiometer voltage divider resistance values with padding stoppers
    const double R_up_U  = (1.0 - alpha_up) * 50000.0 + 1000.0;
    const double R_low_U = alpha_up * 50000.0 + 4700.0;
    const double R_s_U   = R_in + R_k_up + R_D;

    const double R_up_D  = (1.0 - alpha_dn) * 50000.0 + 1000.0;
    const double R_low_D = alpha_dn * 50000.0 + 4700.0;
    const double R_s_D   = R_in + R_k_dn + R_D;

    double v_out = vin;

    if (vin >= 0.0) {
        // --- POSITIVE HALF-WAVE: UP BRANCH IS ACTIVE ---
        solve_ebers_moll_step (X_up, vin, dt, C_up, R_up_U, R_low_U, R_p_up, R_s_U, pos_up);
        
        // Direct diode conduction current
        const double x_d = juce::jlimit (-100.0, 80.0, X_up[3] / V_T);
        const double iD  = I_SD * (std::exp (x_d) - 1.0);
        v_out = vin - R_in * iD;

        // Inactive DOWN branch: autonomous linear RC discharge through the bridge
        if (C_dn > 1e-12) {
            const double R_bleed = (pos_dn == 0) ? R_up_D : R_low_D;
            const int state_idx  = (pos_dn == 0) ? 0 : 1;
            X_dn[state_idx] *= std::exp (-dt / (R_bleed * C_dn));
        }
        // Junctions shut off instantaneously in reverse bias
        X_dn[2] = 0.0; 
        X_dn[3] = 0.0; 
        if (pos_dn == 0) X_dn[1] = 0.0; else X_dn[0] = 0.0;
    } 
    else {
        // --- NEGATIVE HALF-WAVE: DOWN BRANCH IS ACTIVE ---
        // Invert input voltage for symmetric branch solver
        solve_ebers_moll_step (X_dn, -vin, dt, C_dn, R_up_D, R_low_D, R_p_dn, R_s_D, pos_dn);
        
        const double x_d = juce::jlimit (-100.0, 80.0, X_dn[3] / V_T);
        const double iD  = I_SD * (std::exp (x_d) - 1.0);
        v_out = vin + R_in * iD;

        // Inactive UP branch: autonomous linear RC discharge through the bridge
        if (C_up > 1e-12) {
            const double R_bleed = (pos_up == 0) ? R_up_U : R_low_U;
            const int state_idx  = (pos_up == 0) ? 0 : 1;
            X_up[state_idx] *= std::exp (-dt / (R_bleed * C_up));
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

    constexpr float qFactor = 1.4f;
    bool srChanged = (sr != lastSampleRate);
    lastSampleRate = sr;

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

void GomuGomuNoDrive::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    size_t numChannels = static_cast<size_t>(juce::jmax (1, getTotalNumOutputChannels()));
    
    // Polyphase IIR Half-Band Oversampling (Order 1 = 2^1 = 2x oversampling factor).
    // Provides >90 dB stopband rejection with minimal phase distortion and negligible CPU cost.
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

    juce::dsp::ProcessSpec filterSpec;
    filterSpec.sampleRate = sampleRate;
    filterSpec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    filterSpec.numChannels = 1;

    for (size_t ch = 0; ch < 2; ++ch) {
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

    // 1. Input Drive Scaling & Op-Amp Active Gain
    double alpha_in = (inGainParam != nullptr) ? static_cast<double>(*inGainParam) : 0.5; 
    double R_t1 = 4700.0;
    double R_t2 = 4700.0;
    double P_gain = 50000.0;
    double R_up_div = R_t1 + (1.0 - alpha_in) * P_gain;
    double R_dn_div = R_t2 + alpha_in * P_gain;
    double G_in = R_dn_div / (R_up_div + R_dn_div);

    constexpr double guitarPeakVolts = 0.1; 
    double activeOpAmpBoost = 1.0 + (alpha_in * 74.0); 
    double toVoltsScale = guitarPeakVolts * activeOpAmpBoost;

    buffer.applyGain (static_cast<float>(G_in));

    // 1b. Hardware DC-Blocker Filter (15 Hz) to eliminate soundcard/ADC offsets
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
    float pPlus = 0.0f, pMinus = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        float inS = buffer.getSample (0, i);
        if (inS > pPlus) pPlus = inS;
        if (inS < pMinus) pMinus = inS;
        scopeData.bufferIn[static_cast<size_t>(wIdx)] = inS;
        wIdx = (wIdx + 1) % ScopeData::bufferSize;
    }

    // 2. Pre-EQ Processing (Linear tone-shaping prior to saturation)
    for (int ch = 0; ch < maxChannels; ++ch) {
        float* chData = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i) {
            float s = chData[i];
            for (size_t b = 0; b < numEqBands; ++b) {
                s = preEqFilters[static_cast<size_t>(ch)][b].processSample (s);
            }
            chData[i] = s;
        }
    }

    // 3. Polyphase Half-Band Oversampling (2x Interpolation)
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

    // 4. Real-Time Rubber Zener Nonlinear Solver Loop
    for (size_t ch = 0; ch < osBlock.getNumChannels(); ++ch) {
        float* data = osBlock.getChannelPointer (ch);
        if (ch >= 2) continue; 
        for (size_t i = 0; i < osBlock.getNumSamples(); ++i) {
            double vin_volts = static_cast<double>(data[i]) * toVoltsScale; 
            
            double vout_volts = clippers[ch].processSample (
                vin_volts, R_in_Thevenin, dt,
                alpha_up, R_k_up, R_p_up, C_up, pos_up,
                alpha_dn, R_k_dn, R_p_dn, C_dn, pos_dn
            );
            
            constexpr double nominalCeilingVolts = 3.0; 
            data[i] = static_cast<float>(vout_volts / nominalCeilingVolts);
        }
    }

    // 5. Polyphase Half-Band Oversampling (2x Decimation)
    if (oversampler != nullptr) oversampler->processSamplesDown (block);

    // 6. Post-EQ Processing (Harmonic spectrum smoothing)
    for (int ch = 0; ch < maxChannels; ++ch) {
        float* chData = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i) {
            float s = chData[i];
            for (size_t b = 0; b < numEqBands; ++b) {
                s = postEqFilters[static_cast<size_t>(ch)][b].processSample (s);
            }
            chData[i] = s;
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
    scopeData.peakInPlus.store (pPlus, std::memory_order_relaxed);
    scopeData.peakInMinus.store (pMinus, std::memory_order_relaxed);
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
