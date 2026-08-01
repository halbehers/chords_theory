#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "Audio/DahdsrEnvelope.h"
#include "Audio/Lfo.h"
#include "Audio/VoiceFilter.h"
#include "Audio/VoiceSharedState.h"

namespace audio
{

// A single sine-wave voice shaped by a 6-stage DahdsrEnvelope, a per-voice VoiceFilter, and an
// Lfo modulating the filter's cutoff. Audio-thread only: startNote/stopNote/renderNextBlock never
// allocate.
class SineSynthVoice : public juce::SynthesiserVoice
{
public:
    explicit SineSynthVoice(VoiceSharedState& sharedState);

    bool canPlaySound(juce::SynthesiserSound* sound) override;

    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound* sound, int currentPitchWheelPosition) override;
    void stopNote(float velocity, bool allowTailOff) override;

    void pitchWheelMoved(int newPitchWheelValue) override;
    void controllerMoved(int controllerNumber, int newControllerValue) override;

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;

    // DahdsrEnvelope::setSampleRate() alone does not recompute stage sample-lengths - only
    // setParameters() does that - so this override must call both, or the envelope silently uses
    // stale stage lengths from whatever sample rate was in effect at construction (44.1kHz) at
    // any other playback rate. Same gotcha juce::ADSR has, which this class replaced.
    void setCurrentPlaybackSampleRate(double newRate) override;

    // juce::SynthesiserVoice has no channel-count-aware prepare hook of its own - called
    // explicitly by ChordSynthEngine::prepare() (which does know the negotiated bus channel
    // count) after Synthesiser::setCurrentPlaybackSampleRate() has already run.
    void prepareFilter(double sampleRate, int numChannels);

private:
    [[nodiscard]] DahdsrEnvelope::Parameters readEnvelopeParameters() const;
    [[nodiscard]] VoiceFilter::BlockParameters readFilterParameters() const;
    [[nodiscard]] Lfo::Parameters readLfoParameters() const;

    VoiceSharedState& _sharedState;
    double _phase = 0.0;
    double _phaseIncrement = 0.0;
    DahdsrEnvelope _envelope;
    VoiceFilter _filter;
    Lfo _lfo;
};

}
