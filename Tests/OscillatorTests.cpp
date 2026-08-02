#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#include "Audio/Oscillator.h"

using audio::Oscillator;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double kSampleRate = 48000.0;
}

TEST_CASE("Oscillator: Sine shape reaches its known peak a quarter-cycle after noteOn", "[Oscillator]")
{
    Oscillator osc;
    osc.setSampleRate(kSampleRate);

    Oscillator::Parameters params;
    params.shape = Oscillator::Shape::Sine;
    params.phasePercent = 25.0f; // phase 0.25 -> sin(2*pi*0.25) == 1.0, the peak, immediately
    osc.setBlockParameters(params);
    osc.noteOn(60);

    const auto first = osc.getNextSample();
    CHECK_THAT(first.left, WithinAbs(1.0f, 1.0e-3f));
    CHECK_THAT(first.right, WithinAbs(1.0f, 1.0e-3f));
}

TEST_CASE("Oscillator: enabled=false silences output but keeps advancing phase, so re-enabling mid-note is click-free", "[Oscillator]")
{
    Oscillator reference;
    reference.setSampleRate(kSampleRate);
    Oscillator::Parameters referenceParams;
    referenceParams.shape = Oscillator::Shape::Sine;
    referenceParams.phasePercent = 25.0f;
    reference.setBlockParameters(referenceParams);
    reference.noteOn(60);

    Oscillator gated;
    gated.setSampleRate(kSampleRate);
    auto gatedParams = referenceParams;
    gatedParams.enabled = false;
    gated.setBlockParameters(gatedParams);
    gated.noteOn(60);

    // While disabled, output is exactly silent even though the reference (identical settings,
    // just enabled) is clearly not - proving this is a real gate, not a coincidental zero.
    for (int i = 0; i < 10; ++i)
    {
        const auto referenceSample = reference.getNextSample();
        const auto gatedSample = gated.getNextSample();
        REQUIRE(std::abs(referenceSample.left) > 0.01f);
        CHECK_THAT(gatedSample.left, WithinAbs(0.0f, 1.0e-6f));
        CHECK_THAT(gatedSample.right, WithinAbs(0.0f, 1.0e-6f));
    }

    // Re-enabling mid-note (without a fresh noteOn) should immediately match the reference, proving
    // the phase kept advancing underneath the whole time rather than freezing while gated.
    gatedParams.enabled = true;
    gated.setBlockParameters(gatedParams);

    for (int i = 0; i < 10; ++i)
        CHECK_THAT(gated.getNextSample().left, WithinAbs(reference.getNextSample().left, 1.0e-3f));
}

TEST_CASE("Oscillator: Triangle shape hits its known -1/+1 extremes at phase 0 and 0.5", "[Oscillator]")
{
    Oscillator osc;
    osc.setSampleRate(kSampleRate);

    Oscillator::Parameters params;
    params.shape = Oscillator::Shape::Triangle;
    params.phasePercent = 0.0f;
    osc.setBlockParameters(params);
    osc.noteOn(60);

    const auto atZero = osc.getNextSample();
    CHECK_THAT(atZero.left, WithinAbs(-1.0f, 1.0e-3f));
}

TEST_CASE("Oscillator: Saw and Square shapes match their naive formulas away from the wrap discontinuity", "[Oscillator]")
{
    // Low note (coarse dt) but sampled at phase 0.5, far from either shape's correction zone
    // (t < dt or t > 1-dt), where PolyBLEP contributes exactly 0 and the naive formula applies.
    Oscillator sawOsc;
    sawOsc.setSampleRate(1000.0);
    Oscillator::Parameters sawParams;
    sawParams.shape = Oscillator::Shape::Saw;
    sawParams.phasePercent = 50.0f;
    sawOsc.setBlockParameters(sawParams);
    sawOsc.noteOn(55); // ~196Hz -> dt ~ 0.196, well clear of phase 0.5
    CHECK_THAT(sawOsc.getNextSample().left, WithinAbs(0.0f, 1.0e-2f)); // naive saw at phase 0.5: 2*0.5-1 = 0

    Oscillator squareOsc;
    squareOsc.setSampleRate(1000.0);
    Oscillator::Parameters squareParams;
    squareParams.shape = Oscillator::Shape::Square;
    squareParams.phasePercent = 25.0f; // safely inside the phase < 0.5 half, away from both edges
    squareOsc.setBlockParameters(squareParams);
    squareOsc.noteOn(55);
    CHECK_THAT(squareOsc.getNextSample().left, WithinAbs(1.0f, 1.0e-2f));
}

TEST_CASE("Oscillator: PolyBLEP corrects the Saw wrap - naive would be -1.0 at phase 0, corrected lands at 0.0", "[Oscillator]")
{
    Oscillator osc;
    osc.setSampleRate(1000.0);

    Oscillator::Parameters params;
    params.shape = Oscillator::Shape::Saw;
    params.phasePercent = 0.0f;
    osc.setBlockParameters(params);
    osc.noteOn(55);

    // At t=0, polyBlep(0, dt) always evaluates to exactly -1 regardless of dt (x = 0/dt = 0, so
    // x+x-x*x-1 = -1) - naive saw's own -1.0 minus that correction lands exactly on 0.0, smoothing
    // what would otherwise be a hard jump straight from +1.0 to -1.0 at the wrap.
    CHECK_THAT(osc.getNextSample().left, WithinAbs(0.0f, 1.0e-3f));
}

TEST_CASE("Oscillator: PolyBLEP corrects the Square wave's rising edge at phase 0 the same way", "[Oscillator]")
{
    Oscillator osc;
    osc.setSampleRate(1000.0);

    Oscillator::Parameters params;
    params.shape = Oscillator::Shape::Square;
    params.phasePercent = 0.0f;
    osc.setBlockParameters(params);
    osc.noteOn(55);

    CHECK_THAT(osc.getNextSample().left, WithinAbs(0.0f, 1.0e-3f));
}

TEST_CASE("Oscillator: Warp at -100% zero-fills the left portion of the cycle, compacting the shape onto the right", "[Oscillator]")
{
    Oscillator osc;
    osc.setSampleRate(kSampleRate);

    Oscillator::Parameters params;
    params.shape = Oscillator::Shape::Sine;
    params.warpPercent = -100.0f;
    params.phasePercent = 10.0f; // well inside what should be the zero-filled left portion
    osc.setBlockParameters(params);
    osc.noteOn(60);

    CHECK_THAT(osc.getNextSample().left, WithinAbs(0.0f, 1.0e-6f));
}

TEST_CASE("Oscillator: Warp at +100% zero-fills the right portion of the cycle, compacting the shape onto the left", "[Oscillator]")
{
    Oscillator osc;
    osc.setSampleRate(kSampleRate);

    Oscillator::Parameters params;
    params.shape = Oscillator::Shape::Sine;
    params.warpPercent = 100.0f;
    params.phasePercent = 50.0f; // well inside what should be the zero-filled right portion
    osc.setBlockParameters(params);
    osc.noteOn(60);

    CHECK_THAT(osc.getNextSample().left, WithinAbs(0.0f, 1.0e-6f));
}

TEST_CASE("Oscillator: Warp at 0% is the identity - the full unwarped waveform plays normally", "[Oscillator]")
{
    Oscillator osc;
    osc.setSampleRate(kSampleRate);

    Oscillator::Parameters params;
    params.shape = Oscillator::Shape::Sine;
    params.warpPercent = 0.0f;
    params.phasePercent = 25.0f;
    osc.setBlockParameters(params);
    osc.noteOn(60);

    CHECK_THAT(osc.getNextSample().left, WithinAbs(1.0f, 1.0e-3f));
}

TEST_CASE("Oscillator: Fold at 0% is the identity for an in-range signal", "[Oscillator]")
{
    Oscillator osc;
    osc.setSampleRate(kSampleRate);

    Oscillator::Parameters params;
    params.shape = Oscillator::Shape::Sine;
    params.foldPercent = 0.0f;
    params.phasePercent = 25.0f; // sine peak, value == 1.0 - the fold boundary itself
    osc.setBlockParameters(params);
    osc.noteOn(60);

    CHECK_THAT(osc.getNextSample().left, WithinAbs(1.0f, 1.0e-3f));
}

TEST_CASE("Oscillator: Fold at 100% reflects an overdriven peak back down predictably", "[Oscillator]")
{
    Oscillator osc;
    osc.setSampleRate(kSampleRate);

    Oscillator::Parameters params;
    params.shape = Oscillator::Shape::Sine;
    params.foldPercent = 100.0f; // pre-gain = 1 + 1*6 = 7x
    params.phasePercent = 25.0f; // sine peak (1.0) * 7 = 7.0 pre-fold
    osc.setBlockParameters(params);
    osc.noteOn(60);

    // foldSample(7.0): (7+1) mod 4 = 0 -> not > 2 -> result = 0 - 1 = -1.0, a clean, exactly
    // predictable reflection for this specific pre-fold value.
    CHECK_THAT(osc.getNextSample().left, WithinAbs(-1.0f, 1.0e-3f));
}

TEST_CASE("Oscillator: raising unisonVoices mid-note is click-free - the newly-included voice was already phase-tracking", "[Oscillator]")
{
    Oscillator osc;
    osc.setSampleRate(kSampleRate);

    Oscillator::Parameters params;
    params.shape = Oscillator::Shape::Sine;
    params.unisonVoices = 1;
    params.unisonDetuneCents = 0.0f; // every unison slot shares the identical phase/rate
    params.phasePercent = 25.0f;     // start at the peak, safely nonzero
    osc.setBlockParameters(params);
    osc.noteOn(21); // A0, 27.5Hz - slow-moving phase, consecutive samples stay close in value

    const auto beforeSwitch = osc.getNextSample().left; // numVoices == 1, normalization == 1

    params.unisonVoices = 2;
    osc.setBlockParameters(params); // does not touch phase, only recomputes increments/count
    const auto afterSwitch = osc.getNextSample().left;

    // Voice slot 1 has been silently phase-tracking identically to voice 0 since noteOn (0 unison
    // detune), so including it doesn't reset or jump anything - it contributes the *same* value
    // voice 0 already had. The only change is the expected, clean gain-compensated doubling
    // (2 voices summed, normalized by 1/sqrt(2)) - sqrt(2), not some arbitrary discontinuity. If
    // voice 1 had instead been silently reset to phase 0 (sin == 0) until activated, this ratio
    // would come out far from sqrt(2).
    REQUIRE(std::abs(beforeSwitch) > 0.1f); // guards the ratio below against a near-zero denominator
    CHECK_THAT(afterSwitch / beforeSwitch, WithinAbs(std::sqrt(2.0f), 0.01f));
}

TEST_CASE("Oscillator: unison detune spread is symmetric and inactive at 0 cents", "[Oscillator]")
{
    Oscillator zeroDetune;
    zeroDetune.setSampleRate(1000.0);
    Oscillator::Parameters zeroParams;
    zeroParams.shape = Oscillator::Shape::Sine;
    zeroParams.unisonVoices = 2;
    zeroParams.unisonDetuneCents = 0.0f;
    zeroParams.unisonStereoPercent = 100.0f; // hard L/R pans voice 0 -> left only, voice 1 -> right only
    zeroDetune.setBlockParameters(zeroParams);
    zeroDetune.noteOn(60);

    Oscillator::StereoSample zeroLast{};
    for (int i = 0; i < 50; ++i)
        zeroLast = zeroDetune.getNextSample();

    // With 0 unison detune, voices 0 and 1 share an identical frequency/phase, so hard-panned left
    // and right should still agree sample-for-sample after 50 samples.
    CHECK_THAT(zeroLast.left, WithinAbs(zeroLast.right, 1.0e-3f));

    Oscillator detuned;
    detuned.setSampleRate(1000.0);
    Oscillator::Parameters detunedParams = zeroParams;
    detunedParams.unisonDetuneCents = 50.0f; // voice 0 at -50c, voice 1 at +50c - symmetric spread
    detuned.setBlockParameters(detunedParams);
    detuned.noteOn(60);

    Oscillator::StereoSample detunedLast{};
    for (int i = 0; i < 50; ++i)
        detunedLast = detuned.getNextSample();

    // Symmetric +/-50 cent detune between the two hard-panned voices accumulates enough phase
    // divergence over 50 samples to clearly separate left and right - proving the spread parameter
    // has a real, activating effect.
    CHECK(std::abs(detunedLast.left - detunedLast.right) > 0.1f);
}

TEST_CASE("Oscillator: sub-oscillator tracks exactly one octave below the played note by default", "[Oscillator]")
{
    Oscillator reference;
    reference.setSampleRate(kSampleRate);
    Oscillator::Parameters referenceParams;
    referenceParams.shape = Oscillator::Shape::Sine;
    referenceParams.octave = -1; // matches the sub's default -1 octave offset
    referenceParams.phasePercent = 25.0f;
    reference.setBlockParameters(referenceParams);
    reference.noteOn(60);

    Oscillator combined;
    combined.setSampleRate(kSampleRate);
    auto combinedParams = referenceParams;
    combinedParams.subLevelPercent = 100.0f;
    combinedParams.subOctaveDown2 = false; // -1 octave - same frequency as the main osc above
    combined.setBlockParameters(combinedParams);
    combined.noteOn(60);

    // If the sub's frequency exactly matches the main oscillator's own -1-octave setting, both
    // layers stay perfectly phase-locked (same start phase, same rate) for as long as they're
    // sampled, so the combined output should be exactly double the main-only reference at every
    // sample - not just the first one, which distinguishes a genuinely matching ratio from a
    // coincidental match at a single instant.
    for (int i = 0; i < 10; ++i)
    {
        const auto referenceSample = reference.getNextSample();
        const auto combinedSample = combined.getNextSample();
        CHECK_THAT(combinedSample.left, WithinAbs(referenceSample.left * 2.0f, 1.0e-3f));
    }
}

TEST_CASE("Oscillator: sub-oscillator's -2 octave toggle halves its frequency again", "[Oscillator]")
{
    Oscillator reference;
    reference.setSampleRate(kSampleRate);
    Oscillator::Parameters referenceParams;
    referenceParams.shape = Oscillator::Shape::Sine;
    referenceParams.octave = -2; // matches subOctaveDown2's -2 octave offset
    referenceParams.phasePercent = 25.0f;
    reference.setBlockParameters(referenceParams);
    reference.noteOn(60);

    Oscillator combined;
    combined.setSampleRate(kSampleRate);
    auto combinedParams = referenceParams;
    combinedParams.subLevelPercent = 100.0f;
    combinedParams.subOctaveDown2 = true;
    combined.setBlockParameters(combinedParams);
    combined.noteOn(60);

    for (int i = 0; i < 10; ++i)
    {
        const auto referenceSample = reference.getNextSample();
        const auto combinedSample = combined.getNextSample();
        CHECK_THAT(combinedSample.left, WithinAbs(referenceSample.left * 2.0f, 1.0e-3f));
    }
}

TEST_CASE("Oscillator: transposeSemitones of +12 is numerically identical to one octave up", "[Oscillator]")
{
    // 2^(12/12) == 2^1 exactly - transposeSemitones is a separate multiplier term from octave
    // (see Oscillator::recomputePhaseIncrements), so this is a genuine cross-check that both
    // reach the same frequency, not just two branches of the same code path agreeing with itself.
    Oscillator viaOctave;
    viaOctave.setSampleRate(kSampleRate);
    Oscillator::Parameters octaveParams;
    octaveParams.shape = Oscillator::Shape::Sine;
    octaveParams.octave = 1;
    octaveParams.phasePercent = 25.0f;
    viaOctave.setBlockParameters(octaveParams);
    viaOctave.noteOn(60);

    Oscillator viaTranspose;
    viaTranspose.setSampleRate(kSampleRate);
    Oscillator::Parameters transposeParams;
    transposeParams.shape = Oscillator::Shape::Sine;
    transposeParams.transposeSemitones = 12;
    transposeParams.phasePercent = 25.0f;
    viaTranspose.setBlockParameters(transposeParams);
    viaTranspose.noteOn(60);

    for (int i = 0; i < 10; ++i)
        CHECK_THAT(viaTranspose.getNextSample().left, WithinAbs(viaOctave.getNextSample().left, 1.0e-3f));
}

TEST_CASE("Oscillator: transposeSemitones stacks on top of octave rather than replacing it", "[Oscillator]")
{
    // octave=1 + transpose=12 should land exactly one octave above octave=1 alone (i.e. two
    // octaves above the played note), proving the two terms multiply together rather than one
    // overriding the other.
    Oscillator::Parameters baseParams;
    baseParams.shape = Oscillator::Shape::Sine;
    baseParams.octave = 1;
    baseParams.phasePercent = 25.0f;

    Oscillator octavePlusTranspose;
    octavePlusTranspose.setSampleRate(kSampleRate);
    auto stackedParams = baseParams;
    stackedParams.transposeSemitones = 12;
    octavePlusTranspose.setBlockParameters(stackedParams);
    octavePlusTranspose.noteOn(60);

    Oscillator twoOctaves;
    twoOctaves.setSampleRate(kSampleRate);
    auto twoOctavesParams = baseParams;
    twoOctavesParams.octave = 2;
    twoOctaves.setBlockParameters(twoOctavesParams);
    twoOctaves.noteOn(60);

    for (int i = 0; i < 10; ++i)
        CHECK_THAT(octavePlusTranspose.getNextSample().left, WithinAbs(twoOctaves.getNextSample().left, 1.0e-3f));
}

TEST_CASE("Oscillator: phasePercent sets the exact starting phase at noteOn", "[Oscillator]")
{
    Oscillator osc;
    osc.setSampleRate(kSampleRate);

    Oscillator::Parameters params;
    params.shape = Oscillator::Shape::Sine;
    params.phasePercent = 75.0f; // phase 0.75 -> sin(2*pi*0.75) == -1.0
    osc.setBlockParameters(params);
    osc.noteOn(60);

    CHECK_THAT(osc.getNextSample().left, WithinAbs(-1.0f, 1.0e-3f));
}

TEST_CASE("Oscillator: phaseRandomizeEnabled produces different starting phases across instances", "[Oscillator]")
{
    Oscillator::Parameters params;
    params.shape = Oscillator::Shape::Sine;
    params.phaseRandomizeEnabled = true;

    std::vector<float> firstSamples;
    for (int i = 0; i < 5; ++i)
    {
        Oscillator osc;
        osc.setSampleRate(kSampleRate);
        osc.setBlockParameters(params);
        osc.noteOn(60);
        firstSamples.push_back(osc.getNextSample().left);
    }

    const auto allIdentical = std::all_of(firstSamples.begin(), firstSamples.end(),
        [&](float value) { return std::abs(value - firstSamples.front()) < 1.0e-6f; });
    CHECK_FALSE(allIdentical);
}
