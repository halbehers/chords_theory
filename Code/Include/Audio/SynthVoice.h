#pragma once

#include <array>
#include <memory>

#include <juce_audio_basics/juce_audio_basics.h>
#include <nierika_dsp/nierika_dsp.h>

#include "Audio/DahdsrEnvelope.h"
#include "Audio/Lfo.h"
#include "Audio/VoiceFilter.h"
#include "Audio/VoiceSharedState.h"

namespace audio
{

// A single voice, shaped by a 6-stage DahdsrEnvelope, a per-voice VoiceFilter, and an Lfo
// modulating the filter's cutoff. Its tone source is two independent ndsp::Oscillator instances,
// combined via a pluggable ndsp::SynthAlgorithm (Add/FM/Ring Mod/AM/Serial Fold - see
// VoiceSharedState::MixerState), plus one ndsp::SubOscillator layer added after that combine step.
// All five algorithms are constructed once, in this class's own constructor, and selected by index
// at block rate - no audio-thread allocation. Audio-thread only: startNote/stopNote/
// renderNextBlock never allocate.
class SynthVoice : public juce::SynthesiserVoice
{
public:
    explicit SynthVoice(VoiceSharedState& sharedState);

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
    [[nodiscard]] ndsp::Oscillator::Parameters readOscillatorParameters(const OscillatorState& state) const;
    [[nodiscard]] ndsp::SubOscillator::Parameters readSubOscillatorParameters() const;
    [[nodiscard]] DahdsrEnvelope::Parameters readEnvelopeParameters() const;
    [[nodiscard]] VoiceFilter::BlockParameters readFilterParameters() const;
    [[nodiscard]] Lfo::Parameters readLfoParameters() const;

    // Reads the current algorithm index + that algorithm's own amount parameter from
    // _sharedState.mixer and forwards the amount to the now-active algorithm. The other four
    // algorithms' setBlockParameters() simply isn't called this block - their stale internal
    // amount is unused until reselected, which is harmless (they're stateless combiners).
    void updateMixerBlockParameters();

    VoiceSharedState& _sharedState;
    ndsp::Oscillator _oscillator1;
    ndsp::Oscillator _oscillator2;
    ndsp::SubOscillator _subOscillator;
    std::array<std::unique_ptr<ndsp::SynthAlgorithm>, 5> _algorithms; // index matches mixer-algorithm's AudioParameterChoice order
    int _currentAlgorithmIndex = 0;
    DahdsrEnvelope _envelope;
    VoiceFilter _filter;
    Lfo _lfo;
};

}
