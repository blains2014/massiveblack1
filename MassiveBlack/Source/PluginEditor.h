#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class NegaPhaseLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NegaPhaseLookAndFeel();

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override;

    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox& box) override;
};

class SweepAnalyzerView : public juce::Component
{
public:
    explicit SweepAnalyzerView (NegaPhaseAudioProcessor& proc) : processor (proc) {}
    void paint (juce::Graphics& g) override;
private:
    NegaPhaseAudioProcessor& processor;
};

class NegaPhaseAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NegaPhaseAudioProcessorEditor (NegaPhaseAudioProcessor&);
    ~NegaPhaseAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    NegaPhaseAudioProcessor& audioProcessor;
    NegaPhaseLookAndFeel customLnf;
    juce::Image logoImage;
    SweepAnalyzerView analyzer { audioProcessor };

    // Negative Phaser Knobs
    juce::Slider rateSlider, depthSlider, centerSlider, feedbackSlider;
    juce::ComboBox stagesBox, modeBox;

    // Imprint Knobs
    juce::Slider imprintSlider, dynTrackSlider;

    // Reverb Tail Knobs
    juce::Slider sizeSlider, dampSlider, widthSlider, preDelaySlider;

    // Master Knobs
    juce::Slider mixSlider, outGainSlider;

    std::vector<std::unique_ptr<juce::Label>> labels;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtts;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> stagesAtt, modeAtt;

    void attachKnob (juce::Slider& s, const juce::String& paramId, const juce::String& name,
                     const juce::String& suffix, juce::Colour accent);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NegaPhaseAudioProcessorEditor)
};
