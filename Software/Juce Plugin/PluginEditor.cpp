#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// PEDAL LOOK AND FEEL IMPLEMENTATION (DAVIES 1510 & MINI TOGGLE)
// ==============================================================================
PedalLookAndFeel::PedalLookAndFeel() {}

void PedalLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, const float rotaryStartAngle,
                                         const float rotaryEndAngle, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int> (x, y, width, height);
    int labelHeight = 20;
    auto knobArea = bounds.withTrimmedBottom (labelHeight).toFloat().reduced (3.0f);
    
    float radius = std::min (knobArea.getWidth(), knobArea.getHeight()) * 0.5f;
    float cx = knobArea.getCentreX();
    float cy = knobArea.getCentreY();
    
    g.setColour (juce::Colours::black.withAlpha (0.42f));
    g.fillEllipse (cx - radius + 2.0f, cy - radius + 3.5f, radius * 2.0f, radius * 2.0f);

    juce::ColourGradient skirtGrad (juce::Colour (0xff2b2e34), cx, cy - radius,
                                    juce::Colour (0xff0d0e11), cx, cy + radius, false);
    g.setGradientFill (skirtGrad);
    g.fillEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
    
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawEllipse (cx - radius + 0.5f, cy - radius + 0.5f, radius * 2.0f - 1.0f, radius * 2.0f - 1.0f, 1.0f);

    float innerR = radius * 0.77f;
    juce::ColourGradient headGrad (juce::Colour (0xff3b3f49), cx - innerR * 0.5f, cy - innerR,
                                   juce::Colour (0xff131417), cx + innerR * 0.5f, cy + innerR, false);
    g.setGradientFill (headGrad);
    g.fillEllipse (cx - innerR, cy - innerR, innerR * 2.0f, innerR * 2.0f);

    float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    float ridgeW = innerR * 0.54f;
    float ridgeL = innerR * 1.88f;

    juce::Path ridge;
    ridge.addRoundedRectangle (-ridgeW * 0.5f, -ridgeL * 0.5f, ridgeW, ridgeL, ridgeW * 0.35f);
    ridge.applyTransform (juce::AffineTransform::rotation (angle).translated (cx, cy));

    juce::ColourGradient ridgeGrad (juce::Colour (0xff4b515d), cx - ridgeW * 0.5f, cy,
                                    juce::Colour (0xff111215), cx + ridgeW * 0.5f, cy, false);
    g.setGradientFill (ridgeGrad);
    g.fillPath (ridge);

    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.strokePath (ridge, juce::PathStrokeType (1.0f));

    juce::Path pointer;
    float lineLength = innerR * 0.72f;
    pointer.startNewSubPath (0.0f, -innerR * 0.15f);
    pointer.lineTo (0.0f, -innerR * 0.15f - lineLength);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (cx, cy));

    g.setColour (juce::Colour (0xfff8fafc));
    g.strokePath (pointer, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (juce::Colour (0xff0f172a));
    g.setFont (juce::Font().withHeight (13.5f).withStyle (juce::Font::bold));
    g.drawFittedText (slider.getName(), bounds.getX(), bounds.getBottom() - labelHeight,
                      bounds.getWidth(), labelHeight, juce::Justification::centred, 1);
}

void PedalLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float, float, float,
                                         const juce::Slider::SliderStyle, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    float cx = bounds.getCentreX();
    bool isAttack = (slider.getValue() < 0.5);

    float labelH = 16.0f;
    auto topLabelRect    = bounds.removeFromTop (labelH);
    auto bottomLabelRect = bounds.removeFromBottom (labelH);

    g.setFont (juce::Font().withHeight (12.0f).withStyle (juce::Font::bold));
    g.setColour (isAttack ? juce::Colour (0xff0284c7) : juce::Colour (0xff64748b));
    g.drawFittedText ("ATTACK", topLabelRect.toNearestInt(), juce::Justification::centred, 1);

    g.setColour (!isAttack ? juce::Colour (0xffea580c) : juce::Colour (0xff64748b));
    g.drawFittedText ("BODY", bottomLabelRect.toNearestInt(), juce::Justification::centred, 1);

    float cy = bounds.getCentreY();
    float nutRadius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.28f;
    nutRadius = juce::jlimit (12.0f, 17.0f, nutRadius);

    juce::Path hexNut;
    for (int i = 0; i < 6; ++i)
    {
        float a = i * (juce::MathConstants<float>::twoPi / 6.0f);
        float px = cx + nutRadius * std::cos (a);
        float py = cy + nutRadius * std::sin (a);
        if (i == 0) hexNut.startNewSubPath (px, py);
        else        hexNut.lineTo (px, py);
    }
    hexNut.closeSubPath();

    g.setColour (juce::Colours::black.withAlpha (0.30f));
    g.fillPath (hexNut, juce::AffineTransform::translation (1.5f, 2.0f));

    juce::ColourGradient nutGrad (juce::Colour (0xffe2e8f0), cx - nutRadius, cy - nutRadius,
                                  juce::Colour (0xff64748b), cx + nutRadius, cy + nutRadius, false);
    g.setGradientFill (nutGrad);
    g.fillPath (hexNut);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.strokePath (hexNut, juce::PathStrokeType (1.0f));

    g.setColour (juce::Colour (0xff0f172a));
    g.fillEllipse (cx - nutRadius * 0.55f, cy - nutRadius * 0.55f, nutRadius * 1.1f, nutRadius * 1.1f);

    float leverLen = nutRadius * 1.35f;
    float tipY = isAttack ? cy - leverLen : cy + leverLen;
    float baseW = nutRadius * 0.52f;
    float tipW  = nutRadius * 0.38f;

    juce::Path lever;
    lever.startNewSubPath (cx - baseW * 0.5f, cy);
    lever.lineTo (cx - tipW * 0.5f, tipY);
    lever.lineTo (cx + tipW * 0.5f, tipY);
    lever.lineTo (cx + baseW * 0.5f, cy);
    lever.closeSubPath();

    juce::ColourGradient leverGrad (juce::Colours::white, cx - tipW, tipY,
                                    juce::Colour (0xff64748b), cx + tipW, cy, false);
    g.setGradientFill (leverGrad);
    g.fillPath (lever);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.strokePath (lever, juce::PathStrokeType (0.9f));

    g.setColour (juce::Colour (0xfff8fafc));
    g.fillEllipse (cx - tipW * 0.65f, tipY - tipW * 0.65f, tipW * 1.3f, tipW * 1.3f);
}

void PedalLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                             bool, bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (3.0f);
    float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    auto center = bounds.getCentre();

    juce::ColourGradient ringGrad (juce::Colour (0xffcbd5e1), center.x - radius, center.y - radius,
                                   juce::Colour (0xff475569), center.x + radius, center.y + radius, false);
    g.setGradientFill (ringGrad);
    g.fillEllipse (center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);

    float plungerR = radius * 0.72f;
    if (shouldDrawButtonAsDown) plungerR *= 0.94f;

    juce::ColourGradient plungerGrad (shouldDrawButtonAsDown ? juce::Colour (0xff64748b) : juce::Colour (0xfff1f5f9),
                                      center.x - plungerR, center.y - plungerR,
                                      juce::Colour (0xff334155),
                                      center.x + plungerR, center.y + plungerR, false);
    g.setGradientFill (plungerGrad);
    g.fillEllipse (center.x - plungerR, center.y - plungerR, plungerR * 2.0f, plungerR * 2.0f);

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawEllipse (center.x - plungerR, center.y - plungerR, plungerR * 2.0f, plungerR * 2.0f, 1.2f);
}

// ==============================================================================
// MODULAR FLAT GRAPHIC EQ
// ==============================================================================
GraphicEqComponent::GraphicEqComponent (juce::AudioProcessorValueTreeState& apvts,
                                        const juce::String& paramPrefix,
                                        const juce::String& title,
                                        juce::Colour accentColour)
    : eqTitle (title), accent (accentColour)
{
    for (size_t i = 0; i < GomuGomuNoDrive::numEqBands; ++i)
    {
        juce::String paramID = paramPrefix + juce::String (i);
        params[i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (paramID));
    }
}

int GraphicEqComponent::getBandIndexAt (float xPos, float totalWidth) const
{
    float bandWidth = totalWidth / static_cast<float> (GomuGomuNoDrive::numEqBands);
    return juce::jlimit (0, static_cast<int> (GomuGomuNoDrive::numEqBands) - 1, static_cast<int> (xPos / bandWidth));
}

void GraphicEqComponent::updateBandFromMouse (int bandIndex, float yPos, float trackTop, float trackHeight)
{
    if (!isEnabled() || bandIndex < 0 || bandIndex >= static_cast<int> (GomuGomuNoDrive::numEqBands) || params[static_cast<size_t> (bandIndex)] == nullptr)
        return;

    float norm = 1.0f - juce::jlimit (0.0f, 1.0f, (yPos - trackTop) / trackHeight);
    float gainDb = -15.0f + norm * 30.0f;
    
    auto* p = params[static_cast<size_t> (bandIndex)];
    p->setValueNotifyingHost (p->range.convertTo0to1 (gainDb));
}

void GraphicEqComponent::mouseDown (const juce::MouseEvent& e)
{
    if (!isEnabled()) return;
    auto bounds = getLocalBounds().toFloat();
    float trackTop = 22.0f;
    float trackHeight = bounds.getHeight() - trackTop - 18.0f;

    int band = getBandIndexAt (e.position.x, bounds.getWidth());
    updateBandFromMouse (band, e.position.y, trackTop, trackHeight);

    lastDraggedBand = band;
    lastDraggedGain = (params[static_cast<size_t> (band)] != nullptr) ? params[static_cast<size_t> (band)]->get() : 0.0f;
    repaint();
}

void GraphicEqComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (!isEnabled()) return;
    auto bounds = getLocalBounds().toFloat();
    float trackTop = 22.0f;
    float trackHeight = bounds.getHeight() - trackTop - 18.0f;

    int currentBand = getBandIndexAt (e.position.x, bounds.getWidth());
    float norm = 1.0f - juce::jlimit (0.0f, 1.0f, (e.position.y - trackTop) / trackHeight);
    float currentGain = -15.0f + norm * 30.0f;

    if (lastDraggedBand >= 0 && lastDraggedBand != currentBand)
    {
        int startB = std::min (lastDraggedBand, currentBand);
        int endB   = std::max (lastDraggedBand, currentBand);

        for (int b = startB; b <= endB; ++b)
        {
            float t = static_cast<float> (b - lastDraggedBand) / static_cast<float> (currentBand - lastDraggedBand);
            float interpGain = lastDraggedGain + t * (currentGain - lastDraggedGain);
            auto* p = params[static_cast<size_t> (b)];
            if (p != nullptr)
                p->setValueNotifyingHost (p->range.convertTo0to1 (interpGain));
        }
    }
    else
    {
        updateBandFromMouse (currentBand, e.position.y, trackTop, trackHeight);
    }

    lastDraggedBand = currentBand;
    lastDraggedGain = currentGain;
    repaint();
}

void GraphicEqComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (!isEnabled()) return;
    int band = getBandIndexAt (e.position.x, static_cast<float> (getWidth()));
    if (params[static_cast<size_t> (band)] != nullptr)
    {
        params[static_cast<size_t> (band)]->setValueNotifyingHost (
            params[static_cast<size_t> (band)]->range.convertTo0to1 (0.0f));
        repaint();
    }
}

void GraphicEqComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    float alphaMult = isEnabled() ? 1.0f : 0.35f;

    g.setColour (juce::Colour (0xff111317).withMultipliedAlpha (alphaMult));
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (juce::Colour (0xff252932).withMultipliedAlpha (alphaMult));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);

    g.setColour (accent.withAlpha (0.95f * alphaMult));
    g.setFont (juce::Font().withHeight (12.0f).withStyle (juce::Font::bold));
    g.drawText (eqTitle, bounds.removeFromTop (20.0f).reduced (8.0f, 0.0f), juce::Justification::centredLeft);

    float trackTop = 22.0f;
    float trackHeight = getHeight() - trackTop - 18.0f;
    float midY = trackTop + trackHeight * 0.5f;
    float bandWidth = static_cast<float> (getWidth()) / static_cast<float> (GomuGomuNoDrive::numEqBands);

    g.setColour (juce::Colours::white.withAlpha (0.10f * alphaMult));
    g.drawHorizontalLine (static_cast<int> (midY), 8.0f, static_cast<float> (getWidth()) - 8.0f);

    static const char* bandLabels[] = { "60", "100", "160", "250", "400", "630", "1k", "1.6k", "2.5k", "3.5k", "5k", "8k" };

    for (size_t b = 0; b < GomuGomuNoDrive::numEqBands; ++b)
    {
        float x = b * bandWidth + bandWidth * 0.5f;
        float val = (params[b] != nullptr) ? params[b]->get() : 0.0f;
        float norm = (val + 15.0f) / 30.0f;
        float yVal = trackTop + (1.0f - norm) * trackHeight;

        g.setColour (juce::Colour (0xff21252d).withMultipliedAlpha (alphaMult));
        g.fillRect (x - 1.5f, trackTop, 3.0f, trackHeight);

        g.setColour (accent.withAlpha (0.90f * alphaMult));
        if (std::abs (yVal - midY) > 1.0f)
        {
            float barY = std::min (yVal, midY);
            float barH = std::abs (yVal - midY);
            g.fillRect (x - 2.5f, barY, 5.0f, barH);
        }

        g.setColour (juce::Colours::white.withAlpha (alphaMult));
        g.fillRoundedRectangle (x - 6.0f, yVal - 2.0f, 12.0f, 4.0f, 1.5f);

        g.setColour (juce::Colour (0xff94a3b8).withMultipliedAlpha (alphaMult));
        g.setFont (juce::Font().withHeight (10.5f).withStyle (juce::Font::bold));
        g.drawText (bandLabels[b], static_cast<int> (x - bandWidth * 0.5f), getHeight() - 17,
                    static_cast<int> (bandWidth), 14, juce::Justification::centred);
    }

    if (!isEnabled()) {
        g.setColour (juce::Colour (0xff38ef7d).withAlpha (0.85f));
        g.setFont (juce::Font().withHeight (13.0f).withStyle (juce::Font::bold));
        g.drawFittedText ("VOICING ACTIVE — SELECT 'LAB / CUSTOM' TO TWEAK GRAPHIC EQ", 
                          getLocalBounds(), juce::Justification::centred, 1);
    }
}

// ==============================================================================
// CABINET BUTTON & LED
// ==============================================================================
void CabToggleButton::paintButton (juce::Graphics& g, bool, bool shouldDrawButtonAsDown)
{
    auto bounds = getLocalBounds().toFloat().reduced (shouldDrawButtonAsDown ? 2.5f : 1.5f);
    bool on = getToggleState();

    juce::Colour primary = on ? juce::Colour (0xffff9f24) : juce::Colour (0xff585d68);
    juce::Colour bg      = on ? juce::Colour (0xff261f17) : juce::Colour (0xff1a1b1e);

    g.setColour (bg);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (primary);
    g.drawRoundedRectangle (bounds, 4.0f, 1.5f);

    auto inner = bounds.reduced (4.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (inner, 2.5f);

    auto c = inner.getCentre();
    float rOuter = std::min (inner.getWidth(), inner.getHeight()) * 0.42f;
    float rCap   = rOuter * 0.35f;

    g.setColour (primary.withAlpha (on ? 0.35f : 0.15f));
    g.fillEllipse (c.x - rOuter, c.y - rOuter, rOuter * 2.0f, rOuter * 2.0f);

    g.setColour (primary);
    g.drawEllipse (c.x - rOuter, c.y - rOuter, rOuter * 2.0f, rOuter * 2.0f, 1.2f);
    g.fillEllipse (c.x - rCap, c.y - rCap, rCap * 2.0f, rCap * 2.0f);
}

void LedIndicator::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (2.0f);
    float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    auto center = bounds.getCentre();

    juce::ColourGradient bezel (juce::Colours::white, center.x - radius, center.y - radius,
                                juce::Colour (0xff334155), center.x + radius, center.y + radius, false);
    g.setGradientFill (bezel);
    g.fillEllipse (center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);

    float ledR = radius * 0.72f;
    if (isOn)
    {
        juce::ColourGradient glow (juce::Colours::red.withAlpha (0.80f), center.x, center.y,
                                   juce::Colours::red.withAlpha (0.0f), center.x + ledR * 2.2f, center.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (bounds.expanded (radius * 0.7f));

        juce::ColourGradient core (juce::Colours::white, center.x - ledR * 0.3f, center.y - ledR * 0.3f,
                                   juce::Colour (0xffdc2626), center.x + ledR, center.y + ledR, true);
        g.setGradientFill (core);
    }
    else
    {
        juce::ColourGradient core (juce::Colour (0xff450a0a), center.x - ledR * 0.3f, center.y - ledR * 0.3f,
                                   juce::Colour (0xff0f0202), center.x + ledR, center.y + ledR, true);
        g.setGradientFill (core);
    }

    g.fillEllipse (center.x - ledR, center.y - ledR, ledR * 2.0f, ledR * 2.0f);
}

static void renderCrtBeam (juce::Graphics& g, const juce::Path& path, juce::Colour glowColour, float baseWidth = 1.5f)
{
    g.setColour (glowColour.withAlpha (0.16f));
    g.strokePath (path, juce::PathStrokeType (baseWidth * 3.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (glowColour.withAlpha (0.65f));
    g.strokePath (path, juce::PathStrokeType (baseWidth * 1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (juce::Colours::white.withAlpha (0.95f));
    g.strokePath (path, juce::PathStrokeType (baseWidth * 0.70f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// ==============================================================================
// CATHODE RAY TUBE: STATIC I/V TRANSFER SCOPE
// ==============================================================================
void TransferCurveScope::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (juce::Colour (0xff181a1f));
    g.fillRoundedRectangle (bounds, 12.0f);
    g.setColour (juce::Colour (0xff2d313b));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 12.0f, 1.2f);

    auto screenArea = bounds.reduced (5.0f);
    juce::ColourGradient screenGrad (juce::Colour (0xff0a1614), screenArea.getCentreX(), screenArea.getY(),
                                     juce::Colour (0xff030606), screenArea.getCentreX(), screenArea.getBottom(), false);
    g.setGradientFill (screenGrad);
    g.fillRoundedRectangle (screenArea, 9.0f);

    float midX = screenArea.getCentreX();
    float midY = screenArea.getCentreY();

    g.setColour (juce::Colour (0xff14332b));
    for (int i = 1; i <= 5; ++i)
    {
        float dx = screenArea.getX() + screenArea.getWidth() * (i / 6.0f);
        float dy = screenArea.getY() + screenArea.getHeight() * (i / 6.0f);
        g.drawVerticalLine (static_cast<int>(dx), screenArea.getY(), screenArea.getBottom());
        g.drawHorizontalLine (static_cast<int>(dy), screenArea.getX(), screenArea.getRight());
    }

    g.setColour (juce::Colour (0xff1e4d41));
    g.drawVerticalLine (static_cast<int>(midX), screenArea.getY(), screenArea.getBottom());
    g.drawHorizontalLine (static_cast<int>(midY), screenArea.getX(), screenArea.getRight());

    g.setColour (juce::Colour (0xff38ef7d).withAlpha (0.85f));
    g.setFont (juce::Font().withHeight (11.0f).withStyle (juce::Font::bold));
    g.drawText ("TRANSFER CURVE (+/-10V)", screenArea.reduced (8.0f, 6.0f), juce::Justification::topLeft);

    float scaleX = screenArea.getWidth()  / 20.0f; 
    float scaleY = screenArea.getHeight() / 20.0f;

    double dt = 1.0 / (48000.0 * 2.0);
    double R_in = 10000.0;

    auto getP = [this](const char* id, float def) {
        auto* p = processor.apvts.getRawParameterValue (id);
        return p != nullptr ? static_cast<double>(p->load()) : static_cast<double>(def);
    };

    double alpha_up = juce::jlimit (0.0, 1.0, getP ("THRESH_P", 0.5f));
    double R_p_up   = std::max (getP ("PINCH_P", 1000.0f), 1.0);
    double R_k_up   = std::max (getP ("KNEE_P", 10.0f), 0.0);
    int pos_up      = static_cast<int>(getP ("CAP_POS_P", 0.0f));
    double C_up     = 1e-15;

    double alpha_dn = juce::jlimit (0.0, 1.0, getP ("THRESH_M", 0.5f));
    double R_p_dn   = std::max (getP ("PINCH_M", 1000.0f), 1.0);
    double R_k_dn   = std::max (getP ("KNEE_M", 10.0f), 0.0);
    int pos_dn      = static_cast<int>(getP ("CAP_POS_M", 0.0f));
    double C_dn     = 1e-15;

    juce::Path curve;
    RubberZener dummyZener;

    dummyZener.reset();
    for (int i = 0; i <= 100; ++i)
    {
        double vin = 10.0 * (i / 100.0);
        double vout = dummyZener.processSample (vin, R_in, dt, alpha_up, R_k_up, R_p_up, C_up, pos_up, alpha_dn, R_k_dn, R_p_dn, C_dn, pos_dn);
        float x = midX + static_cast<float>(vin) * scaleX;
        float y = midY - static_cast<float>(vout) * scaleY;
        if (i == 0) curve.startNewSubPath (x, y);
        else        curve.lineTo (x, y);
    }

    dummyZener.reset();
    juce::Path negCurve;
    for (int i = 0; i <= 100; ++i)
    {
        double vin = -10.0 * (i / 100.0);
        double vout = dummyZener.processSample (vin, R_in, dt, alpha_up, R_k_up, R_p_up, C_up, pos_up, alpha_dn, R_k_dn, R_p_dn, C_dn, pos_dn);
        float x = midX + static_cast<float>(vin) * scaleX;
        float y = midY - static_cast<float>(vout) * scaleY;
        if (i == 0) negCurve.startNewSubPath (x, y);
        else        negCurve.lineTo (x, y);
    }
    curve.addPath (negCurve);

    renderCrtBeam (g, curve, juce::Colour (0xff38ef7d), 1.6f);

    // Position réelle calculée en Volts selon le Drive et l'intensité d'attaque
    float vxP = juce::jlimit (0.0f, 10.0f, pPlus);
    float vxM = juce::jlimit (-10.0f, 0.0f, pMinus);

    dummyZener.reset();
    float voutPlus = static_cast<float>(dummyZener.processSample (vxP, R_in, dt, alpha_up, R_k_up, R_p_up, C_up, pos_up, alpha_dn, R_k_dn, R_p_dn, C_dn, pos_dn));
    dummyZener.reset();
    float voutMinus = static_cast<float>(dummyZener.processSample (vxM, R_in, dt, alpha_up, R_k_up, R_p_up, C_up, pos_up, alpha_dn, R_k_dn, R_p_dn, C_dn, pos_dn));

    auto drawDot = [&](float vx, float vy) {
        float x = midX + vx * scaleX;
        float y = midY - vy * scaleY;
        g.setColour (juce::Colour (0xff38ef7d).withAlpha (0.45f));
        g.fillEllipse (x - 6.0f, y - 6.0f, 12.0f, 12.0f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (x - 2.5f, y - 2.5f, 5.0f, 5.0f);
    };

    drawDot (vxP, voutPlus);
    drawDot (vxM, voutMinus);
}

// ==============================================================================
// CATHODE RAY TUBE: TIME DOMAIN (TRANSIENT) SCOPE
// ==============================================================================
void TimeDomainScope::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (juce::Colour (0xff181a1f));
    g.fillRoundedRectangle (bounds, 12.0f);
    g.setColour (juce::Colour (0xff2d313b));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 12.0f, 1.2f);

    auto screenArea = bounds.reduced (5.0f);
    juce::ColourGradient screenGrad (juce::Colour (0xff0a141a), screenArea.getCentreX(), screenArea.getY(),
                                     juce::Colour (0xff020508), screenArea.getCentreX(), screenArea.getBottom(), false);
    g.setGradientFill (screenGrad);
    g.fillRoundedRectangle (screenArea, 9.0f);

    float midY = screenArea.getCentreY();

    g.setColour (juce::Colour (0xff142b38));
    for (int i = 1; i <= 5; ++i)
    {
        float dx = screenArea.getX() + screenArea.getWidth() * (i / 6.0f);
        float dy = screenArea.getY() + screenArea.getHeight() * (i / 6.0f);
        g.drawVerticalLine (static_cast<int>(dx), screenArea.getY(), screenArea.getBottom());
        g.drawHorizontalLine (static_cast<int>(dy), screenArea.getX(), screenArea.getRight());
    }

    g.setColour (juce::Colour (0xff1d4054));
    g.drawHorizontalLine (static_cast<int>(midY), screenArea.getX(), screenArea.getRight());

    g.setFont (juce::Font().withHeight (11.0f).withStyle (juce::Font::bold));
    g.setColour (juce::Colour (0xff00f2fe).withAlpha (0.85f));
    g.drawText ("WAVEFORM MONITOR", screenArea.reduced (8.0f, 6.0f), juce::Justification::topLeft);

    float legY = screenArea.getY() + 7.0f;
    float legR = screenArea.getRight() - 10.0f;

    g.setColour (juce::Colour (0xffff5252));
    g.drawText ("CLIP", legR - 35.0f, legY, 35.0f, 12.0f, juce::Justification::centredLeft);
    g.fillEllipse (legR - 44.0f, legY + 2.5f, 6.5f, 6.5f);

    g.setColour (juce::Colour (0xff38ef7d));
    g.drawText ("IN", legR - 75.0f, legY, 24.0f, 12.0f, juce::Justification::centredLeft);
    g.fillEllipse (legR - 84.0f, legY + 2.5f, 6.5f, 6.5f);

    float scaleY = screenArea.getHeight() * 0.42f;

    juce::Path pathIn, pathOut;
    int readIdx = processor.scopeData.writeIndex.load (std::memory_order_relaxed);

    float sumIn = 0.0f, sumOut = 0.0f;
    for (int i = 0; i < ScopeData::bufferSize; ++i)
    {
        int idx = (readIdx + i) % ScopeData::bufferSize;
        sumIn  += processor.scopeData.bufferIn[static_cast<size_t>(idx)];
        sumOut += processor.scopeData.bufferOut[static_cast<size_t>(idx)];
    }
    float dcIn  = sumIn  / static_cast<float>(ScopeData::bufferSize);
    float dcOut = sumOut / static_cast<float>(ScopeData::bufferSize);

    for (int i = 0; i < ScopeData::bufferSize; ++i)
    {
        int idx = (readIdx + i) % ScopeData::bufferSize;
        float x = screenArea.getX() + screenArea.getWidth() * (i / static_cast<float>(ScopeData::bufferSize - 1));
        
        float rawIn  = processor.scopeData.bufferIn[static_cast<size_t>(idx)]  - dcIn;
        float rawOut = processor.scopeData.bufferOut[static_cast<size_t>(idx)] - dcOut;

        float yIn  = midY - juce::jlimit (-1.0f, 1.0f, rawIn) * scaleY;
        float yOut = midY - juce::jlimit (-1.0f, 1.0f, rawOut) * scaleY;

        if (i == 0) {
            pathIn.startNewSubPath (x, yIn);
            pathOut.startNewSubPath (x, yOut);
        } else {
            pathIn.lineTo (x, yIn);
            pathOut.lineTo (x, yOut);
        }
    }

    renderCrtBeam (g, pathIn, juce::Colour (0xff38ef7d), 1.4f);
    renderCrtBeam (g, pathOut, juce::Colour (0xffff4757), 1.5f);
}

// ==============================================================================
// MAIN EDITOR IMPLEMENTATION
// ==============================================================================
GomuGomuNoDriveEditor::GomuGomuNoDriveEditor (GomuGomuNoDrive& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
      preEqComponent (p.apvts, "PRE_EQ_", "PRE-EQ (INPUT SCULPTING)", juce::Colour (0xff0284c7)),
      postEqComponent (p.apvts, "POST_EQ_", "POST-EQ (TONE SHAPING)", juce::Colour (0xffea580c)),
      oscIU (p), oscTime (p)
{
    setSize (980, 840);
    setLookAndFeel (&customLookAndFeel);

    addAndMakeVisible (preEqComponent);
    addAndMakeVisible (postEqComponent);

    addAndMakeVisible (oscIU);
    addAndMakeVisible (oscTime);
    addAndMakeVisible (led);

    setupKnob (driveSlider, "DRIVE");
    setupKnob (outputSlider, "OUTPUT");
    setupKnob (toneSlider, "TONE");

    driveAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "GAIN", driveSlider);
    outputAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "OUT_GAIN", outputSlider);
    toneAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "TONE", toneSlider);

    // Voicing Selector
    addAndMakeVisible (voicingSelector);
    voicingSelector.addItem ("TS-VOICE", 1);
    voicingSelector.addItem ("RAT-VOICE", 2);
    voicingSelector.addItem ("MUFF-VOICE", 3);
    voicingSelector.addItem ("LAB / CUSTOM", 4);
    voicingAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "VOICING", voicingSelector);

    addAndMakeVisible (footswitch);
    footswitch.setButtonText ("");
    footswitch.setClickingTogglesState (true);
    footswitch.onClick = [this]() { led.setOn (!footswitch.getToggleState()); };

    // West Wing (UP / 西)
    setupKnob   (clipWestSlider,  "CLIP");
    setupKnob   (edgeWestSlider,  "EDGE");
    setupSwitch (capWestSwitch,   "VOICING");
    setupKnob   (kneeWestSlider,  "KNEE");
    setupKnob   (pinchWestSlider, "PINCH");

    clipWestAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "THRESH_P", clipWestSlider);
    capWestAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "CAP_POS_P", capWestSwitch);
    edgeWestAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "CAP_P", edgeWestSlider);
    kneeWestAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "KNEE_P", kneeWestSlider);
    pinchWestAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "PINCH_P", pinchWestSlider);

    // East Wing (DOWN / 東)
    setupKnob   (clipEastSlider,  "CLIP");
    setupKnob   (edgeEastSlider,  "EDGE");
    setupSwitch (capEastSwitch,   "VOICING");
    setupKnob   (kneeEastSlider,  "KNEE");
    setupKnob   (pinchEastSlider, "PINCH");

    clipEastAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "THRESH_M", clipEastSlider);
    capEastAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "CAP_POS_M", capEastSwitch);
    edgeEastAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "CAP_M", edgeEastSlider);
    kneeEastAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "KNEE_M", kneeEastSlider);
    pinchEastAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, "PINCH_M", pinchEastSlider);

    // Cabinet Section
    addAndMakeVisible (cabToggle);
    cabEnableAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "CAB_ENABLE", cabToggle);

    addAndMakeVisible (cabSelector);
    cabSelector.clear();
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        juce::String name = juce::String::fromUTF8 (BinaryData::originalFilenames[i]);
        if (name.endsWithIgnoreCase (".wav"))
            name = name.dropLastCharacters (4);
        cabSelector.addItem (name, i + 1);
    }
    cabSelectAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "CAB_SELECT", cabSelector);
    cabSelector.onChange = [this]() {
        audioProcessor.loadBundledIR (cabSelector.getSelectedItemIndex());
    };

    startTimerHz (30);
}

GomuGomuNoDriveEditor::~GomuGomuNoDriveEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void GomuGomuNoDriveEditor::timerCallback()
{
    float currentPlusV  = audioProcessor.scopeData.peakInVoltsPlus.load (std::memory_order_relaxed);
    float currentMinusV = audioProcessor.scopeData.peakInVoltsMinus.load (std::memory_order_relaxed);

    if (currentPlusV > smoothPeakPlus) smoothPeakPlus = currentPlusV;
    else smoothPeakPlus *= 0.85f;

    if (currentMinusV < smoothPeakMinus) smoothPeakMinus = currentMinusV;
    else smoothPeakMinus *= 0.85f;

    audioProcessor.scopeData.peakInVoltsPlus.store (0.0f, std::memory_order_relaxed);
    audioProcessor.scopeData.peakInVoltsMinus.store (0.0f, std::memory_order_relaxed);

    int voicingIdx = voicingSelector.getSelectedItemIndex();
    bool isLab = (voicingIdx == GomuGomuNoDrive::Voicing_Lab);
    if (preEqComponent.isEnabled() != isLab) {
        preEqComponent.setEnabled (isLab);
        postEqComponent.setEnabled (isLab);
        preEqComponent.repaint();
        postEqComponent.repaint();
    }

    oscIU.updatePeaks (smoothPeakPlus, smoothPeakMinus);
    oscIU.repaint();
    oscTime.repaint();
}

void GomuGomuNoDriveEditor::setupKnob (juce::Slider& slider, const juce::String& name)
{
    slider.setName (name);
    addAndMakeVisible (slider);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
}

void GomuGomuNoDriveEditor::setupSwitch (juce::Slider& slider, const juce::String& name)
{
    slider.setName (name);
    addAndMakeVisible (slider);
    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRange (0, 1, 1);
}

void GomuGomuNoDriveEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xffcbd0d8));

    auto area = getLocalBounds().reduced (12);
    area.removeFromTop (74);
    area.removeFromBottom (74);
    area.removeFromBottom (178);
    auto pedalArea = area;

    float wingW = pedalArea.getWidth() * 0.38f;
    auto westBounds   = pedalArea.removeFromLeft (wingW).toFloat().reduced (3.0f);
    auto eastBounds   = pedalArea.removeFromRight (wingW).toFloat().reduced (3.0f);
    auto centerBounds = pedalArea.toFloat().reduced (3.0f);

    // --- AILE OUEST (西) ---
    juce::ColourGradient westBg (juce::Colour (0xffdde4eb), westBounds.getX(), westBounds.getY(),
                                 juce::Colour (0xffbcc5cf), westBounds.getRight(), westBounds.getBottom(), false);
    g.setGradientFill (westBg);
    g.fillRoundedRectangle (westBounds, 8.0f);
    g.setColour (juce::Colour (0xff707e8f));
    g.drawRoundedRectangle (westBounds, 8.0f, 1.2f);

    g.setColour (juce::Colour (0xff334155).withAlpha (0.08f));
    g.setFont (juce::Font().withHeight (230.0f).withStyle (juce::Font::bold));
    g.drawText (juce::String::fromUTF8 ("西"), westBounds, juce::Justification::centred);

    g.setColour (juce::Colour (0xff0f172a));
    g.setFont (juce::Font().withHeight (15.5f).withStyle (juce::Font::bold));
    g.drawText (juce::String::fromUTF8 ("西 · UPPER CLIP"), westBounds.removeFromTop (26.0f), juce::Justification::centred);

    // --- AILE EST (東) ---
    juce::ColourGradient eastBg (juce::Colour (0xffebe4dc), eastBounds.getX(), eastBounds.getY(),
                                 juce::Colour (0xffcfc4b8), eastBounds.getRight(), eastBounds.getBottom(), false);
    g.setGradientFill (eastBg);
    g.fillRoundedRectangle (eastBounds, 8.0f);
    g.setColour (juce::Colour (0xff8f7e70));
    g.drawRoundedRectangle (eastBounds, 8.0f, 1.2f);

    g.setColour (juce::Colour (0xff7c2d12).withAlpha (0.07f));
    g.setFont (juce::Font().withHeight (230.0f).withStyle (juce::Font::bold));
    g.drawText (juce::String::fromUTF8 ("東"), eastBounds, juce::Justification::centred);

    g.setColour (juce::Colour (0xff27170a));
    g.setFont (juce::Font().withHeight (15.5f).withStyle (juce::Font::bold));
    g.drawText (juce::String::fromUTF8 ("東 · LOWER CLIP"), eastBounds.removeFromTop (26.0f), juce::Justification::centred);

    // --- LOGO CENTRAL ---
    float logoY = centerBounds.getY() + 112.0f;
    auto logoArea = juce::Rectangle<float> (centerBounds.getX(), logoY, centerBounds.getWidth(), 55.0f);

    g.setColour (juce::Colour (0xff0f172a));
    g.setFont (juce::Font().withHeight (20.0f).withStyle (juce::Font::bold));
    g.drawFittedText (juce::String::fromUTF8 ("ゴムゴムの DRIVE"), logoArea.toNearestInt(), juce::Justification::centred, 1);
    
    g.setColour (juce::Colour (0xffdc2626));
    g.setFont (juce::Font().withHeight (10.0f).withStyle (juce::Font::bold));
    g.drawText ("RUBBER ZENER TOPOLOGY", logoArea.translated (0.0f, 22.0f).toNearestInt(), juce::Justification::centred);
}

void GomuGomuNoDriveEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    preEqComponent.setBounds (area.removeFromTop (74));
    area.removeFromTop (6);

    postEqComponent.setBounds (area.removeFromBottom (74));
    area.removeFromBottom (6);

    auto bottomSection = area.removeFromBottom (174);
    area.removeFromBottom (6);

    int scopeWidth = 300;
    oscIU.setBounds (bottomSection.removeFromLeft (scopeWidth));
    oscTime.setBounds (bottomSection.removeFromRight (scopeWidth));

    auto centerBottom = bottomSection.reduced (8, 0);
    auto cabRow = centerBottom.removeFromTop (46);
    cabToggle.setBounds (cabRow.removeFromLeft (44).reduced (2));
    cabSelector.setBounds (cabRow.reduced (4, 7));

    auto switchArea = centerBottom.withSizeKeepingCentre (110, 72);
    led.setBounds (switchArea.removeFromLeft (26).withSizeKeepingCentre (20, 20));
    footswitch.setBounds (switchArea.withSizeKeepingCentre (72, 72));

    // Plateau central
    float wingWidth = area.getWidth() * 0.38f;
    auto westArea   = area.removeFromLeft (wingWidth).reduced (6, 4);
    auto eastArea   = area.removeFromRight (wingWidth).reduced (6, 4);
    auto centerArea = area.reduced (4);

    // 1. DRIVE Knob en haut
    driveSlider.setBounds (centerArea.removeFromTop (110).withSizeKeepingCentre (88, 108));
    centerArea.removeFromTop (55); // Espace Logo

    // 2. Rangée OUTPUT (gauche) & TONE (droite) côte à côte
    auto rowOutTone = centerArea.removeFromTop (110);
    int halfW = rowOutTone.getWidth() / 2;
    outputSlider.setBounds (rowOutTone.removeFromLeft (halfW).withSizeKeepingCentre (82, 102));
    toneSlider.setBounds   (rowOutTone.withSizeKeepingCentre (82, 102));

    // 3. Sélecteur VOICING sous OUTPUT & TONE
    centerArea.removeFromTop (10);
    voicingSelector.setBounds (centerArea.removeFromTop (32).withSizeKeepingCentre (170, 28));

    // --- AILE OUEST (西) ---
    westArea.removeFromTop (26);
    auto westRow1 = westArea.removeFromTop (135);
    clipWestSlider.setBounds (westRow1.withSizeKeepingCentre (106, 126));

    auto westRow2 = westArea.removeFromTop (125);
    int colW2_W = westRow2.getWidth() / 2;
    edgeWestSlider.setBounds (westRow2.removeFromLeft (colW2_W).withSizeKeepingCentre (82, 102));
    capWestSwitch.setBounds  (westRow2.withSizeKeepingCentre (80, 95));

    auto westRow3 = westArea.removeFromTop (125);
    int colW3_W = westRow3.getWidth() / 2;
    kneeWestSlider.setBounds  (westRow3.removeFromLeft (colW3_W).withSizeKeepingCentre (82, 102));
    pinchWestSlider.setBounds (westRow3.withSizeKeepingCentre (82, 102));

    // --- AILE EST (東) ---
    eastArea.removeFromTop (26);
    auto eastRow1 = eastArea.removeFromTop (135);
    clipEastSlider.setBounds (eastRow1.withSizeKeepingCentre (106, 126));

    auto eastRow2 = eastArea.removeFromTop (125);
    int colW2_E = eastRow2.getWidth() / 2;
    edgeEastSlider.setBounds (eastRow2.removeFromLeft (colW2_E).withSizeKeepingCentre (82, 102));
    capEastSwitch.setBounds  (eastRow2.withSizeKeepingCentre (80, 95));

    auto eastRow3 = eastArea.removeFromTop (125);
    int colW3_E = eastRow3.getWidth() / 2;
    kneeEastSlider.setBounds  (eastRow3.removeFromLeft (colW3_E).withSizeKeepingCentre (82, 102));
    pinchEastSlider.setBounds (eastRow3.withSizeKeepingCentre (82, 102));
}
