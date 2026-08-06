#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>

#include <nierika_dsp/nierika_dsp.h>

using ndsp::AddAlgorithm;
using ndsp::AmplitudeModulationAlgorithm;
using ndsp::FmAlgorithm;
using ndsp::Oscillator;
using ndsp::RingModulationAlgorithm;
using ndsp::SerialFoldAlgorithm;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double kSampleRate = 48000.0;

    Oscillator makeOscillator(double sampleRate, float phasePercent, int midiNote = 60)
    {
        Oscillator osc;
        osc.setSampleRate(sampleRate);
        Oscillator::Parameters params;
        params.shape = Oscillator::Shape::Sine;
        params.phasePercent = phasePercent;
        osc.setBlockParameters(params);
        osc.noteOn(midiNote);
        return osc;
    }
}

TEST_CASE("AddAlgorithm: matches osc1 + osc2 exactly, per channel", "[SynthAlgorithm]")
{
    auto reference1 = makeOscillator(kSampleRate, 25.0f);
    auto reference2 = makeOscillator(kSampleRate, 60.0f, 64);

    auto osc1 = makeOscillator(kSampleRate, 25.0f);
    auto osc2 = makeOscillator(kSampleRate, 60.0f, 64);

    AddAlgorithm algorithm;

    for (int i = 0; i < 10; ++i)
    {
        const auto expected1 = reference1.getNextSample();
        const auto expected2 = reference2.getNextSample();
        const auto combined = algorithm.getNextSample(osc1, osc2);

        CHECK_THAT(combined.left, WithinAbs(expected1.left + expected2.left, 1.0e-6f));
        CHECK_THAT(combined.right, WithinAbs(expected1.right + expected2.right, 1.0e-6f));
    }
}

TEST_CASE("FmAlgorithm: amount 0% leaves the carrier unmodulated, matching osc1 alone", "[SynthAlgorithm]")
{
    auto reference = makeOscillator(kSampleRate, 25.0f);

    auto osc1 = makeOscillator(kSampleRate, 25.0f);
    auto osc2 = makeOscillator(kSampleRate, 60.0f, 64);

    FmAlgorithm algorithm;
    algorithm.setBlockParameters(0.0f);

    for (int i = 0; i < 10; ++i)
        CHECK_THAT(algorithm.getNextSample(osc1, osc2).left, WithinAbs(reference.getNextSample().left, 1.0e-4f));
}

TEST_CASE("FmAlgorithm: a nonzero amount measurably perturbs the carrier vs. the amount=0 baseline", "[SynthAlgorithm]")
{
    auto osc1Baseline = makeOscillator(kSampleRate, 25.0f);
    auto osc2Baseline = makeOscillator(kSampleRate, 60.0f, 64);
    FmAlgorithm baselineAlgorithm;
    baselineAlgorithm.setBlockParameters(0.0f);

    auto osc1Modulated = makeOscillator(kSampleRate, 25.0f);
    auto osc2Modulated = makeOscillator(kSampleRate, 60.0f, 64);
    FmAlgorithm modulatedAlgorithm;
    modulatedAlgorithm.setBlockParameters(1.0f);

    auto maxDifference = 0.0f;
    for (int i = 0; i < 20; ++i)
    {
        const auto baseline = baselineAlgorithm.getNextSample(osc1Baseline, osc2Baseline);
        const auto modulated = modulatedAlgorithm.getNextSample(osc1Modulated, osc2Modulated);
        maxDifference = std::max(maxDifference, std::abs(modulated.left - baseline.left));
    }

    CHECK(maxDifference > 0.05f);
}

TEST_CASE("RingModulationAlgorithm: mix 0% matches plain addition exactly", "[SynthAlgorithm]")
{
    auto reference1 = makeOscillator(kSampleRate, 25.0f);
    auto reference2 = makeOscillator(kSampleRate, 60.0f, 64);

    auto osc1 = makeOscillator(kSampleRate, 25.0f);
    auto osc2 = makeOscillator(kSampleRate, 60.0f, 64);

    RingModulationAlgorithm algorithm;
    algorithm.setBlockParameters(0.0f);

    for (int i = 0; i < 10; ++i)
    {
        const auto expected1 = reference1.getNextSample();
        const auto expected2 = reference2.getNextSample();
        CHECK_THAT(algorithm.getNextSample(osc1, osc2).left, WithinAbs(expected1.left + expected2.left, 1.0e-4f));
    }
}

TEST_CASE("RingModulationAlgorithm: mix 100% matches the exact product of osc1 and osc2", "[SynthAlgorithm]")
{
    auto reference1 = makeOscillator(kSampleRate, 25.0f);
    auto reference2 = makeOscillator(kSampleRate, 60.0f, 64);

    auto osc1 = makeOscillator(kSampleRate, 25.0f);
    auto osc2 = makeOscillator(kSampleRate, 60.0f, 64);

    RingModulationAlgorithm algorithm;
    algorithm.setBlockParameters(1.0f);

    for (int i = 0; i < 10; ++i)
    {
        const auto expected1 = reference1.getNextSample();
        const auto expected2 = reference2.getNextSample();
        CHECK_THAT(algorithm.getNextSample(osc1, osc2).left, WithinAbs(expected1.left * expected2.left, 1.0e-4f));
    }
}

TEST_CASE("AmplitudeModulationAlgorithm: depth 0% leaves osc1 untouched, matching osc1 alone", "[SynthAlgorithm]")
{
    auto reference = makeOscillator(kSampleRate, 25.0f);

    auto osc1 = makeOscillator(kSampleRate, 25.0f);
    auto osc2 = makeOscillator(kSampleRate, 60.0f, 64);

    AmplitudeModulationAlgorithm algorithm;
    algorithm.setBlockParameters(0.0f);

    for (int i = 0; i < 10; ++i)
        CHECK_THAT(algorithm.getNextSample(osc1, osc2).left, WithinAbs(reference.getNextSample().left, 1.0e-4f));
}

TEST_CASE("SerialFoldAlgorithm: amount 0% matches osc1 alone (a sine never exceeds the fold's identity range)", "[SynthAlgorithm]")
{
    auto reference = makeOscillator(kSampleRate, 25.0f);

    auto osc1 = makeOscillator(kSampleRate, 25.0f);
    auto osc2 = makeOscillator(kSampleRate, 60.0f, 64);

    SerialFoldAlgorithm algorithm;
    algorithm.setBlockParameters(0.0f);

    for (int i = 0; i < 10; ++i)
        CHECK_THAT(algorithm.getNextSample(osc1, osc2).left, WithinAbs(reference.getNextSample().left, 1.0e-4f));
}

TEST_CASE("SerialFoldAlgorithm: a high amount drives osc1 hard enough to fold back down from its peak", "[SynthAlgorithm]")
{
    // Both oscillators start at their sine peak (phase 0.25 -> sin == 1.0), so osc1's own value is
    // pinned at the overdrive-prone extreme right when osc2's modulator term is also near its own
    // peak - the combination reliably drives osc1*(1+osc2*depth) outside [-1,1] at a high amount,
    // where foldSample's reflection kicks in and the result must differ from the unfolded product.
    auto osc1 = makeOscillator(kSampleRate, 25.0f);
    auto osc2 = makeOscillator(kSampleRate, 25.0f, 64);

    SerialFoldAlgorithm algorithm;
    algorithm.setBlockParameters(1.0f);

    // Both oscillators peak at 1.0f, so the pre-fold value is 1.0 * (1 + 1.0 * 6.0) = 7.0 -
    // foldSample(7.0): (7+1) mod 4 = 0 -> not > 2 -> result = 0 - 1 = -1.0, the same clean,
    // exactly predictable reflection Oscillator's own Fold-at-100% test relies on.
    CHECK_THAT(algorithm.getNextSample(osc1, osc2).left, WithinAbs(-1.0f, 1.0e-3f));
}
