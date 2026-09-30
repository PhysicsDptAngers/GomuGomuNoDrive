#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// ==============================================================================
// CUSTOM LOOK AND FEEL (DAVIES 1510 KNOBS, MINI TOGGLES & ILLUMINATED SWITCHES)
// ==============================================================================
class PedalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PedalLookAndFeel();

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, const float rotaryStartAngle,
                           const float rotaryEndAngle, juce::Slider& slider) override;

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle, juce::Slider& slider) override;

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

// ==============================================================================
// FLAT ROUTING TEXT BUTTON (NO 3D BOX, YELLOW ACTIVE / GRAY INACTIVE)
// ==============================================================================
class RoutingButton : public juce::Button
{
public:
    RoutingButton (const juce::String& name) : juce::Button (name) {}
    void paintButton (juce::Graphics& g, bool isMouseOver, bool shouldDrawButtonAsDown) override;
    void setActive (bool active) { isActive = active; repaint(); }
    bool getActive() const noexcept { return isActive; }
private:
    bool isActive = false;
};

// ==============================================================================
// FLAT GRAPHIC EQ
// ==============================================================================
class GraphicEqComponent : public juce::Component
{
public:
    GraphicEqComponent (juce::AudioProcessorValueTreeState& apvts,
                        const juce::String& paramPrefix,
                        const juce::String& title,
                        juce::Colour accentColour);

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    juce::String eqTitle;
    juce::Colour accent;
    std::array<juce::AudioParameterFloat*, GomuGomuNoDrive::numEqBands> params {};
    int lastDraggedBand = -1;
    float lastDraggedGain = 0.0f;

    void updateBandFromMouse (int bandIndex, float yPos, float trackTop, float trackHeight);
    int getBandIndexAt (float xPos, float totalWidth) const;
};

// ==============================================================================
// CABINET BUTTON & LED INDICATORS
// ==============================================================================
class CabToggleButton : public juce::Button
{
public:
    CabToggleButton() : juce::Button ("CabToggle") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

class LedIndicator : public juce::Component
{
public:
    void setColour (juce::Colour c) { ledColour = c; repaint(); }
    void setOn (bool shouldBeOn) { isOn = shouldBeOn; repaint(); }
    void paint (juce::Graphics& g) override;
private: 
    bool isOn = true;
    juce::Colour ledColour { juce::Colours::red };
};

// ==============================================================================
// CATHODE RAY TUBE (CRT) SCOPES (SOFT ET HARD)
// ==============================================================================
class TransferCurveScope : public juce::Component
{
public:
    TransferCurveScope (GomuGomuNoDrive& p, bool isPedalA) : processor (p), isA (isPedalA) {}
    void updatePeaks (float peakPlus, float peakMinus) { pPlus = peakPlus; pMinus = peakMinus; }
    void paint (juce::Graphics& g) override;
private:
    GomuGomuNoDrive& processor;
    bool isA = true;
    float pPlus = 0.0f, pMinus = 0.0f;
};

class TimeDomainScope : public juce::Component
{
public:
    TimeDomainScope (GomuGomuNoDrive& p, bool isPedalA) : processor (p), isA (isPedalA) {}
    void paint (juce::Graphics& g) override;
private:
    GomuGomuNoDrive& processor;
    bool isA = true;
};

// ==============================================================================
// MAIN EDITOR
// ==============================================================================
class GomuGomuNoDriveEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    GomuGomuNoDriveEditor (GomuGomuNoDrive&);
    ~GomuGomuNoDriveEditor() override;
    
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    GomuGomuNoDrive& audioProcessor;
    PedalLookAndFeel customLookAndFeel;

    // Barre supérieure de routage sans bordure
    RoutingButton btnRoutingAB   { "A -> B  (SERIAL)" };
    RoutingButton btnRoutingBA   { "B -> A  (SERIAL)" };
    RoutingButton btnRoutingPara { "A // B  (PARALLEL)" };
    juce::Slider interCutSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> interCutAtt;

    GraphicEqComponent preEqComponent;
    GraphicEqComponent postEqComponent;

    // Scopes dédiés
    TransferCurveScope oscIU_A;
    TimeDomainScope oscTime_A;
    TransferCurveScope oscIU_B;
    TimeDomainScope oscTime_B;

    // --- PÉDALE A (SOFT CLIPPING) ---
    juce::Slider driveASlider, levelASlider, toneASlider;
    juce::ComboBox voicingASelector;
    juce::Slider clipAUpSlider, edgeAUpSlider, capAUpSwitch;
    juce::Slider clipADnSlider, edgeADnSlider, capADnSwitch;
    juce::TextButton footswitchA;
    LedIndicator ledA;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> footAAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAAtt, levelAAtt, toneAAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> voicingAAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> clipAUpAtt, edgeAUpAtt, capAUpAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> clipADnAtt, edgeADnAtt, capADnAtt;

    // --- PÉDALE B (HARD CLIPPING) ---
    juce::Slider driveBSlider, levelBSlider, toneBSlider;
    juce::ComboBox voicingBSelector;
    juce::Slider clipBUpSlider, edgeBUpSlider, capBUpSwitch, kneeBUpSlider, pinchBUpSlider;
    juce::Slider clipBDnSlider, edgeBDnSlider, capBDnSwitch, kneeBDnSlider, pinchBDnSlider;
    juce::TextButton footswitchB;
    LedIndicator ledB;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> footBAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveBAtt, levelBAtt, toneBAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> voicingBAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> clipBUpAtt, edgeBUpAtt, capBUpAtt, kneeBUpAtt, pinchBUpAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> clipBDnAtt, edgeBDnAtt, capBDnAtt, kneeBDnAtt, pinchBDnAtt;

    // Section Cab
    CabToggleButton cabToggle;
    juce::ComboBox cabSelector;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> cabEnableAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cabSelectAtt;

    float smoothPeakA_Plus = 0.0f, smoothPeakA_Minus = 0.0f;
    float smoothPeakB_Plus = 0.0f, smoothPeakB_Minus = 0.0f;

    void setupKnob (juce::Slider& slider, const juce::String& name);
    void setupSwitch (juce::Slider& slider, const juce::String& name);
    void updateRoutingButtons (int mode);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GomuGomuNoDriveEditor)
};
