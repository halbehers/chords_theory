#pragma once

#include <array>

#include <juce_audio_basics/juce_audio_basics.h>

namespace audio
{

// A single, reusable per-voice oscillator - SynthVoice owns two independent instances, summed by
// simple addition (see SynthVoice::renderNextBlock). Per-sample signal path, per unison voice:
// base waveform (PolyBLEP-corrected Sine/Triangle/Saw/Square) -> Warp (phase squeeze + zero-fill)
// -> Shape noise (linear crossfade toward white noise) -> Fold (wavefolding), then panned and
// summed across up to kMaxUnisonVoices detuned voices, plus an independent sub-oscillator layer
// (plain centered sine, one or two octaves down, not shaped/warped/folded/noised). Audio-thread
// only: nothing here allocates outside construction.
//
// PolyBLEP band-limits the base waveform's own Saw/Square discontinuities. Warp and Fold are
// nonlinear stages that generate their own extra high-frequency content on top of that - fully
// band-limiting those would need oversampling, which is out of scope for now (see the Synth
// Oscillator plan's "Suggested follow-ups"); this is a deliberate, documented limit, not an
// oversight.
class Oscillator
{
public:
    enum class Shape
    {
        Sine,
        Triangle,
        Saw,
        Square
    };

    static constexpr int kMaxUnisonVoices = 16;

    struct Parameters
    {
        Shape shape = Shape::Sine;
        float shapeNoisePercent = 0.0f;    // 0..100
        int   octave = 0;                  // -3..+3
        int   transposeSemitones = 0;      // -12..+12, added on top of octave
        float detuneCents = 0.0f;          // -50..+50
        float warpPercent = 0.0f;          // -100..+100
        float foldPercent = 0.0f;          // 0..100
        float outputDb = 0.0f;             // -24..+6
        int   unisonVoices = 1;            // 1..kMaxUnisonVoices - 1 = today's exact single-voice behavior
        float unisonDetuneCents = 0.0f;    // 0..50, symmetric spread across voices
        float unisonStereoPercent = 0.0f;  // 0..100
        float subLevelPercent = 0.0f;      // 0..100
        bool  subOctaveDown2 = false;      // false = -1 oct, true = -2 oct
        float phasePercent = 0.0f;         // 0..100 -> 0..2pi start phase
        bool  phaseRandomizeEnabled = false;
    };

    struct StereoSample
    {
        float left = 0.0f;
        float right = 0.0f;
    };

    void setSampleRate(double sampleRate) noexcept;

    // Block-rate snapshot, like VoiceFilter/Lfo - re-read once per renderNextBlock, not per sample,
    // so every live knob turn is audible within about one host block on an already-held note.
    void setBlockParameters(const Parameters& parameters) noexcept;

    // Sets the base playing frequency from a MIDI note and resets (or, if phaseRandomizeEnabled,
    // randomizes) every unison voice's and the sub-oscillator's starting phase.
    void noteOn(int midiNoteNumber) noexcept;

    [[nodiscard]] StereoSample getNextSample() noexcept;

private:
    struct UnisonVoiceState
    {
        double phase01 = 0.0;
        double phaseIncrement = 0.0;
        juce::Random random;
    };

    [[nodiscard]] float computeShapeValue(double phase01, double phaseIncrement) const noexcept;
    [[nodiscard]] float computeWarpedShapeValue(double outerPhase01, double outerPhaseIncrement) const noexcept;
    [[nodiscard]] static float foldSample(float x) noexcept;

    // Derives every unison/sub phase increment from the current _baseFrequencyHz/_parameters/
    // _sampleRate - called from both setBlockParameters() (parameters just changed) and noteOn()
    // (base frequency just changed), so either call order is safe and never leaves increments
    // computed from a stale frequency or a stale parameter snapshot.
    void recomputePhaseIncrements() noexcept;

    double _sampleRate = 44100.0;
    double _baseFrequencyHz = 440.0;
    Parameters _parameters;

    std::array<UnisonVoiceState, kMaxUnisonVoices> _unisonVoices;

    double _subPhase01 = 0.0;
    double _subPhaseIncrement = 0.0;
    juce::Random _subRandom;
};

}
