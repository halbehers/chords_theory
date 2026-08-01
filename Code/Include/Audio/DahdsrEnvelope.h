#pragma once

namespace audio
{

// A 6-stage Delay-Attack-Hold-Decay-Sustain-Release envelope generator. Every timed stage
// (Delay/Attack/Hold/Decay/Release) ramps linearly over its own fixed sample count; Sustain holds
// its level indefinitely until an explicit noteOff() (correct for a real, arbitrarily long held
// MIDI note, not just a fixed-duration preview). noteOff() always releases from whatever the
// current value actually is (not always from the sustain level), so a release triggered mid
// Attack/Hold/Decay is click-free. Audio-thread only: setSampleRate/setParameters/reset/noteOn/
// noteOff/getNextSample never allocate.
class DahdsrEnvelope
{
public:
    enum class Stage
    {
        Idle,
        Delay,
        Attack,
        Hold,
        Decay,
        Sustain,
        Release
    };

    struct Parameters
    {
        float delayMs = 0.0f;
        float attackMs = 5.0f;
        float holdMs = 0.0f;
        float decayMs = 0.0f;
        float sustainLevel = 1.0f; // 0..1
        float releaseMs = 200.0f;
    };

    // Must be called before setParameters() - stage lengths are derived from sampleRate.
    void setSampleRate(double sampleRate) noexcept;

    // Recomputes every stage's sample length from the given (millisecond-based) parameters. Not
    // read continuously during Sustain or otherwise re-applied mid-note - call again before the
    // next noteOn() to pick up new values, matching juce::ADSR's own contract.
    void setParameters(const Parameters& parameters) noexcept;

    // Hard reset to Idle at value 0, no ramp - matches juce::ADSR::reset()'s semantics.
    void reset() noexcept;

    // Always restarts from value 0 at the Delay stage, even if already active (voice retrigger) -
    // matches juce::ADSR::noteOn()'s own unconditional-reset behavior.
    void noteOn() noexcept;

    // A no-op if already Idle or already releasing. Otherwise jumps to Release, ramping from
    // whatever the envelope's current value is (not from the sustain level) down to 0 over
    // exactly releaseMs, click-free regardless of which stage noteOff() interrupted.
    void noteOff() noexcept;

    // Steps the state machine by one sample and returns the new value (0..1).
    float getNextSample() noexcept;

    [[nodiscard]] bool isActive() const noexcept { return _stage != Stage::Idle; }
    [[nodiscard]] Stage getStage() const noexcept { return _stage; }

private:
    void enterStage(Stage stage) noexcept;
    [[nodiscard]] int millisecondsToSamples(float ms) const noexcept;
    [[nodiscard]] static Stage nextStage(Stage stage) noexcept;

    double _sampleRate = 44100.0;
    Parameters _parameters;

    int _delaySamples = 0;
    int _attackSamples = 0;
    int _holdSamples = 0;
    int _decaySamples = 0;
    int _releaseSamples = 0;

    Stage _stage = Stage::Idle;
    float _value = 0.0f;

    int _stageSampleIndex = 0;
    int _stageLengthSamples = 0;
    float _stageStartValue = 0.0f;
    float _stageEndValue = 0.0f;
};

}
