#pragma once

#include <nierika_dsp/nierika_dsp.h>

namespace audio
{

// A per-voice LFO (needed regardless of mode - Delay-since-note-on is inherently per-note state).
// In Trigger mode, its own private phase resets on noteOn() and steps sample-by-sample from 0.
// In Free mode, its private phase is instead re-seeded from the engine's own block-start running
// phase at the top of every block (see setParameters's freePhaseSeed01) and then steps
// sample-by-sample from there, identically to Trigger mode - every voice uses the same rate, so
// all voices advance in lockstep within a block and agree with the engine's own tracked value by
// the block's end, giving smooth per-sample modulation that's still a single, globally-shared
// LFO conceptually (not tied to any one note), continuing to run in real time even through
// silence between notes. Audio-thread only: nothing here allocates.
class Lfo
{
public:
    enum class Shape
    {
        Sine,
        Triangle,
        Saw,
        Square
    };

    enum class Mode
    {
        Trigger,
        Free
    };

    struct Parameters
    {
        Shape shape = Shape::Sine;
        double freeRateHz = 2.0; // used when syncEnabled == false
        bool syncEnabled = false;
        ndsp::Timing::NoteTiming syncDivision = ndsp::Timing::NOTE_4;
        Mode mode = Mode::Trigger;
        float smooth = 0.0f;       // 0..1 - blends every shape toward a sine as it increases
        float delayMs = 0.0f;      // silence-then-fade-in window after note-on
        float stereoPercent = 0.0f; // 0..100 -> 0..pi radians of left/right phase offset
    };

    struct StereoSample
    {
        float left = 0.0f;
        float right = 0.0f;
    };

    void setSampleRate(double sampleRate) noexcept;

    // hostBpm is only consulted when parameters.syncEnabled is true. freePhaseSeed01 is only
    // consulted when parameters.mode == Mode::Free - see class doc comment.
    void setParameters(const Parameters& parameters, double hostBpm, double freePhaseSeed01) noexcept;

    // Resets the private (Trigger-mode) phase to 0 and restarts the delay-since-note-on counter -
    // happens regardless of mode, since Delay is always per-note.
    void noteOn() noexcept;

    // No-op: the LFO is not envelope-gated and keeps running through the Release stage.
    void noteOff() noexcept;

    [[nodiscard]] StereoSample getNextSample() noexcept;

private:
    [[nodiscard]] float computeShapeValue(double phase01) const noexcept;

    double _sampleRate = 44100.0;
    Parameters _parameters;
    double _phaseIncrementPerSample = 0.0;
    double _triggerPhase01 = 0.0;
    double _freePhase01 = 0.0;
    int _delaySamples = 0;
    int _samplesSinceNoteOn = 0;
};

}
