#include "Audio/SineSynthVoice.h"

#include <cmath>

#include "Audio/SineSynthSound.h"

namespace audio
{

namespace
{
    constexpr float kAttackSeconds = 0.005f;
    constexpr float kDecaySeconds = 0.0f;
    constexpr float kSustainLevel = 1.0f;
    constexpr float kReleaseSeconds = 0.2f;
    constexpr float kVoiceGain = 0.2f; // headroom for a full chord (several voices summed at once)

    juce::ADSR::Parameters getAdsrParameters()
    {
        return { kAttackSeconds, kDecaySeconds, kSustainLevel, kReleaseSeconds };
    }
}

bool SineSynthVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<SineSynthSound*>(sound) != nullptr;
}

void SineSynthVoice::startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound* sound, int currentPitchWheelPosition)
{
    juce::ignoreUnused(velocity, sound, currentPitchWheelPosition);

    _phase = 0.0;

    const auto frequencyHz = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    _phaseIncrement = juce::MathConstants<double>::twoPi * frequencyHz / getSampleRate();

    _adsr.reset();
    _adsr.setParameters(getAdsrParameters());
    _adsr.noteOn();
}

void SineSynthVoice::stopNote(float velocity, bool allowTailOff)
{
    juce::ignoreUnused(velocity);

    if (allowTailOff)
    {
        _adsr.noteOff();
    }
    else
    {
        _adsr.reset();
        clearCurrentNote();
    }
}

void SineSynthVoice::pitchWheelMoved(int /*newPitchWheelValue*/) {}
void SineSynthVoice::controllerMoved(int /*controllerNumber*/, int /*newControllerValue*/) {}

void SineSynthVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (!_adsr.isActive())
        return;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto envelope = _adsr.getNextSample();
        const auto value = static_cast<float>(std::sin(_phase)) * envelope * kVoiceGain;

        for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
            outputBuffer.addSample(channel, startSample + sample, value);

        _phase += _phaseIncrement;
        if (_phase >= juce::MathConstants<double>::twoPi)
            _phase -= juce::MathConstants<double>::twoPi;

        if (!_adsr.isActive())
        {
            clearCurrentNote();
            break;
        }
    }
}

void SineSynthVoice::setCurrentPlaybackSampleRate(double newRate)
{
    SynthesiserVoice::setCurrentPlaybackSampleRate(newRate);

    if (newRate > 0.0)
    {
        _adsr.setSampleRate(newRate);
        _adsr.setParameters(getAdsrParameters());
    }
}

}
