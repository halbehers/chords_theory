#pragma once

#include <array>

#include <juce_dsp/juce_dsp.h>

namespace audio
{

// A per-voice, per-channel-independent multi-mode filter: drive (tanh waveshaping) -> N cascaded
// juce::dsp::StateVariableTPTFilter stages (N=1 is 12dB/octave, N=2 cascaded same-type stages is
// 24dB/octave) -> dry/wet mix. Per-voice (not one shared filter on the mixed bus) because each
// note needs its own key-tracked cutoff. Left and right channels run through two entirely
// independent filter-stage chains (StateVariableTPTFilter can't run two different cutoffs on one
// instance), fed the same dry oscillator sample but each modulated by its own LFO channel value -
// this is what lets an LFO's "Stereo" width parameter manifest as L/R cutoff divergence. Audio
// -thread only: prepare()/reset() aside, nothing here allocates.
class VoiceFilter
{
public:
    struct BlockParameters
    {
        juce::dsp::StateVariableTPTFilterType type = juce::dsp::StateVariableTPTFilterType::lowpass;
        int numStages = 1; // 1 = 12dB/octave, 2 = 24dB/octave (cascaded, same type)
        float baseCutoffHz = 20000.0f;
        float resonance = 0.70710678f; // 1/sqrt(2) - "flat" 12dB/octave response
        float driveDb = 0.0f;
        float mixPercent = 100.0f;
        float lfoDepthOctaves = 0.0f; // signed max cutoff swing at full LFO deflection
        float keyTrackAmountPercent = 0.0f;
    };

    struct StereoSample
    {
        float left = 0.0f;
        float right = 0.0f;
    };

    // numChannels: 1 or 2, from the negotiated bus layout - with 1, the right chain is skipped
    // and StereoSample::right mirrors left.
    void prepare(double sampleRate, int numChannels);
    void reset();

    // Called once at startNote() - key-tracking is a per-note constant by definition.
    void setKeyTrackedNote(int midiNoteNumber, int referenceNote) noexcept;

    // Snapshot call - once per renderNextBlock, not per sample (see VoiceSharedState's doc
    // comment for why cutoff/resonance/drive/mix/type/slope are re-read at block rate).
    void setBlockParameters(const BlockParameters& parameters) noexcept;

    // dryLeft/dryRight: the oscillator's own stereo dry sample (unison stereo width can make these
    // genuinely differ, not just mirror each other). lfoLeft/lfoRight: this sample's bipolar
    // (-1..1) LFO value per channel (pass 0.0f for both if no LFO is modulating the filter yet).
    // Cutoff is recomputed and clamped to [20Hz, sampleRate*0.49] every sample on every stage -
    // required, not optional: juce::dsp::StateVariableTPTFilter::setCutoffFrequency asserts the
    // frequency stays below Nyquist.
    [[nodiscard]] StereoSample processSample(float dryLeft, float dryRight, float lfoLeft, float lfoRight) noexcept;

private:
    [[nodiscard]] float computeChannelCutoffHz(float lfoValue) const noexcept;
    [[nodiscard]] float driveSample(float input) const noexcept;

    double _sampleRate = 44100.0;
    int _numChannels = 2;
    BlockParameters _parameters;
    int _playedNote = 60;
    int _referenceNote = 60;

    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> _leftStages;
    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> _rightStages;
};

}
