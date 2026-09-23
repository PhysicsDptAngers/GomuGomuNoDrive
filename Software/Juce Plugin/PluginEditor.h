#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// ==============================================================================
// CUSTOM LOOK AND FEEL (DAVIES 1510 KNOBS & MINI TOGGLE SWITCHES)
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
// MODULAR FLAT GRAPHIC EQ
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
// VECTOR SPEAKER CABINET BUTTON
// ==============================================================================
class CabToggleButton : public juce::Button
{
public:
    CabToggleButton() : juce::Button ("CabToggle") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

// ==============================================================================
// CHROME BEZEL LED INDICATOR
// ==============================================================================
class LedIndicator : public juce::Component
{
public:
    void setOn (bool shouldBeOn) { isOn = shouldBeOn; repaint(); }
    void paint (juce::Graphics& g) override;
private: 
    bool isOn = true;
};

// ==============================================================================
// CATHODE RAY TUBE (CRT) SCOPES WITH PHOSPHOR BEAM GLOW
// ==============================================================================
class TransferCurveScope : public juce::Component
{
public:
    TransferCurveScope (GomuGomuNoDrive& p) : processor (p) {}
    void updatePeaks (float peakPlus, float peakMinus) { pPlus = peakPlus; pMinus = peakMinus; }
    void paint (juce::Graphics& g) override;
private:
    GomuGomuNoDrive& processor;
    float pPlus = 0.0f, pMinus = 0.0f;
};

class TimeDomainScope : public juce::Component
{
public:
    TimeDomainScope (GomuGomuNoDrive& p) : processor (p) {}
    void paint (juce::Graphics& g) override;
private:
    GomuGomuNoDrive& processor;
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

    GraphicEqComponent preEqComponent;
    GraphicEqComponent postEqComponent;

    TransferCurveScope oscIU;
    TimeDomainScope oscTime;
    LedIndicator led;

    // Commandes centrales
    juce::Slider driveSlider, outputSlider, toneSlider;
    juce::ComboBox voicingSelector;
    juce::TextButton footswitch;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAtt, outputAtt, toneAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> voicingAtt;

    // Aile Ouest (UP / 西)
    juce::Slider clipWestSlider, edgeWestSlider, kneeWestSlider, pinchWestSlider;
    juce::Slider capWestSwitch; 
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> clipWestAtt, edgeWestAtt, kneeWestAtt, pinchWestAtt, capWestAtt;

    // Aile Est (DOWN / 東)
    juce::Slider clipEastSlider, edgeEastSlider, kneeEastSlider, pinchEastSlider;
    juce::Slider capEastSwitch;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> clipEastAtt, edgeEastAtt, kneeEastAtt, pinchEastAtt, capEastAtt;

    // Section Cab IR
    CabToggleButton cabToggle;
    juce::ComboBox cabSelector;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> cabEnableAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cabSelectAtt;

    float smoothPeakPlus = 0.0f, smoothPeakMinus = 0.0f;

    void setupKnob (juce::Slider& slider, const juce::String& name);
    void setupSwitch (juce::Slider& slider, const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GomuGomuNoDriveEditor)
};
