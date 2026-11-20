#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

class AllPassChain
{
public:
    static constexpr int maxStages = 12;

    void reset()
    {
        z.fill (0.0f);
        lastOut = 0.0f;
    }

    float processSample (float input, float coeff, int numStages, float feedback)
    {
        float x = input + lastOut * feedback;
        for (int i = 0; i < numStages; ++i)
        {
            const float y = -coeff * x + z[static_cast<size_t> (i)];
            z[static_cast<size_t> (i)] = x + coeff * y;
            x = y;
        }
        lastOut = juce::jlimit (-2.0f, 2.0f, x);
        return lastOut;
    }

private:
    std::array<float, maxStages> z {};
    float lastOut = 0.0f;
};

class NegaPhaseAudioProcessor : public juce::AudioProcessor
{
public:
    NegaPhaseAudioProcessor();
    ~NegaPhaseAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    float getCurrentSweepHz() const { return currentSweepHz.load (std::memory_order_relaxed); }
    float getCurrentNegEnv() const  { return currentNegEnv.load (std::memory_order_relaxed); }
    double getCurrentSampleRate() const { return currentSampleRate; }
    void getTailScopeSamples (std::vector<float>& dest) const;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    double currentSampleRate = 44100.0;
    double lfoPhase = 0.0;
    float negEnvState = 0.0f;

    // Input negative-space phaser (tracks negative space from dry input)
    AllPassChain inPhaserL, inPhaserR;
    // Synchronized reverb-tail imprint phaser (carves/imprints onto the reverb tail)
    AllPassChain revPhaserL, revPhaserR;

    static constexpr int maxPreDelaySamples = 16384;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> preDelayL { maxPreDelaySamples };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> preDelayR { maxPreDelaySamples };

    juce::dsp::Reverb reverb;
    juce::AudioBuffer<float> wetBuffer;

    std::atomic<float> currentSweepHz { 900.0f };
    std::atomic<float> currentNegEnv  { 0.0f };

    static constexpr int scopeSize = 256;
    std::array<std::atomic<float>, scopeSize> tailScopeBuffer;
    std::atomic<int> tailScopeWritePos { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NegaPhaseAudioProcessor)
};
