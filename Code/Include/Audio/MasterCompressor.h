#pragma once

#include <juce_dsp/juce_dsp.h>

namespace audio
{

// A one-knob "bus glue" compressor on the master output. A single Amount (0-100%) drives
// Threshold (0dB down to -24dB) and auto-makeup gain together, with everything else fixed at
// well-established bus-compression defaults: 4:1 ratio, 6dB soft knee, 10ms attack, 250ms release.
// Detection is stereo-linked (a single envelope follower fed by max(|left|,|right|)), and the
// identical resulting gain is applied to both channels, preserving the stereo image. Operates on
// the whole already-summed stereo mix, once per block - not per-voice, unlike VoiceFilter.
// Program-dependent auto-release is a deliberately out-of-scope refinement for this pass, same
// "documented limit, not an oversight" spirit as Oscillator's Warp/Fold oversampling note.
class MasterCompressor
{
public:
    struct Parameters
    {
        float amountPercent = 0.0f; // 0 = bypass
    };

    void prepare(double sampleRate, int samplesPerBlock, int numChannels);
    void reset();

    // Snapshot call - once per renderNextBlock, like every other *BlockParameters call in this codebase.
    void setBlockParameters(const Parameters& parameters) noexcept;

    // In-place, whole-block processing - the envelope follower still needs per-sample accuracy
    // (loops internally), but there's no per-voice reason to expose a per-sample API here.
    void process(juce::AudioBuffer<float>& buffer) noexcept;

private:
    juce::dsp::BallisticsFilter<float> _envelopeFollower;
    Parameters _parameters;
};

}
