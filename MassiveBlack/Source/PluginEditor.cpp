#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <complex>

namespace Palette
{
    static const juce::Colour bgObsidian   (0xff0b0c0f);
    static const juce::Colour panelDark    (0xff13151b);
    static const juce::Colour panelBorder  (0xff232734);
    static const juce::Colour crispWhite   (0xfff5f7fa);
    static const juce::Colour mutedGrey    (0xff8e96a8);
    static const juce::Colour electricYel  (0xffffd426);
    static const juce::Colour vibrantRed   (0xffff3b4e);
}

NegaPhaseLookAndFeel::NegaPhaseLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Palette::crispWhite);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff181b24));
    setColour (juce::ComboBox::textColourId, Palette::crispWhite);
    setColour (juce::ComboBox::outlineColourId, Palette::panelBorder);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff13151b));
    setColour (juce::PopupMenu::textColourId, Palette::crispWhite);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Palette::electricYel.withAlpha (0.25f));
}

void NegaPhaseLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                           juce::Slider& slider)
{
    const float radius = juce::jmin (width, height) * 0.41f;
    const float cx = x + width * 0.5f;
    const float cy = y + height * 0.5f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
    if (accent.isTransparent()) accent = Palette::electricYel;

    g.setColour (juce::Colour (0xff181b24));
    g.fillEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
    g.setColour (Palette::panelBorder);
    g.drawEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.0f);

    juce::Path bgArc;
    bgArc.addCentredArc (cx, cy, radius + 2.5f, radius + 2.5f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (0xff262b38));
    g.strokePath (bgArc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valArc;
    valArc.addCentredArc (cx, cy, radius + 2.5f, radius + 2.5f, 0.0f, rotaryStartAngle, angle, true);
    g.setColour (accent);
    g.strokePath (valArc, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float px = cx + (radius * 0.72f) * std::sin (angle);
    const float py = cy - (radius * 0.72f) * std::cos (angle);
    g.setColour (Palette::crispWhite);
    g.drawLine (cx, cy, px, py, 1.8f);
}

void NegaPhaseLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                         int, int, int, int, juce::ComboBox&)
{
    auto b = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height)).reduced (0.5f);
    g.setColour (juce::Colour (0xff181b24));
    g.fillRoundedRectangle (b, 4.0f);
    g.setColour (Palette::panelBorder);
    g.drawRoundedRectangle (b, 4.0f, 1.0f);
}

// ==============================================================================
void SweepAnalyzerView::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    if (b.getWidth() <= 4.0f || b.getHeight() <= 4.0f) return;

    g.setColour (juce::Colour (0xff090a0e));
    g.fillRoundedRectangle (b, 6.0f);

    // Pro-Q 4 style logarithmic frequency grid (50, 100, 200, 500, 1k, 2k, 5k, 10k)
    const float freqs[] = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };
    const char* fLabels[] = { "50", "100", "200", "500", "1k", "2k", "5k", "10k" };

    g.setFont (juce::FontOptions (9.5f));
    for (int i = 0; i < 8; ++i)
    {
        const float normX = std::log10 (freqs[i] / 25.0f) / std::log10 (18000.0f / 25.0f);
        const float gx = b.getX() + normX * b.getWidth();
        g.setColour (juce::Colour (0xff191c25));
        g.drawVerticalLine (static_cast<int> (gx), b.getY() + 4, b.getBottom() - 16);
        g.setColour (Palette::mutedGrey.withAlpha (0.55f));
        g.drawText (fLabels[i], static_cast<int> (gx - 14), static_cast<int> (b.getBottom() - 15), 28, 12, juce::Justification::centred);
    }

    const float midY = b.getY() + b.getHeight() * 0.45f;
    g.setColour (juce::Colour (0xff1d212c));
    g.drawHorizontalLine (static_cast<int> (midY), b.getX() + 4, b.getRight() - 4);

    // Live Reverb Tail Waveform behind frequency curve
    std::vector<float> tailSamples;
    processor.getTailScopeSamples (tailSamples);
    if (!tailSamples.empty())
    {
        juce::Path tailPath;
        const float waveH = b.getHeight() * 0.25f;
        for (size_t i = 0; i < tailSamples.size(); ++i)
        {
            const float x = b.getX() + (static_cast<float> (i) / static_cast<float> (tailSamples.size() - 1)) * b.getWidth();
            const float y = (b.getBottom() - 28.0f) - tailSamples[i] * waveH;
            if (i == 0) tailPath.startNewSubPath (x, y);
            else        tailPath.lineTo (x, y);
        }
        g.setColour (Palette::crispWhite.withAlpha (0.28f));
        g.strokePath (tailPath, juce::PathStrokeType (1.3f));
    }

    // Exact Sweeping All-Pass / Negative-Space Transfer Curve on Reverb Tail
    const float sweepHz    = processor.getCurrentSweepHz();
    const float feedback   = processor.apvts.getRawParameterValue ("phaser_feedback")->load();
    const float imprintAmt = processor.apvts.getRawParameterValue ("imprint_amount")->load();
    const int stagesIdx    = static_cast<int> (processor.apvts.getRawParameterValue ("phaser_stages")->load());
    const int stageCounts[4] = { 4, 6, 8, 12 };
    const int numStages    = stageCounts[juce::jlimit (0, 3, stagesIdx)];
    const bool negGhost    = static_cast<int> (processor.apvts.getRawParameterValue ("imprint_mode")->load()) == 1;
    const double sr        = juce::jmax (8000.0, processor.getCurrentSampleRate());

    const float tanVal = std::tan (juce::MathConstants<float>::pi * sweepHz / static_cast<float> (sr));
    const float apCoeff = (tanVal - 1.0f) / (tanVal + 1.0f);

    juce::Path curvePath;
    juce::Path fillPath;
    const int numPts = 220;

    for (int i = 0; i <= numPts; ++i)
    {
        const float normX = static_cast<float> (i) / static_cast<float> (numPts);
        const float f = 25.0f * std::pow (18000.0f / 25.0f, normX);
        const float w = juce::MathConstants<float>::twoPi * f / static_cast<float> (sr);
        const std::complex<float> zInv (std::cos (-w), std::sin (-w));

        // 1-stage allpass H(z) = (c + z^-1) / (1 + c * z^-1)
        const std::complex<float> stageH = (apCoeff + zInv) / (1.0f + apCoeff * zInv);
        std::complex<float> chainH (1.0f, 0.0f);
        for (int st = 0; st < numStages; ++st)
            chainH *= stageH;

        const std::complex<float> closedLoop = chainH / (1.0f - feedback * chainH);
        const std::complex<float> combH = negGhost
            ? (1.0f - imprintAmt) + imprintAmt * 0.7f * (1.0f - closedLoop)
            : (1.0f - imprintAmt) + imprintAmt * 0.5f * (1.0f + closedLoop);

        const float mag = juce::jlimit (0.02f, 1.6f, std::abs (combH));
        const float db = juce::Decibels::gainToDecibels (mag, -28.0f);
        const float normY = juce::jlimit (0.05f, 0.95f, 0.5f - (db / 32.0f));

        const float px = b.getX() + normX * b.getWidth();
        const float py = b.getY() + 14.0f + normY * (b.getHeight() - 34.0f);

        if (i == 0)
        {
            curvePath.startNewSubPath (px, py);
            fillPath.startNewSubPath (px, midY);
            fillPath.lineTo (px, py);
        }
        else
        {
            curvePath.lineTo (px, py);
            fillPath.lineTo (px, py);
        }
    }
    fillPath.lineTo (b.getRight(), midY);
    fillPath.closeSubPath();

    g.setColour ((negGhost ? Palette::vibrantRed : Palette::electricYel).withAlpha (0.14f));
    g.fillPath (fillPath);

    g.setColour (negGhost ? Palette::vibrantRed : Palette::electricYel);
    g.strokePath (curvePath, juce::PathStrokeType (2.2f));

    // LFO Modulation Range Band & Live Sweep Marker
    const float centerHz = processor.apvts.getRawParameterValue ("phaser_center")->load();
    const float depthVal = processor.apvts.getRawParameterValue ("phaser_depth")->load();
    const float minHz    = juce::jmax (25.0f, centerHz * std::pow (2.0f, -depthVal * 2.6f));
    const float maxHz    = juce::jmin (18000.0f, centerHz * std::pow (2.0f, depthVal * 2.6f));
    auto hzToX = [&b] (float hz) -> float
    {
        const float n = std::log10 (juce::jlimit (25.0f, 18000.0f, hz) / 25.0f) / std::log10 (18000.0f / 25.0f);
        return b.getX() + n * b.getWidth();
    };
    const float xMin = hzToX (minHz);
    const float xMax = hzToX (maxHz);
    const float xNow = hzToX (sweepHz);

    g.setColour (Palette::crispWhite.withAlpha (0.04f));
    g.fillRect (xMin, b.getY() + 22.0f, juce::jmax (2.0f, xMax - xMin), b.getHeight() - 38.0f);
    g.setColour (Palette::vibrantRed.withAlpha (0.85f));
    g.drawVerticalLine (static_cast<int> (xNow), b.getY() + 22.0f, b.getBottom() - 16.0f);

    // Live Negative-Space Envelope Meter on right edge
    const float negEnv = processor.getCurrentNegEnv();
    const float meterH = (b.getHeight() - 36.0f) * negEnv;
    g.setColour (Palette::vibrantRed.withAlpha (0.75f));
    g.fillRoundedRectangle (b.getRight() - 10.0f, b.getBottom() - 18.0f - meterH, 5.0f, meterH, 2.0f);

    // Sweep & Reverb Readout
    g.setColour (Palette::crispWhite.withAlpha (0.85f));
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    const juce::String modeStr = negGhost ? "NEGATIVE GHOST" : "CARVE CUTS";
    g.drawText ("REVERB + NEGATIVE PHASER MODULATION  |  " + modeStr + "  |  SWEEP: " + juce::String (static_cast<int> (sweepHz)) + " Hz",
                b.reduced (10.0f, 6.0f), juce::Justification::topLeft);

    g.setColour (Palette::panelBorder);
    g.drawRoundedRectangle (b, 6.0f, 1.0f);
}

// ==============================================================================
NegaPhaseAudioProcessorEditor::NegaPhaseAudioProcessorEditor (NegaPhaseAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&customLnf);
    logoImage = juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize);

    addAndMakeVisible (analyzer);

    attachKnob (rateSlider,     "phaser_rate",     "RATE",     " Hz", Palette::electricYel);
    attachKnob (depthSlider,    "phaser_depth",    "DEPTH",    {},    Palette::electricYel);
    attachKnob (centerSlider,   "phaser_center",   "CENTER",   " Hz", Palette::electricYel);
    attachKnob (feedbackSlider, "phaser_feedback", "FEEDBACK", {},    Palette::vibrantRed);

    stagesBox.addItemList ({ "4", "6", "8", "12" }, 1);
    addAndMakeVisible (stagesBox);
    stagesAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (audioProcessor.apvts, "phaser_stages", stagesBox);

    modeBox.addItemList ({ "Carve Cuts", "Negative Ghost" }, 1);
    addAndMakeVisible (modeBox);
    modeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (audioProcessor.apvts, "imprint_mode", modeBox);

    attachKnob (imprintSlider,  "imprint_amount", "IMPRINT",   {}, Palette::vibrantRed);
    attachKnob (dynTrackSlider, "dyn_duck",       "DYN TRACK", {}, Palette::vibrantRed);

    attachKnob (sizeSlider,     "rev_size",     "SIZE",     {},    Palette::crispWhite);
    attachKnob (dampSlider,     "rev_damp",     "DAMP",     {},    Palette::crispWhite);
    attachKnob (widthSlider,    "rev_width",    "WIDTH",    {},    Palette::crispWhite);
    attachKnob (preDelaySlider, "rev_predelay", "PRE-DLY",  " ms", Palette::crispWhite);

    attachKnob (mixSlider,     "dry_wet",  "MIX",    {},    Palette::electricYel);
    attachKnob (outGainSlider, "out_gain", "OUTPUT", " dB", Palette::crispWhite);

    setSize (920, 540);
    startTimerHz (30);
}

NegaPhaseAudioProcessorEditor::~NegaPhaseAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void NegaPhaseAudioProcessorEditor::timerCallback()
{
    analyzer.repaint();
}

void NegaPhaseAudioProcessorEditor::attachKnob (juce::Slider& s, const juce::String& paramId, const juce::String& name,
                                                const juce::String& suffix, juce::Colour accent)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 14);
    s.setColour (juce::Slider::rotarySliderFillColourId, accent);
    if (suffix.isNotEmpty())
        s.setTextValueSuffix (suffix);
    addAndMakeVisible (s);

    auto lbl = std::make_unique<juce::Label>();
    lbl->setText (name, juce::dontSendNotification);
    lbl->setJustificationType (juce::Justification::centred);
    lbl->setColour (juce::Label::textColourId, Palette::mutedGrey);
    lbl->setFont (juce::FontOptions (9.5f, juce::Font::bold));
    addAndMakeVisible (*lbl);
    labels.push_back (std::move (lbl));

    sliderAtts.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (audioProcessor.apvts, paramId, s));
}

void NegaPhaseAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::bgObsidian);

    if (logoImage.isValid())
        g.drawImageWithin (logoImage, 14, 6, 36, 36, juce::RectanglePlacement::centred);

    g.setColour (Palette::crispWhite);
    g.setFont (juce::FontOptions (19.0f, juce::Font::bold));
    g.drawText ("MASSIVE BLACK", 56, 5, 280, 22, juce::Justification::left);

    g.setColour (Palette::electricYel);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("SnoopyChu  |  Negative-Space Phaser -> Reverb Tail Imprint", 56, 26, 440, 16, juce::Justification::left);

    // Bottom 3 Control Panels
    g.setColour (Palette::panelDark);
    g.fillRoundedRectangle (14.0f, 344.0f, 364.0f, 182.0f, 6.0f);
    g.fillRoundedRectangle (388.0f, 344.0f, 180.0f, 182.0f, 6.0f);
    g.fillRoundedRectangle (578.0f, 344.0f, 328.0f, 182.0f, 6.0f);

    g.setColour (Palette::panelBorder);
    g.drawRoundedRectangle (14.0f, 344.0f, 364.0f, 182.0f, 6.0f, 1.0f);
    g.drawRoundedRectangle (388.0f, 344.0f, 180.0f, 182.0f, 6.0f, 1.0f);
    g.drawRoundedRectangle (578.0f, 344.0f, 328.0f, 182.0f, 6.0f, 1.0f);

    g.setColour (Palette::mutedGrey);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("NEGATIVE PHASER", 24,  350, 180, 14, juce::Justification::left);
    g.drawText ("IMPRINT",         398, 350, 140, 14, juce::Justification::left);
    g.drawText ("REVERB & MIX",    588, 350, 180, 14, juce::Justification::left);
}

void NegaPhaseAudioProcessorEditor::resized()
{
    analyzer.setBounds (14, 50, getWidth() - 28, 284);
    if (labels.size() < 12) return;

    auto placeKnob = [] (juce::Slider& s, juce::Label& l, int x, int y)
    {
        l.setBounds (x, y, 58, 12);
        s.setBounds (x, y + 12, 58, 54);
    };

    // Negative Phaser Panel
    placeKnob (rateSlider,     *labels[0], 24,  372);
    placeKnob (depthSlider,    *labels[1], 92,  372);
    placeKnob (centerSlider,   *labels[2], 160, 372);
    placeKnob (feedbackSlider, *labels[3], 228, 372);
    stagesBox.setBounds (298, 392, 68, 24);
    modeBox.setBounds   (24,  462, 140, 26);

    // Imprint Panel
    placeKnob (imprintSlider,  *labels[4], 404, 372);
    placeKnob (dynTrackSlider, *labels[5], 486, 372);

    // Reverb & Mix Panel
    placeKnob (sizeSlider,     *labels[6],  588, 372);
    placeKnob (dampSlider,     *labels[7],  652, 372);
    placeKnob (widthSlider,    *labels[8],  716, 372);
    placeKnob (preDelaySlider, *labels[9],  780, 372);
    placeKnob (mixSlider,      *labels[10], 716, 446);
    placeKnob (outGainSlider,  *labels[11], 780, 446);
}
