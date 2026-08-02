#include "Audio/VoiceFilter.h"

#include <cmath>

#include <nierika_dsp/nierika_dsp.h>

namespace audio
{

void VoiceFilter::prepare(double sampleRate, int numChannels)
{
    _sampleRate = sampleRate;
    _numChannels = numChannels;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = 512;
    spec.numChannels = 1; // each stage handles exactly one logical (left- or right-) channel

    for (auto& stage : _leftStages)
    {
        stage.prepare(spec);
        stage.reset();
    }

    for (auto& stage : _rightStages)
    {
        stage.prepare(spec);
        stage.reset();
    }
}

void VoiceFilter::reset()
{
    for (auto& stage : _leftStages)
        stage.reset();

    for (auto& stage : _rightStages)
        stage.reset();
}

void VoiceFilter::setKeyTrackedNote(int midiNoteNumber, int referenceNote) noexcept
{
    _playedNote = midiNoteNumber;
    _referenceNote = referenceNote;
}

void VoiceFilter::setBlockParameters(const BlockParameters& parameters) noexcept
{
    _parameters = parameters;

    for (auto& stage : _leftStages)
    {
        stage.setType(parameters.type);
        stage.setResonance(parameters.resonance);
    }

    for (auto& stage : _rightStages)
    {
        stage.setType(parameters.type);
        stage.setResonance(parameters.resonance);
    }
}

VoiceFilter::StereoSample VoiceFilter::processSample(float dryLeft, float dryRight, float lfoLeft, float lfoRight) noexcept
{
    const auto drivenLeft = driveSample(dryLeft);
    const auto nyquistLimit = static_cast<float>(_sampleRate) * 0.49f;
    const auto numStages = juce::jlimit(0, static_cast<int>(_leftStages.size()), _parameters.numStages);

    const auto leftCutoff = juce::jlimit(20.0f, nyquistLimit, computeChannelCutoffHz(lfoLeft));
    auto leftWet = drivenLeft;
    for (int stage = 0; stage < numStages; ++stage)
    {
        auto& filter = _leftStages[static_cast<std::size_t>(stage)];
        filter.setCutoffFrequency(leftCutoff);
        leftWet = filter.processSample(0, leftWet);
    }

    auto rightWet = leftWet;
    if (_numChannels > 1)
    {
        const auto drivenRight = driveSample(dryRight);
        const auto rightCutoff = juce::jlimit(20.0f, nyquistLimit, computeChannelCutoffHz(lfoRight));
        rightWet = drivenRight;
        for (int stage = 0; stage < numStages; ++stage)
        {
            auto& filter = _rightStages[static_cast<std::size_t>(stage)];
            filter.setCutoffFrequency(rightCutoff);
            rightWet = filter.processSample(0, rightWet);
        }
    }

    // Deliberately "linear" rather than nierika_dsp's usual "balanced" DryWetMixingRule default -
    // "balanced" caps both dry and wet at 0.5 (a built-in -6dB dip at 100% wet), which would make
    // the plugin's default mixPercent=100 quieter than today's un-filtered sound. "linear" gives
    // 100% mix = fully wet at unity gain, matching the "sonically unchanged at defaults" goal.
    const auto [dryGain, wetGain] = ndsp::DryWetUtils::splitDryWetValue<float>(
        juce::jlimit(0.0f, 1.0f, _parameters.mixPercent / 100.0f), juce::dsp::DryWetMixingRule::linear);

    return { dryLeft * dryGain + leftWet * wetGain, dryRight * dryGain + rightWet * wetGain };
}

float VoiceFilter::computeChannelCutoffHz(float lfoValue) const noexcept
{
    const auto keyTrackSemitones = static_cast<float>(_playedNote - _referenceNote) * (_parameters.keyTrackAmountPercent / 100.0f);
    const auto keyTrackMultiplier = std::pow(2.0f, keyTrackSemitones / 12.0f);
    const auto lfoMultiplier = std::pow(2.0f, _parameters.lfoDepthOctaves * lfoValue);

    return _parameters.baseCutoffHz * keyTrackMultiplier * lfoMultiplier;
}

float VoiceFilter::driveSample(float input) const noexcept
{
    const auto preGain = juce::Decibels::decibelsToGain(_parameters.driveDb);
    return std::tanh(input * preGain);
}

}
