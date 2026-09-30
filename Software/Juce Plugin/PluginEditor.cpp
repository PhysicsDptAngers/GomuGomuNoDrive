#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// CRT PHOSPHOR HELPER
// ==============================================================================
static void renderCrtBeam (juce::Graphics& g, const juce::Path& path, juce::Colour glowColour, float baseWidth = 1.4f)
{
    g.setColour (glowColour.withAlpha (0.16f));
    g.strokePath (path, juce::PathStrokeType (baseWidth * 3.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (glowColour.withAlpha (0.65f));
    g.strokePath (path, juce::PathStrokeType (baseWidth * 1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colours::white.withAlpha (0.95f));
    g.strokePath (path, juce::PathStrokeType (baseWidth * 0.70f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// ==============================================================================
// ROUTING BUTTON IMPLEMENTATION
// ==============================================================================
void RoutingButton::paintButton (juce::Graphics& g, bool isMouseOver, bool)
{
    g.setFont (juce::Font (juce::FontOptions (13.5f, isActive ? juce::Font::bold : juce::Font::plain)));
    if (isActive)
        g.setColour (juce::Colour (0xfffacc15));
    else
        g.setColour (isMouseOver ? juce::Colours::white : juce::Colour (0xff94a3b8));

    g.drawFittedText (getName(), getLocalBounds(), juce::Justification::centred, 1);
}

// ==============================================================================
// LOOK AND FEEL IMPLEMENTATION
// ==============================================================================
PedalLookAndFeel::PedalLookAndFeel() {}

void PedalLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, const float rotaryStartAngle,
                                         const float rotaryEndAngle, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int> (x, y, width, height);
    int labelHeight = 18;
    auto knobArea = bounds.withTrimmedBottom (labelHeight).toFloat().reduced (2.0f);
    
    float radius = std::min (knobArea.getWidth(), knobArea.getHeight()) * 0.5f;
    float cx = knobArea.getCentreX();
    float cy = knobArea.getCentreY();
    
    g.setColour (juce::Colours::black.withAlpha (0.42f));
    g.fillEllipse (cx - radius + 2.0f, cy - radius + 3.0f, radius * 2.0f, radius * 2.0f);

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

    juce::Path pointer;
    float lineLength = innerR * 0.72f;
    pointer.startNewSubPath (0.0f, -innerR * 0.15f);
    pointer.lineTo (0.0f, -innerR * 0.15f - lineLength);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (cx, cy));

    g.setColour (juce::Colour (0xfff8fafc));
    g.strokePath (pointer, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (juce::Colour (0xff0f172a));
    g.setFont (juce::Font (juce::FontOptions (12.0f)).boldened());
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

    float labelH = 14.0f;
    auto topLabelRect    = bounds.removeFromTop (labelH);
    auto bottomLabelRect = bounds.removeFromBottom (labelH);

    g.setFont (juce::Font (juce::FontOptions (10.5f)).boldened());
    g.setColour (isAttack ? juce::Colour (0xff0284c7) : juce::Colour (0xff64748b));
    g.drawFittedText ("ATK", topLabelRect.toNearestInt(), juce::Justification::centred, 1);

    g.setColour (!isAttack ? juce::Colour (0xffea580c) : juce::Colour (0xff64748b));
    g.drawFittedText ("BDY", bottomLabelRect.toNearestInt(), juce::Justification::centred, 1);

    float cy = bounds.getCentreY();
    float nutRadius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.28f;
    nutRadius = juce::jlimit (10.0f, 15.0f, nutRadius);

    juce::Path hexNut;
    for (int i = 0; i < 6; ++i) {
        float a = i * (juce::MathConstants<float>::twoPi / 6.0f);
        float px = cx + nutRadius * std::cos (a);
        float py = cy + nutRadius * std::sin (a);
        if (i == 0) hexNut.startNewSubPath (px, py); else hexNut.lineTo (px, py);
    }
    hexNut.closeSubPath();

    juce::ColourGradient nutGrad (juce::Colour (0xffe2e8f0), cx - nutRadius, cy - nutRadius,
                                  juce::Colour (0xff64748b), cx + nutRadius, cy + nutRadius, false);
    g.setGradientFill (nutGrad);
    g.fillPath (hexNut);

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

    g.setColour (juce::Colour (0xffcbd5e1));
    g.fillPath (lever);
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
    bool isEngaged = button.getToggleState();
    if (shouldDrawButtonAsDown || isEngaged) plungerR *= 0.94f;

    juce::ColourGradient plungerGrad ((shouldDrawButtonAsDown || isEngaged) ? juce::Colour (0xff64748b) : juce::Colour (0xfff1f5f9),
                                      center.x - plungerR, center.y - plungerR,
                                      juce::Colour (0xff334155),
                                      center.x + plungerR, center.y + plungerR, false);
    g.setGradientFill (plungerGrad);
    g.fillEllipse (center.x - plungerR, center.y - plungerR, plungerR * 2.0f, plungerR * 2.0f);

    if (isEngaged) {
        g.setColour (juce::Colour (0xff38bdf8).withAlpha (0.4f));
        g.drawEllipse (center.x - radius + 1.0f, center.y - radius + 1.0f, radius * 2.0f - 2.0f, radius * 2.0f - 2.0f, 1.5f);
    }
}

// ==============================================================================
// GRAPHIC EQ & LED IMPLEMENTATION
// ==============================================================================
GraphicEqComponent::GraphicEqComponent (juce::AudioProcessorValueTreeState& apvts,
                                        const juce::String& paramPrefix,
                                        const juce::String& title,
                                        juce::Colour accentColour)
    : eqTitle (title), accent (accentColour)
{
    for (size_t i = 0; i < GomuGomuNoDrive::numEqBands; ++i) {
        params[i] = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (paramPrefix + juce::String (i)));
    }
}

int GraphicEqComponent::getBandIndexAt (float xPos, float totalWidth) const {
    float bandWidth = totalWidth / static_cast<float> (GomuGomuNoDrive::numEqBands);
    return juce::jlimit (0, static_cast<int> (GomuGomuNoDrive::numEqBands) - 1, static_cast<int> (xPos / bandWidth));
}

void GraphicEqComponent::updateBandFromMouse (int bandIndex, float yPos, float trackTop, float trackHeight) {
    if (!isEnabled() || bandIndex < 0 || bandIndex >= static_cast<int> (GomuGomuNoDrive::numEqBands) || params[static_cast<size_t> (bandIndex)] == nullptr) return;
    float norm = 1.0f - juce::jlimit (0.0f, 1.0f, (yPos - trackTop) / trackHeight);
    float gainDb = -15.0f + norm * 30.0f;
    auto* currentParam = params[static_cast<size_t> (bandIndex)];
    currentParam->setValueNotifyingHost (currentParam->range.convertTo0to1 (gainDb));
}

void GraphicEqComponent::mouseDown (const juce::MouseEvent& e) {
    if (!isEnabled()) return;
    float trackTop = 20.0f;
    float trackHeight = getHeight() - trackTop - 16.0f;
    int band = getBandIndexAt (e.position.x, (float)getWidth());
    updateBandFromMouse (band, e.position.y, trackTop, trackHeight);
    lastDraggedBand = band;
    lastDraggedGain = params[static_cast<size_t> (band)] != nullptr ? params[static_cast<size_t> (band)]->get() : 0.0f;
    repaint();
}

void GraphicEqComponent::mouseDrag (const juce::MouseEvent& e) {
    if (!isEnabled()) return;
    float trackTop = 20.0f;
    float trackHeight = getHeight() - trackTop - 16.0f;
    int currentBand = getBandIndexAt (e.position.x, (float)getWidth());
    float norm = 1.0f - juce::jlimit (0.0f, 1.0f, (e.position.y - trackTop) / trackHeight);
    float currentGain = -15.0f + norm * 30.0f;

    if (lastDraggedBand >= 0 && lastDraggedBand != currentBand) {
        int startB = std::min (lastDraggedBand, currentBand);
        int endB   = std::max (lastDraggedBand, currentBand);
        for (int b = startB; b <= endB; ++b) {
            float t = (float)(b - lastDraggedBand) / (float)(currentBand - lastDraggedBand);
            float interp = lastDraggedGain + t * (currentGain - lastDraggedGain);
            if (params[static_cast<size_t> (b)] != nullptr)
                params[static_cast<size_t> (b)]->setValueNotifyingHost (params[static_cast<size_t> (b)]->range.convertTo0to1 (interp));
        }
    } else {
        updateBandFromMouse (currentBand, e.position.y, trackTop, trackHeight);
    }
    lastDraggedBand = currentBand;
    lastDraggedGain = currentGain;
    repaint();
}

void GraphicEqComponent::mouseDoubleClick (const juce::MouseEvent& e) {
    if (!isEnabled()) return;
    int band = getBandIndexAt (e.position.x, (float)getWidth());
    if (params[static_cast<size_t> (band)] != nullptr) {
        params[static_cast<size_t> (band)]->setValueNotifyingHost (params[static_cast<size_t> (band)]->range.convertTo0to1 (0.0f));
        repaint();
    }
}

void GraphicEqComponent::paint (juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    float alpha = isEnabled() ? 1.0f : 0.35f;

    g.setColour (juce::Colour (0xff111317).withMultipliedAlpha (alpha));
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (juce::Colour (0xff252932).withMultipliedAlpha (alpha));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);

    g.setColour (accent.withAlpha (0.95f * alpha));
    g.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
    g.drawText (eqTitle, bounds.removeFromTop (18.0f).reduced (8.0f, 0.0f), juce::Justification::centredLeft);

    float trackTop = 18.0f;
    float trackHeight = getHeight() - trackTop - 15.0f;
    float midY = trackTop + trackHeight * 0.5f;
    float bandWidth = (float)getWidth() / (float)GomuGomuNoDrive::numEqBands;

    g.setColour (juce::Colours::white.withAlpha (0.10f * alpha));
    g.drawHorizontalLine ((int)midY, 6.0f, (float)getWidth() - 6.0f);

    static const char* labels[] = { "60", "100", "160", "250", "400", "630", "1k", "1.6k", "2.5k", "3.5k", "5k", "8k" };
    for (size_t b = 0; b < GomuGomuNoDrive::numEqBands; ++b) {
        float x = b * bandWidth + bandWidth * 0.5f;
        float val = params[b] != nullptr ? params[b]->get() : 0.0f;
        float norm = (val + 15.0f) / 30.0f;
        float yVal = trackTop + (1.0f - norm) * trackHeight;

        g.setColour (accent.withAlpha (0.90f * alpha));
        if (std::abs (yVal - midY) > 1.0f) {
            g.fillRect (x - 2.0f, std::min (yVal, midY), 4.0f, std::abs (yVal - midY));
        }
        g.setColour (juce::Colours::white.withAlpha (alpha));
        g.fillRoundedRectangle (x - 5.0f, yVal - 2.0f, 10.0f, 4.0f, 1.0f);

        g.setColour (juce::Colour (0xff94a3b8).withMultipliedAlpha (alpha));
        g.setFont (juce::Font (juce::FontOptions (9.5f)).boldened());
        g.drawText (labels[b], (int)(x - bandWidth * 0.5f), getHeight() - 14, (int)bandWidth, 12, juce::Justification::centred);
    }
}

void CabToggleButton::paintButton (juce::Graphics& g, bool, bool shouldDrawButtonAsDown) {
    auto bounds = getLocalBounds().toFloat().reduced (shouldDrawButtonAsDown ? 2.0f : 1.0f);
    bool on = getToggleState();
    juce::Colour primary = on ? juce::Colour (0xffff9f24) : juce::Colour (0xff585d68);
    g.setColour (on ? juce::Colour (0xff261f17) : juce::Colour (0xff1a1b1e));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (primary);
    g.drawRoundedRectangle (bounds, 4.0f, 1.2f);
}

void LedIndicator::paint (juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat().reduced (2.0f);
    float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    auto center = bounds.getCentre();

    juce::ColourGradient bezel (juce::Colours::white, center.x - radius, center.y - radius,
                                juce::Colour (0xff334155), center.x + radius, center.y + radius, false);
    g.setGradientFill (bezel);
    g.fillEllipse (center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);

    float ledR = radius * 0.72f;
    if (isOn) {
        juce::ColourGradient glow (ledColour.withAlpha (0.85f), center.x, center.y,
                                   ledColour.withAlpha (0.0f), center.x + ledR * 2.2f, center.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (bounds.expanded (radius * 0.7f));
        g.setColour (juce::Colours::white);
        g.fillEllipse (center.x - ledR * 0.35f, center.y - ledR * 0.35f, ledR * 0.7f, ledR * 0.7f);
    } else {
        g.setColour (ledColour.darker (0.85f));
        g.fillEllipse (center.x - ledR, center.y - ledR, ledR * 2.0f, ledR * 2.0f);
    }
}

// ==============================================================================
// CRT SCOPES
// ==============================================================================
void TransferCurveScope::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (juce::Colour (0xff181a1f));
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (juce::Colour (0xff2d313b));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

    auto screenArea = bounds.reduced (4.0f);
    juce::ColourGradient screenGrad (juce::Colour (0xff0a1614), screenArea.getCentreX(), screenArea.getY(),
                                     juce::Colour (0xff030606), screenArea.getCentreX(), screenArea.getBottom(), false);
    g.setGradientFill (screenGrad);
    g.fillRoundedRectangle (screenArea, 6.0f);

    float midX = screenArea.getCentreX();
    float midY = screenArea.getCentreY();

    g.setColour (juce::Colour (0xff14332b));
    for (int i = 1; i <= 5; ++i) {
        float dx = screenArea.getX() + screenArea.getWidth() * (i / 6.0f);
        float dy = screenArea.getY() + screenArea.getHeight() * (i / 6.0f);
        g.drawVerticalLine ((int)dx, screenArea.getY(), screenArea.getBottom());
        g.drawHorizontalLine ((int)dy, screenArea.getX(), screenArea.getRight());
    }

    g.setColour (juce::Colour (0xff1e4d41));
    g.drawVerticalLine ((int)midX, screenArea.getY(), screenArea.getBottom());
    g.drawHorizontalLine ((int)midY, screenArea.getX(), screenArea.getRight());

    g.setColour (juce::Colour (0xff38ef7d).withAlpha (0.9f));
    g.setFont (juce::Font (juce::FontOptions (9.5f)).boldened());
    g.drawText (isA ? "TRANSFER A (SOFT)" : "TRANSFER B (HARD)", screenArea.reduced (6.0f, 3.0f), juce::Justification::topLeft);

    double dt = 1.0 / (48000.0 * 2.0);
    juce::Path curve;

    if (isA)
    {
        auto getPA = [this](const char* id, float def) {
            auto* p = processor.apvts.getRawParameterValue (id);
            return p != nullptr ? static_cast<double>(p->load()) : static_cast<double>(def);
        };

        double driveAlpha = juce::jlimit (0.0, 1.0, getPA ("A_DRIVE", 0.5f));
        double RF_A       = 4700.0 + 50000.0 * (1.0 + 9.0 * std::pow (driveAlpha, 1.6));
        double alpha_up   = juce::jlimit (0.0, 1.0, getPA ("A_THRESH_P", 0.5f));
        int pos_up        = static_cast<int>(getPA ("A_CAP_POS_P", 0.0f));
        double alpha_dn   = juce::jlimit (0.0, 1.0, getPA ("A_THRESH_M", 0.5f));
        int pos_dn        = static_cast<int>(getPA ("A_CAP_POS_M", 0.0f));
        double C_dummy    = 1e-15;

        // Échelle adaptée à l'amplitude instrumentale de l'AOP
        float scaleX = screenArea.getWidth()  / 3.0f;
        float scaleY = screenArea.getHeight() / 4.2f;

        RubberZenerSoft dummySoft;
        dummySoft.reset();

        for (int i = 0; i <= 80; ++i) {
            double vin = 1.4 * (i / 80.0);
            double vout = dummySoft.processSample (vin, dt, alpha_up, C_dummy, pos_up, alpha_dn, C_dummy, pos_dn, RF_A);
            float x = midX + static_cast<float>(vin) * scaleX;
            float y = midY - static_cast<float>(vout) * scaleY;
            if (i == 0) curve.startNewSubPath (x, y);
            else        curve.lineTo (x, y);
        }

        dummySoft.reset();
        juce::Path negCurve;
        for (int i = 0; i <= 80; ++i) {
            double vin = -1.4 * (i / 80.0);
            double vout = dummySoft.processSample (vin, dt, alpha_up, C_dummy, pos_up, alpha_dn, C_dummy, pos_dn, RF_A);
            float x = midX + static_cast<float>(vin) * scaleX;
            float y = midY - static_cast<float>(vout) * scaleY;
            if (i == 0) negCurve.startNewSubPath (x, y);
            else        negCurve.lineTo (x, y);
        }
        curve.addPath (negCurve);
    }
    else
    {
        auto getPB = [this](const char* id, float def) {
            auto* p = processor.apvts.getRawParameterValue (id);
            return p != nullptr ? static_cast<double>(p->load()) : static_cast<double>(def);
        };

        double alpha_up = juce::jlimit (0.0, 1.0, getPB ("B_THRESH_P", 0.5f));
        double R_p_up   = std::max (getPB ("B_PINCH_P", 1000.0f), 1.0);
        double R_k_up   = std::max (getPB ("B_KNEE_P", 10.0f), 0.0);
        int pos_up      = static_cast<int>(getPB ("B_CAP_POS_P", 0.0f));

        double alpha_dn = juce::jlimit (0.0, 1.0, getPB ("B_THRESH_M", 0.5f));
        double R_p_dn   = std::max (getPB ("B_PINCH_M", 1000.0f), 1.0);
        double R_k_dn   = std::max (getPB ("B_KNEE_M", 10.0f), 0.0);
        int pos_dn      = static_cast<int>(getPB ("B_CAP_POS_M", 0.0f));

        double C_dummy  = 1e-15;
        double R_in     = 10000.0;

        float scaleX = screenArea.getWidth()  / 20.0f; 
        float scaleY = screenArea.getHeight() / 20.0f;

        RubberZenerHard dummyHard;
        dummyHard.reset();

        for (int i = 0; i <= 80; ++i) {
            double vin = 10.0 * (i / 80.0);
            double vout = dummyHard.processSample (vin, R_in, dt, alpha_up, R_k_up, R_p_up, C_dummy, pos_up, alpha_dn, R_k_dn, R_p_dn, C_dummy, pos_dn);
            float x = midX + static_cast<float>(vin) * scaleX;
            float y = midY - static_cast<float>(vout) * scaleY;
            if (i == 0) curve.startNewSubPath (x, y);
            else        curve.lineTo (x, y);
        }

        dummyHard.reset();
        juce::Path negCurve;
        for (int i = 0; i <= 80; ++i) {
            double vin = -10.0 * (i / 80.0);
            double vout = dummyHard.processSample (vin, R_in, dt, alpha_up, R_k_up, R_p_up, C_dummy, pos_up, alpha_dn, R_k_dn, R_p_dn, C_dummy, pos_dn);
            float x = midX + static_cast<float>(vin) * scaleX;
            float y = midY - static_cast<float>(vout) * scaleY;
            if (i == 0) negCurve.startNewSubPath (x, y);
            else        negCurve.lineTo (x, y);
        }
        curve.addPath (negCurve);
    }

    renderCrtBeam (g, curve, juce::Colour (0xff38ef7d), 1.4f);

    float scaleDotX = isA ? (screenArea.getWidth() / 3.0f) : (screenArea.getWidth() / 20.0f);
    float scaleDotY = isA ? (screenArea.getHeight() / 4.2f) : (screenArea.getHeight() / 20.0f);
    float vxP = juce::jlimit (0.0f, isA ? 1.4f : 10.0f, pPlus);
    float vxM = juce::jlimit (isA ? -1.4f : -10.0f, 0.0f, pMinus);

    auto drawDot = [&](float vx, float vy) {
        float x = midX + vx * scaleDotX;
        float y = midY - vy * scaleDotY;
        g.setColour (juce::Colour (0xff38ef7d).withAlpha (0.45f));
        g.fillEllipse (x - 4.5f, y - 4.5f, 9.0f, 9.0f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (x - 2.0f, y - 2.0f, 4.0f, 4.0f);
    };

    if (isA) {
        RubberZenerSoft dS;
        dS.reset();
        auto getPA = [this](const char* id, float def) {
            auto* p = processor.apvts.getRawParameterValue (id);
            return p != nullptr ? static_cast<double>(p->load()) : static_cast<double>(def);
        };
        double dAlpha = juce::jlimit (0.0, 1.0, getPA ("A_DRIVE", 0.5f));
        double RF_A   = 4700.0 + 50000.0 * (1.0 + 9.0 * std::pow (dAlpha, 1.6));
        float vyP = (float)dS.processSample (vxP, dt, getPA("A_THRESH_P",0.5f), 1e-15, (int)getPA("A_CAP_POS_P",0), getPA("A_THRESH_M",0.5f), 1e-15, (int)getPA("A_CAP_POS_M",0), RF_A);
        dS.reset();
        float vyM = (float)dS.processSample (vxM, dt, getPA("A_THRESH_P",0.5f), 1e-15, (int)getPA("A_CAP_POS_P",0), getPA("A_THRESH_M",0.5f), 1e-15, (int)getPA("A_CAP_POS_M",0), RF_A);
        drawDot (vxP, vyP);
        drawDot (vxM, vyM);
    } else {
        RubberZenerHard dH;
        dH.reset();
        auto getPB = [this](const char* id, float def) {
            auto* p = processor.apvts.getRawParameterValue (id);
            return p != nullptr ? static_cast<double>(p->load()) : static_cast<double>(def);
        };
        float vyP = (float)dH.processSample (vxP, 10000.0, dt, getPB("B_THRESH_P",0.5f), getPB("B_KNEE_P",10.0f), getPB("B_PINCH_P",1000.0f), 1e-15, (int)getPB("B_CAP_POS_P",0), getPB("B_THRESH_M",0.5f), getPB("B_KNEE_M",10.0f), getPB("B_PINCH_M",1000.0f), 1e-15, (int)getPB("B_CAP_POS_M",0));
        dH.reset();
        float vyM = (float)dH.processSample (vxM, 10000.0, dt, getPB("B_THRESH_P",0.5f), getPB("B_KNEE_P",10.0f), getPB("B_PINCH_P",1000.0f), 1e-15, (int)getPB("B_CAP_POS_P",0), getPB("B_THRESH_M",0.5f), getPB("B_KNEE_M",10.0f), getPB("B_PINCH_M",1000.0f), 1e-15, (int)getPB("B_CAP_POS_M",0));
        drawDot (vxP, vyP);
        drawDot (vxM, vyM);
    }
}

void TimeDomainScope::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff181a1f));
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (juce::Colour (0xff2d313b));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

    auto screen = bounds.reduced (4.0f);
    juce::ColourGradient screenGrad (juce::Colour (0xff0a141a), screen.getCentreX(), screen.getY(),
                                     juce::Colour (0xff020508), screen.getCentreX(), screen.getBottom(), false);
    g.setGradientFill (screenGrad);
    g.fillRoundedRectangle (screen, 6.0f);

    float midY = screen.getCentreY();
    g.setColour (juce::Colour (0xff142b38));
    g.drawHorizontalLine ((int)midY, screen.getX(), screen.getRight());

    g.setFont (juce::Font (juce::FontOptions (9.5f)).boldened());
    g.setColour (juce::Colour (0xff00f2fe).withAlpha (0.9f));
    g.drawText (isA ? "WAVEFORM A" : "WAVEFORM B", screen.reduced (6.0f, 3.0f), juce::Justification::topLeft);

    juce::Path pathIn, pathOut;
    int readIdx = processor.scopeData.writeIndex.load (std::memory_order_relaxed);
    float scaleY = screen.getHeight() * 0.42f;

    for (int i = 0; i < ScopeData::bufferSize; ++i) {
        int idx = (readIdx + i) % ScopeData::bufferSize;
        float x = screen.getX() + screen.getWidth() * (i / (float)(ScopeData::bufferSize - 1));
        
        float rawIn  = isA ? processor.scopeData.bufferInA[(size_t)idx]  : processor.scopeData.bufferInB[(size_t)idx];
        float rawOut = isA ? processor.scopeData.bufferOutA[(size_t)idx] : processor.scopeData.bufferOutB[(size_t)idx];
        
        float yIn  = midY - juce::jlimit (-1.0f, 1.0f, rawIn) * scaleY;
        float yOut = midY - juce::jlimit (-1.0f, 1.0f, rawOut) * scaleY;
        if (i == 0) { pathIn.startNewSubPath (x, yIn); pathOut.startNewSubPath (x, yOut); }
        else        { pathIn.lineTo (x, yIn); pathOut.lineTo (x, yOut); }
    }
    g.setColour (juce::Colour (0xff38ef7d).withAlpha (0.75f));
    g.strokePath (pathIn, juce::PathStrokeType (1.1f));
    g.setColour (isA ? juce::Colour (0xfffbbf24) : juce::Colour (0xffff4757));
    g.strokePath (pathOut, juce::PathStrokeType (1.3f));
}

// ==============================================================================
// MAIN EDITOR IMPLEMENTATION
// ==============================================================================
GomuGomuNoDriveEditor::GomuGomuNoDriveEditor (GomuGomuNoDrive& processorRef)
    : AudioProcessorEditor (&processorRef), audioProcessor (processorRef),
      preEqComponent (processorRef.apvts, "PRE_EQ_", "PRE-EQ (INPUT SCULPTING)", juce::Colour (0xff0284c7)),
      postEqComponent (processorRef.apvts, "POST_EQ_", "POST-EQ (TONE SHAPING)", juce::Colour (0xffea580c)),
      oscIU_A (processorRef, true), oscTime_A (processorRef, true),
      oscIU_B (processorRef, false), oscTime_B (processorRef, false)
{
    setSize (1520, 890);
    setLookAndFeel (&customLookAndFeel);

    // --- BARRE DE ROUTAGE TEXTUELLE ---
    addAndMakeVisible (btnRoutingAB);
    addAndMakeVisible (btnRoutingBA);
    addAndMakeVisible (btnRoutingPara);

    auto* rParam = audioProcessor.apvts.getRawParameterValue ("ROUTING");
    int initR = rParam != nullptr ? (int)rParam->load() : 0;
    updateRoutingButtons (initR);

    btnRoutingAB.onClick = [this]() {
        if (auto* param = audioProcessor.apvts.getParameter ("ROUTING"))
            param->setValueNotifyingHost (param->convertTo0to1 (0.0f));
        updateRoutingButtons (0);
    };
    btnRoutingBA.onClick = [this]() {
        if (auto* param = audioProcessor.apvts.getParameter ("ROUTING"))
            param->setValueNotifyingHost (param->convertTo0to1 (1.0f));
        updateRoutingButtons (1);
    };
    btnRoutingPara.onClick = [this]() {
        if (auto* param = audioProcessor.apvts.getParameter ("ROUTING"))
            param->setValueNotifyingHost (param->convertTo0to1 (2.0f));
        updateRoutingButtons (2);
    };

    setupKnob (interCutSlider, "INTER-CUT");
    interCutAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "INTER_CUT", interCutSlider);

    addAndMakeVisible (preEqComponent);
    addAndMakeVisible (postEqComponent);
    addAndMakeVisible (oscIU_A);
    addAndMakeVisible (oscTime_A);
    addAndMakeVisible (oscIU_B);
    addAndMakeVisible (oscTime_B);

    // --- PÉDALE A (SOFT CLIPPING) ---
    setupKnob (driveASlider, "DRIVE A");
    setupKnob (levelASlider, "LEVEL A");
    setupKnob (toneASlider,  "TONE A");
    driveAAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "A_DRIVE", driveASlider);
    levelAAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "A_LEVEL", levelASlider);
    toneAAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "A_TONE",  toneASlider);

    addAndMakeVisible (voicingASelector);
    voicingASelector.addItemList ({ "TS-VOICE", "RAT-VOICE", "MUFF-VOICE", "LAB / CUSTOM" }, 1);
    voicingAAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (audioProcessor.apvts, "A_VOICING", voicingASelector);

    setupKnob   (clipAUpSlider, "CLIP");
    setupKnob   (edgeAUpSlider, "EDGE");
    setupSwitch (capAUpSwitch,  "VOICING");
    clipAUpAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "A_THRESH_P", clipAUpSlider);
    edgeAUpAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "A_CAP_P",    edgeAUpSlider);
    capAUpAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "A_CAP_POS_P", capAUpSwitch);

    setupKnob   (clipADnSlider, "CLIP");
    setupKnob   (edgeADnSlider, "EDGE");
    setupSwitch (capADnSwitch,  "VOICING");
    clipADnAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "A_THRESH_M", clipADnSlider);
    edgeADnAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "A_CAP_M",    edgeADnSlider);
    capADnAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "A_CAP_POS_M", capADnSwitch);

    addAndMakeVisible (footswitchA);
    addAndMakeVisible (ledA);
    footswitchA.setClickingTogglesState (true);
    footswitchA.setButtonText ("");
    ledA.setColour (juce::Colour (0xff10b981));
    footAAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "A_ENABLE", footswitchA);

    // --- PÉDALE B (HARD CLIPPING) ---
    setupKnob (driveBSlider, "DRIVE B");
    setupKnob (levelBSlider, "LEVEL B");
    setupKnob (toneBSlider,  "TONE B");
    driveBAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "B_DRIVE", driveBSlider);
    levelBAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "B_LEVEL", levelBSlider);
    toneBAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "B_TONE",  toneBSlider);

    addAndMakeVisible (voicingBSelector);
    voicingBSelector.addItemList ({ "TS-VOICE", "RAT-VOICE", "MUFF-VOICE", "LAB / CUSTOM" }, 1);
    voicingBAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (audioProcessor.apvts, "B_VOICING", voicingBSelector);

    setupKnob   (clipBUpSlider,  "CLIP");
    setupKnob   (edgeBUpSlider,  "EDGE");
    setupSwitch (capBUpSwitch,   "VOICING");
    setupKnob   (kneeBUpSlider,  "KNEE");
    setupKnob   (pinchBUpSlider, "PINCH");
    clipBUpAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "B_THRESH_P", clipBUpSlider);
    edgeBUpAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "B_CAP_P",    edgeBUpSlider);
    capBUpAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "B_CAP_POS_P", capBUpSwitch);
    kneeBUpAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "B_KNEE_P",   kneeBUpSlider);
    pinchBUpAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "B_PINCH_P",  pinchBUpSlider);

    setupKnob   (clipBDnSlider,  "CLIP");
    setupKnob   (edgeBDnSlider,  "EDGE");
    setupSwitch (capBDnSwitch,   "VOICING");
    setupKnob   (kneeBDnSlider,  "KNEE");
    setupKnob   (pinchBDnSlider, "PINCH");
    clipBDnAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "B_THRESH_M", clipBDnSlider);
    edgeBDnAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "B_CAP_M",    edgeBDnSlider);
    capBDnAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "B_CAP_POS_M", capBDnSwitch);
    kneeBDnAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "B_KNEE_M",   kneeBDnSlider);
    pinchBDnAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, "B_PINCH_M",  pinchBDnSlider);

    addAndMakeVisible (footswitchB);
    addAndMakeVisible (ledB);
    footswitchB.setClickingTogglesState (true);
    footswitchB.setButtonText ("");
    ledB.setColour (juce::Colour (0xffef4444));
    footBAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "B_ENABLE", footswitchB);

    // Section Cab
    addAndMakeVisible (cabToggle);
    cabEnableAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (audioProcessor.apvts, "CAB_ENABLE", cabToggle);

    addAndMakeVisible (cabSelector);
    cabSelector.clear();
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i) {
        juce::String name = juce::String::fromUTF8 (BinaryData::originalFilenames[i]);
        if (name.endsWithIgnoreCase (".wav")) name = name.dropLastCharacters (4);
        cabSelector.addItem (name, i + 1);
    }
    cabSelectAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (audioProcessor.apvts, "CAB_SELECT", cabSelector);
    cabSelector.onChange = [this]() { audioProcessor.loadBundledIR (cabSelector.getSelectedItemIndex()); };

    startTimerHz (30);
}

GomuGomuNoDriveEditor::~GomuGomuNoDriveEditor() {
    stopTimer();
    setLookAndFeel (nullptr);
}

void GomuGomuNoDriveEditor::updateRoutingButtons (int mode) {
    btnRoutingAB.setActive   (mode == 0);
    btnRoutingBA.setActive   (mode == 1);
    btnRoutingPara.setActive (mode == 2);
    repaint();
}

void GomuGomuNoDriveEditor::timerCallback() {
    float curAPlus  = audioProcessor.scopeData.peakInVoltsA_Plus.load (std::memory_order_relaxed);
    float curAMinus = audioProcessor.scopeData.peakInVoltsA_Minus.load (std::memory_order_relaxed);
    if (curAPlus > smoothPeakA_Plus) smoothPeakA_Plus = curAPlus; else smoothPeakA_Plus *= 0.85f;
    if (curAMinus < smoothPeakA_Minus) smoothPeakA_Minus = curAMinus; else smoothPeakA_Minus *= 0.85f;

    float curBPlus  = audioProcessor.scopeData.peakInVoltsB_Plus.load (std::memory_order_relaxed);
    float curBMinus = audioProcessor.scopeData.peakInVoltsB_Minus.load (std::memory_order_relaxed);
    if (curBPlus > smoothPeakB_Plus) smoothPeakB_Plus = curBPlus; else smoothPeakB_Plus *= 0.85f;
    if (curBMinus < smoothPeakB_Minus) smoothPeakB_Minus = curBMinus; else smoothPeakB_Minus *= 0.85f;

    audioProcessor.scopeData.peakInVoltsA_Plus.store (0.0f, std::memory_order_relaxed);
    audioProcessor.scopeData.peakInVoltsA_Minus.store (0.0f, std::memory_order_relaxed);
    audioProcessor.scopeData.peakInVoltsB_Plus.store (0.0f, std::memory_order_relaxed);
    audioProcessor.scopeData.peakInVoltsB_Minus.store (0.0f, std::memory_order_relaxed);

    if (auto* rVal = audioProcessor.apvts.getRawParameterValue ("ROUTING")) {
        updateRoutingButtons ((int)rVal->load());
    }

    auto* aParam = audioProcessor.apvts.getRawParameterValue ("A_ENABLE");
    auto* bParam = audioProcessor.apvts.getRawParameterValue ("B_ENABLE");
    bool aOn = (aParam != nullptr && aParam->load() > 0.5f);
    bool bOn = (bParam != nullptr && bParam->load() > 0.5f);
    ledA.setOn (aOn);
    ledB.setOn (bOn);

    bool isLab = (voicingASelector.getSelectedItemIndex() == 3) || (voicingBSelector.getSelectedItemIndex() == 3);
    if (preEqComponent.isEnabled() != isLab) {
        preEqComponent.setEnabled (isLab);
        postEqComponent.setEnabled (isLab);
        preEqComponent.repaint();
        postEqComponent.repaint();
    }

    oscIU_A.updatePeaks (smoothPeakA_Plus, smoothPeakA_Minus);
    oscIU_B.updatePeaks (smoothPeakB_Plus, smoothPeakB_Minus);
    oscIU_A.repaint();
    oscTime_A.repaint();
    oscIU_B.repaint();
    oscTime_B.repaint();
}

void GomuGomuNoDriveEditor::setupKnob (juce::Slider& s, const juce::String& name) {
    s.setName (name);
    addAndMakeVisible (s);
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
}

void GomuGomuNoDriveEditor::setupSwitch (juce::Slider& s, const juce::String& name) {
    s.setName (name);
    addAndMakeVisible (s);
    s.setSliderStyle (juce::Slider::LinearVertical);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setRange (0, 1, 1);
}

void GomuGomuNoDriveEditor::paint (juce::Graphics& g) {
    g.fillAll (juce::Colour (0xffcbd0d8));
    auto area = getLocalBounds().reduced (8);

    // 1. Barre de rack supérieure
    auto topArea = area.removeFromTop (46);
    g.setColour (juce::Colour (0xff0f172a));
    g.fillRoundedRectangle (topArea.toFloat(), 6.0f);
    g.setColour (juce::Colour (0xff334155));
    g.drawRoundedRectangle (topArea.toFloat(), 6.0f, 1.2f);

    area.removeFromTop (4);
    area.removeFromTop (56);    // Pre-EQ
    area.removeFromBottom (38); // Cab bar
    area.removeFromBottom (56); // Post-EQ
    area.removeFromBottom (6);

    // 2. Zone des Pédales
    int gapW = 60;
    int pedalW = (area.getWidth() - gapW) / 2;
    auto leftPedal  = area.removeFromLeft (pedalW);
    auto gapArea    = area.removeFromLeft (gapW);
    auto rightPedal = area;

    // --- CHÂSSIS PÉDALE A (SOFT) ---
    juce::ColourGradient gradA (juce::Colour (0xfff1f5f9), leftPedal.getX(), leftPedal.getY(),
                                juce::Colour (0xffcbd5e1), leftPedal.getRight(), leftPedal.getBottom(), false);
    g.setGradientFill (gradA);
    g.fillRoundedRectangle (leftPedal.toFloat(), 10.0f);
    g.setColour (juce::Colour (0xff64748b));
    g.drawRoundedRectangle (leftPedal.toFloat(), 10.0f, 1.5f);

    // --- CHÂSSIS PÉDALE B (HARD) ---
    juce::ColourGradient gradB (juce::Colour (0xfff3f4f6), rightPedal.getX(), rightPedal.getY(),
                                juce::Colour (0xffd1d5db), rightPedal.getRight(), rightPedal.getBottom(), false);
    g.setGradientFill (gradB);
    g.fillRoundedRectangle (rightPedal.toFloat(), 10.0f);
    g.setColour (juce::Colour (0xff4b5563));
    g.drawRoundedRectangle (rightPedal.toFloat(), 10.0f, 1.5f);

    // Helper pour dessiner les ailes et le corps central
    auto drawPedalChassis = [&](juce::Rectangle<int> pArea, bool isSoft) {
        auto contentArea = pArea.reduced (6);
        contentArea.removeFromTop (26); // Titre
        auto scopeArea = contentArea.removeFromBottom (130);
        contentArea.removeFromBottom (6);

        float wingW = contentArea.getWidth() * 0.38f;
        auto westBounds = contentArea.removeFromLeft (wingW).toFloat().reduced (2.0f);
        auto eastBounds = contentArea.removeFromRight (wingW).toFloat().reduced (2.0f);
        auto centerBounds = contentArea.toFloat().reduced (2.0f);

        // Aile GAUCHE (北 en Soft, 西 en Hard)
        juce::ColourGradient westBg (juce::Colour (0xffdde4eb), westBounds.getX(), westBounds.getY(),
                                     juce::Colour (0xffbcc5cf), westBounds.getRight(), westBounds.getBottom(), false);
        g.setGradientFill (westBg);
        g.fillRoundedRectangle (westBounds, 8.0f);
        g.setColour (juce::Colour (0xff707e8f));
        g.drawRoundedRectangle (westBounds, 8.0f, 1.2f);

        g.setColour (juce::Colour (0xff334155).withAlpha (0.08f));
        g.setFont (juce::Font (juce::FontOptions (180.0f)).boldened());
        g.drawText (isSoft ? juce::String::fromUTF8 ("北") : juce::String::fromUTF8 ("西"), 
                    westBounds, juce::Justification::centred);

        g.setColour (juce::Colour (0xff0f172a));
        g.setFont (juce::Font (juce::FontOptions (13.5f)).boldened());
        g.drawText (isSoft ? juce::String::fromUTF8 ("北 · UPPER CLIP") : juce::String::fromUTF8 ("西 · UPPER CLIP"), 
                    westBounds.removeFromTop (22.0f), juce::Justification::centred);

        // Aile DROITE (南 en Soft, 東 en Hard)
        juce::ColourGradient eastBg (juce::Colour (0xffebe4dc), eastBounds.getX(), eastBounds.getY(),
                                     juce::Colour (0xffcfc4b8), eastBounds.getRight(), eastBounds.getBottom(), false);
        g.setGradientFill (eastBg);
        g.fillRoundedRectangle (eastBounds, 8.0f);
        g.setColour (juce::Colour (0xff8f7e70));
        g.drawRoundedRectangle (eastBounds, 8.0f, 1.2f);

        g.setColour (juce::Colour (0xff7c2d12).withAlpha (0.07f));
        g.setFont (juce::Font (juce::FontOptions (180.0f)).boldened());
        g.drawText (isSoft ? juce::String::fromUTF8 ("南") : juce::String::fromUTF8 ("東"), 
                    eastBounds, juce::Justification::centred);

        g.setColour (juce::Colour (0xff27170a));
        g.setFont (juce::Font (juce::FontOptions (13.5f)).boldened());
        g.drawText (isSoft ? juce::String::fromUTF8 ("南 · LOWER CLIP") : juce::String::fromUTF8 ("東 · LOWER CLIP"), 
                    eastBounds.removeFromTop (22.0f), juce::Justification::centred);

        // Kanji central thématique sous les potards : 柔 (Jū) ou 剛 (Gō)
        g.setColour (juce::Colour (0xff334155).withAlpha (0.08f));
        g.setFont (juce::Font (juce::FontOptions (210.0f)).boldened());
        g.drawText (isSoft ? juce::String::fromUTF8 ("柔") : juce::String::fromUTF8 ("剛"),
                    centerBounds.translated (0.0f, 25.0f), juce::Justification::centred);

        // Badge central
        float logoY = centerBounds.getY() + 185.0f;
        auto logoArea = juce::Rectangle<float> (centerBounds.getX(), logoY, centerBounds.getWidth(), 50.0f);
        g.setColour (juce::Colour (0xff0f172a));
        g.setFont (juce::Font (juce::FontOptions (18.0f)).boldened());
        g.drawFittedText (juce::String::fromUTF8 ("ゴムゴムの DRIVE"), logoArea.toNearestInt(), juce::Justification::centred, 1);
        
        g.setColour (isSoft ? juce::Colour (0xff059669) : juce::Colour (0xffdc2626));
        g.setFont (juce::Font (juce::FontOptions (9.5f)).boldened());
        g.drawText (isSoft ? "SOFT CLIPPING TOPOLOGY" : "HARD CLIPPING TOPOLOGY", 
                    logoArea.translated (0.0f, 20.0f).toNearestInt(), juce::Justification::centred);
    };

    drawPedalChassis (leftPedal, true);
    drawPedalChassis (rightPedal, false);

    // Titres principaux des pédales
    g.setColour (juce::Colour (0xff0f172a));
    g.setFont (juce::Font (juce::FontOptions (15.5f)).boldened());
    g.drawText (juce::String::fromUTF8 ("ゴムゴムの DRIVE [ソフト]"), leftPedal.removeFromTop (26), juce::Justification::centred);
    g.drawText (juce::String::fromUTF8 ("ゴムゴムの DRIVE [ハード]"), rightPedal.removeFromTop (26), juce::Justification::centred);

    // --- EMBASES JACK ET PATCH CÂBLE ---
    float jackY = leftPedal.getY() + 260.0f;

    // Embase Jack Entrée Guitare (gauche de A)
    float inJackX = leftPedal.getX() - 6.0f;
    g.setColour (juce::Colour (0xff1e293b));
    g.fillRoundedRectangle (inJackX, jackY - 14.0f, 6.0f, 28.0f, 1.5f);
    g.setColour (juce::Colour (0xff94a3b8));
    g.drawRoundedRectangle (inJackX, jackY - 14.0f, 6.0f, 28.0f, 1.5f, 1.0f);

    // Embase Jack Sortie Ampli (droite de B)
    float outJackX = rightPedal.getRight();
    g.setColour (juce::Colour (0xff1e293b));
    g.fillRoundedRectangle (outJackX, jackY - 14.0f, 6.0f, 28.0f, 1.5f);
    g.setColour (juce::Colour (0xff94a3b8));
    g.drawRoundedRectangle (outJackX, jackY - 14.0f, 6.0f, 28.0f, 1.5f, 1.0f);

    // Embases internes dans le gap
    float jackA_X = leftPedal.getRight();
    float jackB_X = rightPedal.getX() - 6.0f;

    auto drawNut = [&](float x, float y) {
        g.setColour (juce::Colour (0xff334155));
        g.fillRect (x, y - 11.0f, 6.0f, 22.0f);
        g.setColour (juce::Colour (0xffe2e8f0));
        g.drawRect (x, y - 11.0f, 6.0f, 22.0f, 1.0f);
    };
    drawNut (jackA_X, jackY);
    drawNut (jackB_X, jackY);

    // Câble patch visible uniquement en série (modes A -> B et B -> A)
    auto* rParam = audioProcessor.apvts.getRawParameterValue ("ROUTING");
    int currentRouting = rParam != nullptr ? (int)rParam->load() : 0;

    if (currentRouting != 2) {
        juce::Rectangle<float> plugA (jackA_X + 6.0f, jackY - 9.0f, 14.0f, 18.0f);
        juce::Rectangle<float> plugB (jackB_X - 14.0f, jackY - 9.0f, 14.0f, 18.0f);
        g.setColour (juce::Colour (0xffcbd5e1));
        g.fillRoundedRectangle (plugA, 2.5f);
        g.fillRoundedRectangle (plugB, 2.5f);
        g.setColour (juce::Colour (0xff475569));
        g.drawRoundedRectangle (plugA, 2.5f, 1.0f);
        g.drawRoundedRectangle (plugB, 2.5f, 1.0f);

        juce::Path cable;
        float startX = plugA.getRight();
        float endX   = plugB.getX();
        cable.startNewSubPath (startX, jackY);
        cable.cubicTo (startX + 18.0f, jackY + 70.0f, endX - 18.0f, jackY + 70.0f, endX, jackY);

        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.strokePath (cable, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::translation (2.0f, 4.0f));

        g.setColour (juce::Colour (0xff1e2229));
        g.strokePath (cable, juce::PathStrokeType (6.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xff475569).withAlpha (0.6f));
        g.strokePath (cable, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void GomuGomuNoDriveEditor::resized() {
    auto area = getLocalBounds().reduced (8);

    // 1. Barre de Routage Supérieure
    auto topBar = area.removeFromTop (46).reduced (6, 4);
    interCutSlider.setBounds (topBar.removeFromRight (140));

    auto rBtns = topBar.removeFromLeft (720);
    int bW = rBtns.getWidth() / 3;
    btnRoutingAB.setBounds   (rBtns.removeFromLeft (bW));
    btnRoutingBA.setBounds   (rBtns.removeFromLeft (bW));
    btnRoutingPara.setBounds (rBtns);

    area.removeFromTop (4);

    // 2. Pre-EQ
    preEqComponent.setBounds (area.removeFromTop (56));
    area.removeFromTop (4);

    // 3. Post-EQ
    postEqComponent.setBounds (area.removeFromBottom (56));
    area.removeFromBottom (4);

    // 4. Barre Cab IR
    auto cabRow = area.removeFromBottom (34).reduced (10, 0);
    cabToggle.setBounds (cabRow.removeFromLeft (36).reduced (2));
    cabSelector.setBounds (cabRow.removeFromLeft (320).reduced (4, 2));

    area.removeFromBottom (6);

    // 5. Agencement des deux Pédales (A et B)
    int gapW = 60;
    int pedalW = (area.getWidth() - gapW) / 2;
    auto areaA = area.removeFromLeft (pedalW).reduced (6);
    area.removeFromLeft (gapW);
    auto areaB = area.reduced (6);

    // --- LAYOUT PÉDALE A (SOFT) ---
    areaA.removeFromTop (26);
    auto bottomA = areaA.removeFromBottom (126);
    areaA.removeFromBottom (4);

    int scopeWA = (bottomA.getWidth() - 8) / 2;
    oscIU_A.setBounds   (bottomA.removeFromLeft (scopeWA));
    bottomA.removeFromLeft (8);
    oscTime_A.setBounds (bottomA);

    float wingWA = areaA.getWidth() * 0.38f;
    auto westA = areaA.removeFromLeft (wingWA).reduced (4);
    auto eastA = areaA.removeFromRight (wingWA).reduced (4);
    auto centerA = areaA.reduced (4);

    driveASlider.setBounds (centerA.removeFromTop (100).withSizeKeepingCentre (82, 98));
    auto rowOutToneA = centerA.removeFromTop (90);
    int halfCA = rowOutToneA.getWidth() / 2;
    levelASlider.setBounds (rowOutToneA.removeFromLeft (halfCA).withSizeKeepingCentre (72, 88));
    toneASlider.setBounds  (rowOutToneA.withSizeKeepingCentre (72, 88));
    voicingASelector.setBounds (centerA.removeFromTop (30).withSizeKeepingCentre (140, 24));

    auto footBoxA = centerA.withSizeKeepingCentre (100, 70);
    ledA.setBounds (footBoxA.removeFromLeft (24).withSizeKeepingCentre (18, 18));
    footswitchA.setBounds (footBoxA.withSizeKeepingCentre (64, 64));

    // Aile Gauche A (北)
    westA.removeFromTop (22);
    clipAUpSlider.setBounds (westA.removeFromTop (125).withSizeKeepingCentre (94, 114));
    auto rowW2_A = westA.removeFromTop (115);
    int colW2A = rowW2_A.getWidth() / 2;
    edgeAUpSlider.setBounds (rowW2_A.removeFromLeft (colW2A).withSizeKeepingCentre (74, 94));
    capAUpSwitch.setBounds  (rowW2_A.withSizeKeepingCentre (68, 85));

    // Aile Droite A (南)
    eastA.removeFromTop (22);
    clipADnSlider.setBounds (eastA.removeFromTop (125).withSizeKeepingCentre (94, 114));
    auto rowE2_A = eastA.removeFromTop (115);
    int colE2A = rowE2_A.getWidth() / 2;
    edgeADnSlider.setBounds (rowE2_A.removeFromLeft (colE2A).withSizeKeepingCentre (74, 94));
    capADnSwitch.setBounds  (rowE2_A.withSizeKeepingCentre (68, 85));

    // --- LAYOUT PÉDALE B (HARD) ---
    areaB.removeFromTop (26);
    auto bottomB = areaB.removeFromBottom (126);
    areaB.removeFromBottom (4);

    int scopeWB = (bottomB.getWidth() - 8) / 2;
    oscIU_B.setBounds   (bottomB.removeFromLeft (scopeWB));
    bottomB.removeFromLeft (8);
    oscTime_B.setBounds (bottomB);

    float wingWB = areaB.getWidth() * 0.38f;
    auto westB = areaB.removeFromLeft (wingWB).reduced (4);
    auto eastB = areaB.removeFromRight (wingWB).reduced (4);
    auto centerB = areaB.reduced (4);

    driveBSlider.setBounds (centerB.removeFromTop (100).withSizeKeepingCentre (82, 98));
    auto rowOutToneB = centerB.removeFromTop (90);
    int halfCB = rowOutToneB.getWidth() / 2;
    levelBSlider.setBounds (rowOutToneB.removeFromLeft (halfCB).withSizeKeepingCentre (72, 88));
    toneBSlider.setBounds  (rowOutToneB.withSizeKeepingCentre (72, 88));
    voicingBSelector.setBounds (centerB.removeFromTop (30).withSizeKeepingCentre (140, 24));

    auto footBoxB = centerB.withSizeKeepingCentre (100, 70);
    ledB.setBounds (footBoxB.removeFromLeft (24).withSizeKeepingCentre (18, 18));
    footswitchB.setBounds (footBoxB.withSizeKeepingCentre (64, 64));

    // Aile Gauche B (西)
    westB.removeFromTop (22);
    clipBUpSlider.setBounds (westB.removeFromTop (115).withSizeKeepingCentre (88, 108));
    auto rowW2_B = westB.removeFromTop (105);
    int colW2B = rowW2_B.getWidth() / 2;
    edgeBUpSlider.setBounds (rowW2_B.removeFromLeft (colW2B).withSizeKeepingCentre (72, 92));
    capBUpSwitch.setBounds  (rowW2_B.withSizeKeepingCentre (65, 80));

    auto rowW3_B = westB.removeFromTop (105);
    int colW3B = rowW3_B.getWidth() / 2;
    kneeBUpSlider.setBounds  (rowW3_B.removeFromLeft (colW3B).withSizeKeepingCentre (72, 92));
    pinchBUpSlider.setBounds (rowW3_B.withSizeKeepingCentre (72, 92));

    // Aile Droite B (東)
    eastB.removeFromTop (22);
    clipBDnSlider.setBounds (eastB.removeFromTop (115).withSizeKeepingCentre (88, 108));
    auto rowE2_B = eastB.removeFromTop (105);
    int colE2B = rowE2_B.getWidth() / 2;
    edgeBDnSlider.setBounds (rowE2_B.removeFromLeft (colE2B).withSizeKeepingCentre (72, 92));
    capBDnSwitch.setBounds  (rowE2_B.withSizeKeepingCentre (65, 80));

    auto rowE3_B = eastB.removeFromTop (105);
    int colE3B = rowE3_B.getWidth() / 2;
    kneeBDnSlider.setBounds  (rowE3_B.removeFromLeft (colE3B).withSizeKeepingCentre (72, 92));
    pinchBDnSlider.setBounds (rowE3_B.withSizeKeepingCentre (72, 92));
}
