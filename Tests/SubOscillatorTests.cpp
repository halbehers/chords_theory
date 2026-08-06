#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <tuple>

#include <nierika_dsp/nierika_dsp.h>

using ndsp::Oscillator;
using ndsp::SubOscillator;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double kSampleRate = 48000.0;
}

TEST_CASE("SubOscillator: defaults to one octave below the note, matching Oscillator's own octave math", "[SubOscillator]")
{
    // Oscillator is a well-tested, independently-implemented reference for the octave/transpose
    // frequency math - a genuine cross-check, not a self-consistency check against SubOscillator's
    // own formula.
    Oscillator reference;
    reference.setSampleRate(kSampleRate);
    Oscillator::Parameters referenceParams;
    referenceParams.shape = Oscillator::Shape::Sine;
    referenceParams.octave = -1;
    reference.setBlockParameters(referenceParams);
    reference.noteOn(60);

    SubOscillator sub;
    sub.setSampleRate(kSampleRate);
    SubOscillator::Parameters subParams; // defaults: enabled=false, octave=-1, transpose=0, tone=0
    subParams.enabled = true;
    sub.setBlockParameters(subParams);
    sub.noteOn(60);

    for (int i = 0; i < 20; ++i)
        CHECK_THAT(sub.getNextSample(), WithinAbs(reference.getNextSample().left, 1.0e-4f));
}

TEST_CASE("SubOscillator: transposeSemitones of +12 matches one extra octave up", "[SubOscillator]")
{
    // 2^(12/12) == 2^1 exactly - transposeSemitones is a separate multiplier term from octave (see
    // SubOscillator::recomputePhaseIncrement), so this is a genuine cross-check that both reach the
    // same frequency, not just two branches of the same code path agreeing with itself.
    SubOscillator viaOctave;
    viaOctave.setSampleRate(kSampleRate);
    SubOscillator::Parameters octaveParams;
    octaveParams.enabled = true;
    octaveParams.octave = 0; // cancel the default -1 offset for a clean comparison
    viaOctave.setBlockParameters(octaveParams);
    viaOctave.noteOn(60);

    SubOscillator viaTranspose;
    viaTranspose.setSampleRate(kSampleRate);
    auto transposeParams = octaveParams;
    transposeParams.octave = -1;
    transposeParams.transposeSemitones = 12;
    viaTranspose.setBlockParameters(transposeParams);
    viaTranspose.noteOn(60);

    for (int i = 0; i < 20; ++i)
        CHECK_THAT(viaTranspose.getNextSample(), WithinAbs(viaOctave.getNextSample(), 1.0e-4f));
}

TEST_CASE("SubOscillator: doubling tuningReferenceHz doubles frequency, matching one octave up", "[SubOscillator]")
{
    SubOscillator viaOctave;
    viaOctave.setSampleRate(kSampleRate);
    SubOscillator::Parameters octaveParams;
    octaveParams.enabled = true;
    octaveParams.octave = 0; // cancel the default -1 offset for a clean comparison
    viaOctave.setBlockParameters(octaveParams);
    viaOctave.noteOn(60);

    SubOscillator viaTuning;
    viaTuning.setSampleRate(kSampleRate);
    auto tuningParams = octaveParams;
    tuningParams.octave = -1;
    tuningParams.tuningReferenceHz = 880.0f;
    viaTuning.setBlockParameters(tuningParams);
    viaTuning.noteOn(60);

    for (int i = 0; i < 20; ++i)
        CHECK_THAT(viaTuning.getNextSample(), WithinAbs(viaOctave.getNextSample(), 1.0e-4f));
}

TEST_CASE("SubOscillator: tonePercent=100 reaches the square wave's +1 plateau away from its edges", "[SubOscillator]")
{
    SubOscillator sub;
    sub.setSampleRate(kSampleRate);
    SubOscillator::Parameters params;
    params.enabled = true;
    params.octave = 0;
    params.tonePercent = 100.0f;
    sub.setBlockParameters(params);
    sub.noteOn(60); // ~261.63Hz -> dt ~ 0.00545

    std::ignore = sub.getNextSample(); // phase 0 - exactly at the discontinuity, skip
    CHECK_THAT(sub.getNextSample(), WithinAbs(1.0f, 1.0e-2f)); // phase == dt, just clear of the correction zone
}

TEST_CASE("SubOscillator: tonePercent blends linearly between sine and square", "[SubOscillator]")
{
    SubOscillator sineSub;
    SubOscillator squareSub;
    SubOscillator blendSub;
    for (auto* sub : { &sineSub, &squareSub, &blendSub })
        sub->setSampleRate(kSampleRate);

    SubOscillator::Parameters params;
    params.enabled = true;
    params.octave = 0;

    auto sineParams = params;
    sineParams.tonePercent = 0.0f;
    auto squareParams = params;
    squareParams.tonePercent = 100.0f;
    auto blendParams = params;
    blendParams.tonePercent = 30.0f;

    sineSub.setBlockParameters(sineParams);
    sineSub.noteOn(60);
    squareSub.setBlockParameters(squareParams);
    squareSub.noteOn(60);
    blendSub.setBlockParameters(blendParams);
    blendSub.noteOn(60);

    for (int i = 0; i < 10; ++i)
    {
        const auto sineValue = sineSub.getNextSample();
        const auto squareValue = squareSub.getNextSample();
        const auto expected = sineValue * 0.7f + squareValue * 0.3f;
        CHECK_THAT(blendSub.getNextSample(), WithinAbs(expected, 1.0e-4f));
    }
}

TEST_CASE("SubOscillator: enabled=false silences output but keeps advancing phase, so re-enabling mid-note is click-free", "[SubOscillator]")
{
    SubOscillator reference;
    reference.setSampleRate(kSampleRate);
    SubOscillator::Parameters referenceParams;
    referenceParams.enabled = true;
    reference.setBlockParameters(referenceParams);
    reference.noteOn(60);

    SubOscillator gated;
    gated.setSampleRate(kSampleRate);
    auto gatedParams = referenceParams;
    gatedParams.enabled = false;
    gated.setBlockParameters(gatedParams);
    gated.noteOn(60);

    // While disabled, output is exactly silent - proving this is a real gate, not a coincidental zero.
    for (int i = 0; i < 10; ++i)
    {
        std::ignore = reference.getNextSample();
        CHECK_THAT(gated.getNextSample(), WithinAbs(0.0f, 1.0e-6f));
    }

    // Re-enabling mid-note (without a fresh noteOn) should immediately match the reference, proving
    // the phase kept advancing underneath the whole time rather than freezing while gated.
    gatedParams.enabled = true;
    gated.setBlockParameters(gatedParams);

    for (int i = 0; i < 10; ++i)
        CHECK_THAT(gated.getNextSample(), WithinAbs(reference.getNextSample(), 1.0e-3f));
}
