#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

NegaPhaseAudioProcessor::NegaPhaseAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
#else
    :
#endif
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    for (auto& s : tailScopeBuffer)
        s.store (0.0f);
}

NegaPhaseAudioProcessor::~NegaPhaseAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout NegaPhaseAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Negative Phaser Sweep Controls
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "phaser_rate", "Rate (Hz)", juce::NormalisableRange<float> (0.02f, 8.0f, 0.01f, 0.45f), 0.45f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "phaser_depth", "Depth", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.75f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "phaser_center", "Center (Hz)", juce::NormalisableRange<float> (120.0f, 6000.0f, 1.0f, 0.35f), 900.0f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "phaser_feedback", "Feedback", juce::NormalisableRange<float> (-0.88f, 0.88f, 0.01f), 0.45f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "phaser_stages", "Stages", juce::StringArray { "4", "6", "8", "12" }, 1));

    // Imprint Controls
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "imprint_mode", "Mode", juce::StringArray { "Carve Cuts", "Negative Ghost" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "imprint_amount", "Imprint", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.90f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "dyn_duck", "Dyn Track", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.35f));

    // Reverb Tail Controls
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "rev_size", "Size", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.74f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "rev_damp", "Damping", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.35f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "rev_width", "Width", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.92f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "rev_predelay", "Pre-Delay (ms)", juce::NormalisableRange<float> (0.0f, 150.0f, 0.5f), 12.0f));

    // Master Mix
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "dry_wet", "Mix", juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.55f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "out_gain", "Output (dB)", juce::NormalisableRange<float> (-18.0f, 12.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}

void NegaPhaseAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax (8000.0, sampleRate);
    lfoPhase = 0.0;
    negEnvState = 0.0f;

    inPhaserL.reset();
    inPhaserR.reset();
    revPhaserL.reset();
    revPhaserR.reset();

    juce::dsp::ProcessSpec spec { currentSampleRate, static_cast<juce::uint32> (juce::jmax (1, samplesPerBlock)), 2 };
    preDelayL.prepare (spec);
    preDelayR.prepare (spec);
    preDelayL.reset();
    preDelayR.reset();

    reverb.prepare (spec);
    reverb.reset();

    wetBuffer.setSize (2, juce::jmax (1, samplesPerBlock));
}

void NegaPhaseAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool NegaPhaseAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}
#endif

void NegaPhaseAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0 || buffer.getNumChannels() < 1)
        return;

    const float rate       = apvts.getRawParameterValue ("phaser_rate")->load();
    const float depth      = apvts.getRawParameterValue ("phaser_depth")->load();
    const float centerHz   = apvts.getRawParameterValue ("phaser_center")->load();
    const float feedback   = apvts.getRawParameterValue ("phaser_feedback")->load();
    const int stagesIdx    = static_cast<int> (apvts.getRawParameterValue ("phaser_stages")->load());
    const int stageCounts[4] = { 4, 6, 8, 12 };
    const int numStages    = stageCounts[juce::jlimit (0, 3, stagesIdx)];

    const int modeIdx      = static_cast<int> (apvts.getRawParameterValue ("imprint_mode")->load());
    const bool negativeGhost = (modeIdx == 1);
    const float imprintAmt = apvts.getRawParameterValue ("imprint_amount")->load();
    const float dynTrack   = apvts.getRawParameterValue ("dyn_duck")->load();

    const float revSize    = apvts.getRawParameterValue ("rev_size")->load();
    const float revDamp    = apvts.getRawParameterValue ("rev_damp")->load();
    const float revWidth   = apvts.getRawParameterValue ("rev_width")->load();
    const float preDelayMs = apvts.getRawParameterValue ("rev_predelay")->load();

    const float mix        = apvts.getRawParameterValue ("dry_wet")->load();
    const float outGain    = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("out_gain")->load());

    wetBuffer.setSize (2, numSamples, false, false, true);

    const float preDelaySamples = juce::jlimit (0.0f, static_cast<float> (maxPreDelaySamples - 2),
                                                (preDelayMs / 1000.0f) * static_cast<float> (currentSampleRate));
    preDelayL.setDelay (preDelaySamples);
    preDelayR.setDelay (preDelaySamples);

    const float* inL = buffer.getReadPointer (0);
    const float* inR = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : inL;

    // 1. Feed Pre-Delay -> 100% Wet Reverb Tail
    for (int s = 0; s < numSamples; ++s)
    {
        preDelayL.pushSample (0, inL[s]);
        preDelayR.pushSample (0, inR[s]);
        wetBuffer.setSample (0, s, preDelayL.popSample (0));
        wetBuffer.setSample (1, s, preDelayR.popSample (0));
    }

    juce::dsp::Reverb::Parameters revParams;
    revParams.roomSize = revSize;
    revParams.damping  = revDamp;
    revParams.width    = revWidth;
    revParams.wetLevel = 1.0f;
    revParams.dryLevel = 0.0f;
    reverb.setParameters (revParams);

    juce::dsp::AudioBlock<float> wetBlock (wetBuffer);
    juce::dsp::ProcessContextReplacing<float> wetCtx (wetBlock);
    reverb.process (wetCtx);

    // 2. Real-Time Negative Phaser Imprint on Reverb Tail
    const double phaseInc = (juce::MathConstants<double>::twoPi * rate) / currentSampleRate;
    const float envAtk = 0.015f;
    const float envRel = 0.0008f;

    float lastSweepHz = centerHz;

    for (int s = 0; s < numSamples; ++s)
    {
        lfoPhase += phaseInc;
        if (lfoPhase >= juce::MathConstants<double>::twoPi)
            lfoPhase -= juce::MathConstants<double>::twoPi;

        // Bipolar exponential sweep around centerHz
        const float lfoL = static_cast<float> (std::sin (lfoPhase));
        const float lfoR = static_cast<float> (std::sin (lfoPhase + 0.55)); // stereo spread

        const float octRange = depth * 2.6f;
        const float freqL = juce::jlimit (40.0f, static_cast<float> (currentSampleRate * 0.42),
                                          centerHz * std::pow (2.0f, lfoL * octRange));
        const float freqR = juce::jlimit (40.0f, static_cast<float> (currentSampleRate * 0.42),
                                          centerHz * std::pow (2.0f, lfoR * octRange));
        lastSweepHz = freqL;

        auto calcCoeff = [this] (float f) -> float
        {
            const float tanVal = std::tan (juce::MathConstants<float>::pi * f / static_cast<float> (currentSampleRate));
            return (tanVal - 1.0f) / (tanVal + 1.0f);
        };

        const float cL = calcCoeff (freqL);
        const float cR = calcCoeff (freqR);

        // Extract input's negative-space energy for dynamic tracking
        const float dryL = inL[s];
        const float dryR = inR[s];
        const float apInL = inPhaserL.processSample (dryL, cL, numStages, feedback);
        const float apInR = inPhaserR.processSample (dryR, cR, numStages, feedback);
        const float negIn = 0.5f * ((dryL - apInL) + (dryR - apInR));
        const float absNeg = std::abs (negIn);
        negEnvState += (absNeg > negEnvState ? envAtk : envRel) * (absNeg - negEnvState);

        // Dynamic imprint depth boost from the negative-space envelope
        const float effectiveImprint = juce::jlimit (0.0f, 1.0f, imprintAmt * (1.0f - dynTrack * 0.4f + dynTrack * juce::jmin (1.0f, negEnvState * 3.0f)));

        // Imprint onto the Reverb Tail
        const float tailL = wetBuffer.getSample (0, s);
        const float tailR = wetBuffer.getSample (1, s);

        const float apRevL = revPhaserL.processSample (tailL, cL, numStages, feedback);
        const float apRevR = revPhaserR.processSample (tailR, cR, numStages, feedback);

        float imprintedL, imprintedR;
        if (negativeGhost)
        {
            // Negative Ghost: isolates only the negative-space frequencies inside the reverb tail
            const float ghostL = 0.5f * (tailL - apRevL);
            const float ghostR = 0.5f * (tailR - apRevR);
            imprintedL = tailL * (1.0f - effectiveImprint) + ghostL * effectiveImprint * 1.4f;
            imprintedR = tailR * (1.0f - effectiveImprint) + ghostR * effectiveImprint * 1.4f;
        }
        else
        {
            // Carve Cuts: slices the exact sweeping notch pattern directly out of the reverb tail
            const float carvedL = 0.5f * (tailL + apRevL);
            const float carvedR = 0.5f * (tailR + apRevR);
            imprintedL = tailL * (1.0f - effectiveImprint) + carvedL * effectiveImprint;
            imprintedR = tailR * (1.0f - effectiveImprint) + carvedR * effectiveImprint;
        }

        const int wp = tailScopeWritePos.fetch_add (1, std::memory_order_relaxed) & (scopeSize - 1);
        tailScopeBuffer[static_cast<size_t> (wp)].store (0.5f * (imprintedL + imprintedR), std::memory_order_relaxed);

        const float finalL = juce::jlimit (-1.0f, 1.0f, (dryL * (1.0f - mix) + imprintedL * mix) * outGain);
        const float finalR = juce::jlimit (-1.0f, 1.0f, (dryR * (1.0f - mix) + imprintedR * mix) * outGain);

        buffer.setSample (0, s, finalL);
        if (buffer.getNumChannels() > 1)
            buffer.setSample (1, s, finalR);
    }

    currentSweepHz.store (lastSweepHz, std::memory_order_relaxed);
    currentNegEnv.store (juce::jlimit (0.0f, 1.0f, negEnvState * 2.5f), std::memory_order_relaxed);
}

void NegaPhaseAudioProcessor::getTailScopeSamples (std::vector<float>& dest) const
{
    dest.resize (scopeSize);
    const int wp = tailScopeWritePos.load (std::memory_order_relaxed);
    for (int i = 0; i < scopeSize; ++i)
    {
        const int idx = (wp + i) & (scopeSize - 1);
        dest[static_cast<size_t> (i)] = tailScopeBuffer[static_cast<size_t> (idx)].load (std::memory_order_relaxed);
    }
}

juce::AudioProcessorEditor* NegaPhaseAudioProcessor::createEditor()
{
    return new NegaPhaseAudioProcessorEditor (*this);
}

void NegaPhaseAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void NegaPhaseAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NegaPhaseAudioProcessor();
}
