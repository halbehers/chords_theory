#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <tuple>

#include "Audio/Lfo.h"

using audio::Lfo;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double kSampleRate = 1000.0; // 1 sample == 1ms, keeps test math trivial
    constexpr double kHostBpm = 120.0; // unused unless syncEnabled - a placeholder here
}

TEST_CASE("Lfo: Trigger mode resets phase to 0 on noteOn, producing a sine's phase-0 value first", "[Lfo]")
{
    Lfo lfo;
    lfo.setSampleRate(kSampleRate);

    Lfo::Parameters params;
    params.shape = Lfo::Shape::Sine;
    params.freeRateHz = 10.0;
    params.mode = Lfo::Mode::Trigger;
    lfo.setParameters(params, kHostBpm, 0.0);

    lfo.noteOn();
    const auto first = lfo.getNextSample();
    CHECK_THAT(first.left, WithinAbs(0.0f, 1.0e-5f));
}

TEST_CASE("Lfo: Trigger mode phase advances at the configured rate", "[Lfo]")
{
    Lfo lfo;
    lfo.setSampleRate(kSampleRate);

    Lfo::Parameters params;
    params.shape = Lfo::Shape::Sine;
    params.freeRateHz = 10.0; // period = 100 samples at 1000Hz -> phase increments by 0.01/sample
    params.mode = Lfo::Mode::Trigger;
    lfo.setParameters(params, kHostBpm, 0.0);

    lfo.noteOn();
    for (int i = 0; i < 25; ++i)
        std::ignore = lfo.getNextSample(); // discard the first 25 calls (phase 0.00 .. 0.24)

    const auto atQuarterCycle = lfo.getNextSample(); // 26th call: phase = 25 * 0.01 = 0.25 -> the peak
    CHECK_THAT(atQuarterCycle.left, WithinAbs(1.0f, 1.0e-3f));
}

TEST_CASE("Lfo: Free mode phase is driven by the seed, not reset by noteOn", "[Lfo]")
{
    Lfo lfo;
    lfo.setSampleRate(kSampleRate);

    Lfo::Parameters params;
    params.shape = Lfo::Shape::Sine;
    params.mode = Lfo::Mode::Free;
    lfo.setParameters(params, kHostBpm, 0.25); // seed at phase 0.25 (the peak for a sine)

    lfo.noteOn(); // must NOT reset the free-running phase (only Trigger mode's private phase)

    const auto sample = lfo.getNextSample();
    CHECK_THAT(sample.left, WithinAbs(1.0f, 1.0e-5f)); // sin(2*pi*0.25) == 1.0
}

TEST_CASE("Lfo: smooth blends the raw shape toward a pure sine", "[Lfo]")
{
    // Square and sine visibly disagree at phase 0.1 (+1.0 vs ~0.588), unlike e.g. phase 0.25
    // where they coincide - a meaningful differentiator for smooth=0 vs smooth=1.
    Lfo::Parameters params;
    params.shape = Lfo::Shape::Square;
    params.freeRateHz = 10.0;
    params.mode = Lfo::Mode::Trigger;

    Lfo lfoRaw;
    lfoRaw.setSampleRate(kSampleRate);
    params.smooth = 0.0f;
    lfoRaw.setParameters(params, kHostBpm, 0.0);
    lfoRaw.noteOn();
    for (int i = 0; i < 10; ++i)
        std::ignore = lfoRaw.getNextSample();
    const auto rawAtPhase = lfoRaw.getNextSample(); // phase = 0.10
    CHECK_THAT(rawAtPhase.left, WithinAbs(1.0f, 1.0e-5f)); // raw square: phase < 0.5 -> +1.0 exactly

    Lfo lfoSmooth;
    lfoSmooth.setSampleRate(kSampleRate);
    params.smooth = 1.0f;
    lfoSmooth.setParameters(params, kHostBpm, 0.0);
    lfoSmooth.noteOn();
    for (int i = 0; i < 10; ++i)
        std::ignore = lfoSmooth.getNextSample();
    const auto smoothAtPhase = lfoSmooth.getNextSample(); // phase = 0.10
    const auto expectedSine = static_cast<float>(std::sin(juce::MathConstants<double>::twoPi * 0.10));
    CHECK_THAT(smoothAtPhase.left, WithinAbs(expectedSine, 1.0e-3f)); // fully smoothed -> pure sine
}

TEST_CASE("Lfo: stereoPercent produces the expected left/right phase divergence", "[Lfo]")
{
    Lfo lfo;
    lfo.setSampleRate(kSampleRate);

    Lfo::Parameters params;
    params.shape = Lfo::Shape::Sine;
    params.mode = Lfo::Mode::Free;
    params.stereoPercent = 100.0f; // full divergence: right = left + half a cycle
    lfo.setParameters(params, kHostBpm, 0.25); // seed phase 0.25 -> left at the peak

    const auto sample = lfo.getNextSample();
    CHECK_THAT(sample.left, WithinAbs(1.0f, 1.0e-5f)); // sin(2*pi*0.25) == 1.0
    CHECK_THAT(sample.right, WithinAbs(-1.0f, 1.0e-5f)); // phase 0.75 -> sin(2*pi*0.75) == -1.0
}

TEST_CASE("Lfo: delayMs gates output to silence for exactly that many samples after noteOn", "[Lfo]")
{
    Lfo lfo;
    lfo.setSampleRate(kSampleRate); // 1 sample == 1ms

    Lfo::Parameters params;
    params.shape = Lfo::Shape::Sine;
    params.freeRateHz = 10.0;
    params.mode = Lfo::Mode::Trigger;
    params.delayMs = 10.0f; // 10 samples of silence
    lfo.setParameters(params, kHostBpm, 0.0);

    lfo.noteOn();

    for (int i = 0; i < 10; ++i)
    {
        const auto sample = lfo.getNextSample();
        CHECK_THAT(sample.left, WithinAbs(0.0f, 1.0e-6f));
        CHECK_THAT(sample.right, WithinAbs(0.0f, 1.0e-6f));
    }

    // The phase advances underneath throughout the delay, so once it elapses the modulation is
    // already mid-cycle rather than restarting from 0 - the 11th sample should be clearly nonzero.
    const auto afterDelay = lfo.getNextSample();
    CHECK(std::abs(afterDelay.left) > 1.0e-3f);
}

TEST_CASE("Lfo: noteOff is a safe no-op that doesn't stop or reset the LFO", "[Lfo]")
{
    Lfo lfo;
    lfo.setSampleRate(kSampleRate);

    Lfo::Parameters params;
    params.shape = Lfo::Shape::Sine;
    params.freeRateHz = 10.0;
    params.mode = Lfo::Mode::Trigger;
    lfo.setParameters(params, kHostBpm, 0.0);

    lfo.noteOn();
    for (int i = 0; i < 25; ++i)
        std::ignore = lfo.getNextSample();

    REQUIRE_NOTHROW(lfo.noteOff());

    const auto afterNoteOff = lfo.getNextSample(); // 26th call - phase should keep advancing, not reset
    CHECK_THAT(afterNoteOff.left, WithinAbs(1.0f, 1.0e-3f)); // phase = 0.25 -> the peak, same as the un-released case
}
