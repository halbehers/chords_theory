#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <vector>

#include "Audio/MasterCompressor.h"

using audio::MasterCompressor;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 512;

    // Fills a stereo buffer with a full-scale sine at the given frequency, one block per call -
    // callers loop this to build up enough sustained signal for the compressor's envelope follower
    // (10ms attack) to settle well past its initial ramp-up.
    void fillSineBlock(juce::AudioBuffer<float>& buffer, double frequencyHz, double& phase01, float amplitude = 1.0f)
    {
        const auto phaseIncrement = frequencyHz / kSampleRate;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto value = amplitude * static_cast<float>(std::sin(juce::MathConstants<double>::twoPi * phase01));
            buffer.setSample(0, i, value);
            buffer.setSample(1, i, value);
            phase01 += phaseIncrement;
            if (phase01 >= 1.0)
                phase01 -= std::floor(phase01);
        }
    }
}

TEST_CASE("MasterCompressor: amount 0% is an exact bypass", "[MasterCompressor]")
{
    MasterCompressor compressor;
    compressor.prepare(kSampleRate, kBlockSize, 2);
    compressor.setBlockParameters({ .amountPercent = 0.0f });

    juce::AudioBuffer<float> buffer(2, kBlockSize);
    double phase = 0.0;
    fillSineBlock(buffer, 440.0, phase);

    juce::AudioBuffer<float> reference;
    reference.makeCopyOf(buffer);

    compressor.process(buffer);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        CHECK_THAT(buffer.getSample(0, i), WithinAbs(reference.getSample(0, i), 1.0e-9f));
        CHECK_THAT(buffer.getSample(1, i), WithinAbs(reference.getSample(1, i), 1.0e-9f));
    }
}

TEST_CASE("MasterCompressor: amount 100% measurably reduces a loud sustained signal's level", "[MasterCompressor]")
{
    MasterCompressor compressor;
    compressor.prepare(kSampleRate, kBlockSize, 2);
    compressor.setBlockParameters({ .amountPercent = 100.0f });

    juce::AudioBuffer<float> buffer(2, kBlockSize);
    double phase = 0.0;

    // Run several blocks so the envelope follower's 10ms attack has long settled by the last one -
    // only that last block is used for the actual measurement.
    for (int block = 0; block < 10; ++block)
    {
        fillSineBlock(buffer, 440.0, phase, 1.0f); // full-scale (0dBFS) sine
        compressor.process(buffer);
    }

    auto peak = 0.0f;
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        peak = std::max(peak, std::abs(buffer.getSample(0, i)));

    // Even with auto-makeup gain, a 0dBFS input (well above the -24dB threshold at 100% amount)
    // must end up measurably quieter than the dry 1.0f peak - proving real gain reduction is
    // happening, not just a makeup-gain volume boost.
    CHECK(peak < 0.9f);
    CHECK(peak > 0.05f); // sanity: not silenced either
}

TEST_CASE("MasterCompressor: stereo-linked detection applies the same gain reduction to both channels", "[MasterCompressor]")
{
    MasterCompressor compressor;
    compressor.prepare(kSampleRate, kBlockSize, 2);
    compressor.setBlockParameters({ .amountPercent = 100.0f });

    // Loud left, quiet right, held constant across every block (including the settling blocks) -
    // detection is driven by the louder channel (left) throughout, so by the final block the
    // envelope follower is already settled and the same resulting gain must apply identically to
    // both channels, i.e. right's own gain-reduction ratio (relative to its dry level) matches
    // left's, even though right alone would never have crossed the threshold on its own.
    const auto phaseIncrement = 440.0 / kSampleRate;
    double phase = 0.0;
    juce::AudioBuffer<float> buffer(2, kBlockSize);

    auto fillAsymmetricBlock = [&]()
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto raw = static_cast<float>(std::sin(juce::MathConstants<double>::twoPi * phase));
            buffer.setSample(0, i, raw * 1.0f);   // left: full-scale
            buffer.setSample(1, i, raw * 0.1f);   // right: quiet
            phase += phaseIncrement;
            if (phase >= 1.0)
                phase -= std::floor(phase);
        }
    };

    for (int block = 0; block < 9; ++block)
    {
        fillAsymmetricBlock();
        compressor.process(buffer);
    }

    fillAsymmetricBlock();
    juce::AudioBuffer<float> dry;
    dry.makeCopyOf(buffer);

    compressor.process(buffer);

    // Compare the actual per-sample gain factor applied to each channel - they must match, since
    // one linked gain value is applied to both.
    for (int i = 0; i < buffer.getNumSamples(); i += 32) // sparse sampling is enough to prove the point
    {
        if (std::abs(dry.getSample(0, i)) < 1.0e-3f || std::abs(dry.getSample(1, i)) < 1.0e-3f)
            continue; // skip near-zero-crossing samples, where the gain ratio is numerically unstable

        const auto leftGain = buffer.getSample(0, i) / dry.getSample(0, i);
        const auto rightGain = buffer.getSample(1, i) / dry.getSample(1, i);
        CHECK_THAT(leftGain, WithinAbs(rightGain, 1.0e-3f));
    }
}
