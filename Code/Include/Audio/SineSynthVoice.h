#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace audio
{

// A single sine-wave voice with a short attack/release envelope (no decay/sustain shaping needed
// for a click-to-audition preview sound). Audio-thread only: startNote/stopNote/renderNextBlock
// never allocate.
class SineSynthVoice : public juce::SynthesiserVoice
{
public:
    bool canPlaySound(juce::SynthesiserSound* sound) override;

    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound* sound, int currentPitchWheelPosition) override;
    void stopNote(float velocity, bool allowTailOff) override;

    void pitchWheelMoved(int newPitchWheelValue) override;
    void controllerMoved(int controllerNumber, int newControllerValue) override;

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;

    // ADSR::setSampleRate() alone does not recompute attack/decay/release rates - only
    // setParameters() does that (see ADSR::recalculateRates()) - so this override must call both,
    // or the envelope silently uses stale rates from whatever sample rate was in effect at the
    // ADSR's construction (44.1kHz) at any other playback rate.
    void setCurrentPlaybackSampleRate(double newRate) override;

private:
    double _phase = 0.0;
    double _phaseIncrement = 0.0;
    juce::ADSR _adsr;
};

}
