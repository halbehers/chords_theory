#include "Audio/MasterCompressor.h"

#include <algorithm>
#include <cmath>

namespace audio
{

namespace
{
    constexpr float kMaxThresholdReductionDb = 24.0f; // amount=100% -> threshold = 0 - 24 = -24dB
    constexpr float kRatio = 4.0f;                     // classic gentle-but-audible bus-glue ratio
    constexpr float kKneeDb = 6.0f;                    // soft knee - smooth gain-reduction onset, avoids pumping
    constexpr float kAttackMs = 10.0f;                 // fast enough to tame transients, slow enough to preserve punch
    constexpr float kReleaseMs = 250.0f;                // typical musical/glue release

    // Standard soft-knee gain-reduction computer: 0 below the knee, the usual linear
    // above-threshold formula above it, a quadratic blend through the knee itself.
    float computeSoftKneeGainReductionDb(float levelDb, float thresholdDb, float ratio, float kneeDb) noexcept
    {
        const auto halfKnee = kneeDb * 0.5f;

        if (levelDb <= thresholdDb - halfKnee)
            return 0.0f;
        if (levelDb >= thresholdDb + halfKnee)
            return (thresholdDb - levelDb) * (1.0f - 1.0f / ratio);

        const auto x = levelDb - thresholdDb + halfKnee;
        return -(1.0f - 1.0f / ratio) * x * x / (2.0f * kneeDb);
    }
}

void MasterCompressor::prepare(double sampleRate, int samplesPerBlock, int numChannels)
{
    juce::ignoreUnused(numChannels); // stereo-linked detection is always a single internal channel, regardless of the bus's own channel count

    _envelopeFollower.prepare({ sampleRate, static_cast<juce::uint32>(samplesPerBlock), 1u });
    _envelopeFollower.setLevelCalculationType(juce::dsp::BallisticsFilterLevelCalculationType::peak);
    _envelopeFollower.setAttackTime(kAttackMs);
    _envelopeFollower.setReleaseTime(kReleaseMs);
}

void MasterCompressor::reset()
{
    _envelopeFollower.reset();
}

void MasterCompressor::setBlockParameters(const Parameters& parameters) noexcept
{
    _parameters = parameters;
}

void MasterCompressor::process(juce::AudioBuffer<float>& buffer) noexcept
{
    const auto amount01 = juce::jlimit(0.0f, 1.0f, _parameters.amountPercent / 100.0f);
    if (amount01 <= 0.0f)
        return; // exact bypass - no gain multiply, no envelope-follower state drift while untouched

    const auto thresholdDb = juce::jmap(amount01, 0.0f, -kMaxThresholdReductionDb);
    const auto makeupDb = -thresholdDb * (1.0f - 1.0f / kRatio) * 0.5f;
    const auto makeupGain = juce::Decibels::decibelsToGain(makeupDb);

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    const auto numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        const auto rightSample = right != nullptr ? right[i] : left[i];
        const auto linkedInput = std::max(std::abs(left[i]), std::abs(rightSample));

        const auto envelopeLevel = _envelopeFollower.processSample(0, linkedInput);
        const auto levelDb = juce::Decibels::gainToDecibels(envelopeLevel, -100.0f);

        const auto gainReductionDb = computeSoftKneeGainReductionDb(levelDb, thresholdDb, kRatio, kKneeDb);
        const auto gain = juce::Decibels::decibelsToGain(gainReductionDb) * makeupGain;

        left[i] *= gain;
        if (right != nullptr)
            right[i] *= gain;
    }
}

}
