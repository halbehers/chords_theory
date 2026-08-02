#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <tuple>

#include "Audio/VoiceFilter.h"

using audio::VoiceFilter;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double kSampleRate = 48000.0;

    // Runs numSamples of a sine wave at testFrequencyHz through filter, returning the RMS of the
    // left channel's output over the second half only (skips the filter's initial settling
    // transient, which would otherwise dilute the steady-state measurement).
    double measureOutputRms(VoiceFilter& filter, float testFrequencyHz, int numSamples = 4800)
    {
        double phase = 0.0;
        const double phaseIncrement = juce::MathConstants<double>::twoPi * static_cast<double>(testFrequencyHz) / kSampleRate;
        double sumSquares = 0.0;

        for (int i = 0; i < numSamples; ++i)
        {
            const auto input = static_cast<float>(std::sin(phase));
            const auto output = filter.processSample(input, input, 0.0f, 0.0f);

            if (i > numSamples / 2)
                sumSquares += static_cast<double>(output.left) * static_cast<double>(output.left);

            phase += phaseIncrement;
            if (phase >= juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;
        }

        return std::sqrt(sumSquares);
    }
}

TEST_CASE("VoiceFilter: a low-pass filter heavily attenuates a sine wave well above its cutoff", "[VoiceFilter]")
{
    VoiceFilter filter;
    filter.prepare(kSampleRate, 2);

    VoiceFilter::BlockParameters params;
    params.type = juce::dsp::StateVariableTPTFilterType::lowpass;
    params.numStages = 1;
    params.baseCutoffHz = 200.0f;
    filter.setBlockParameters(params);

    const auto attenuatedRms = measureOutputRms(filter, 5000.0f);

    VoiceFilter wideOpenFilter;
    wideOpenFilter.prepare(kSampleRate, 2);
    VoiceFilter::BlockParameters wideOpenParams;
    wideOpenParams.baseCutoffHz = 20000.0f;
    wideOpenFilter.setBlockParameters(wideOpenParams);
    const auto passedRms = measureOutputRms(wideOpenFilter, 5000.0f);

    CHECK(attenuatedRms < passedRms * 0.3); // a 200Hz low-pass should heavily attenuate a 5kHz tone
}

TEST_CASE("VoiceFilter: mix=0% reproduces the dry input exactly regardless of filter settings", "[VoiceFilter]")
{
    VoiceFilter filter;
    filter.prepare(kSampleRate, 2);

    VoiceFilter::BlockParameters params;
    params.baseCutoffHz = 100.0f; // aggressive filtering that would otherwise be very audible
    params.mixPercent = 0.0f;
    filter.setBlockParameters(params);

    for (const float input : { 0.0f, 0.3f, -0.5f, 0.9f, -0.9f })
    {
        const auto output = filter.processSample(input, input, 0.0f, 0.0f);
        CHECK_THAT(output.left, WithinAbs(input, 1.0e-6f));
        CHECK_THAT(output.right, WithinAbs(input, 1.0e-6f));
    }
}

TEST_CASE("VoiceFilter: key-tracking shifts the effective cutoff proportionally to the played note", "[VoiceFilter]")
{
    constexpr float testFrequencyHz = 1500.0f; // between 1000Hz (untracked cutoff) and 2000Hz (one octave up)

    VoiceFilter::BlockParameters params;
    params.type = juce::dsp::StateVariableTPTFilterType::lowpass;
    params.numStages = 1;
    params.baseCutoffHz = 1000.0f;
    params.keyTrackAmountPercent = 100.0f;

    VoiceFilter atReferenceNote;
    atReferenceNote.prepare(kSampleRate, 2);
    atReferenceNote.setKeyTrackedNote(60, 60); // played at the reference note - cutoff stays at 1000Hz
    atReferenceNote.setBlockParameters(params);
    const auto rmsAtReferenceNote = measureOutputRms(atReferenceNote, testFrequencyHz);

    VoiceFilter oneOctaveUp;
    oneOctaveUp.prepare(kSampleRate, 2);
    oneOctaveUp.setKeyTrackedNote(72, 60); // one octave above the reference - cutoff shifts to 2000Hz
    oneOctaveUp.setBlockParameters(params);
    const auto rmsOneOctaveUp = measureOutputRms(oneOctaveUp, testFrequencyHz);

    // 1500Hz sits below the tracked-up 2000Hz cutoff but above the untracked 1000Hz one, so it
    // should pass through far less attenuated once key-tracking shifts the cutoff past it.
    CHECK(rmsOneOctaveUp > rmsAtReferenceNote * 1.5);
}

TEST_CASE("VoiceFilter: extreme cutoff settings are clamped below Nyquist and don't trip the filter's internal assert", "[VoiceFilter]")
{
    VoiceFilter filter;
    filter.prepare(kSampleRate, 2);
    filter.setKeyTrackedNote(127, 0); // extreme key-track push

    VoiceFilter::BlockParameters params;
    params.baseCutoffHz = 20000.0f;
    params.keyTrackAmountPercent = 100.0f;
    params.lfoDepthOctaves = 10.0f; // deliberately extreme, stacks with key-tracking
    filter.setBlockParameters(params);

    for (int i = 0; i < 100; ++i)
        std::ignore = filter.processSample(0.5f, 0.5f, 1.0f, 1.0f);

    SUCCEED("processSample completed without tripping StateVariableTPTFilter's Nyquist assert");
}

TEST_CASE("VoiceFilter: a mono voice's right channel mirrors the left channel", "[VoiceFilter]")
{
    VoiceFilter filter;
    filter.prepare(kSampleRate, 1);

    VoiceFilter::BlockParameters params;
    params.baseCutoffHz = 500.0f;
    filter.setBlockParameters(params);

    for (float phaseDeg = 0.0f; phaseDeg < 360.0f; phaseDeg += 15.0f)
    {
        const auto input = std::sin(juce::degreesToRadians(phaseDeg));
        const auto output = filter.processSample(input, input, 0.0f, 0.0f);
        CHECK_THAT(output.right, WithinAbs(output.left, 1.0e-6f));
    }
}
