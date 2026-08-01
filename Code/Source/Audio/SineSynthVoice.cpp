#include "Audio/SineSynthVoice.h"

#include <cmath>

#include "Audio/SineSynthSound.h"

namespace audio
{

namespace
{
    constexpr float kVoiceGain = 0.2f; // headroom for a full chord (several voices summed at once)
    constexpr int kKeyTrackReferenceNote = 60; // middle C - filter-key-track-percent is relative to this
    constexpr float kMaxLfoDepthOctaves = 4.0f; // cutoff swing at lfo-depth-percent == 100
}

SineSynthVoice::SineSynthVoice(VoiceSharedState& sharedState):
    _sharedState(sharedState)
{
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

    _envelope.reset();
    _envelope.setParameters(readEnvelopeParameters());
    _envelope.noteOn();

    _filter.reset();
    _filter.setKeyTrackedNote(midiNoteNumber, kKeyTrackReferenceNote);
    _filter.setBlockParameters(readFilterParameters());

    _lfo.noteOn();
}

void SineSynthVoice::stopNote(float velocity, bool allowTailOff)
{
    juce::ignoreUnused(velocity);

    _lfo.noteOff();

    if (allowTailOff)
    {
        _envelope.noteOff();
    }
    else
    {
        _envelope.reset();
        clearCurrentNote();
    }
}

void SineSynthVoice::pitchWheelMoved(int /*newPitchWheelValue*/) {}
void SineSynthVoice::controllerMoved(int /*controllerNumber*/, int /*newControllerValue*/) {}

void SineSynthVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (!_envelope.isActive())
        return;

    // Block-rate re-snapshot, separate from the once-at-startNote envelope/key-track snapshot -
    // this is what makes turning the cutoff/resonance/drive/mix/type/slope/LFO knobs audible on
    // an already-sounding, held note (see VoiceSharedState's doc comment for the cadence rationale).
    _filter.setBlockParameters(readFilterParameters());
    _lfo.setParameters(readLfoParameters(), _sharedState.hostBpm, _sharedState.freeLfoPhase01);

    const auto numChannels = outputBuffer.getNumChannels();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto envelopeValue = _envelope.getNextSample();
        const auto dryValue = static_cast<float>(std::sin(_phase)) * envelopeValue * kVoiceGain;

        const auto lfoValue = _lfo.getNextSample();
        const auto filtered = _filter.processSample(dryValue, lfoValue.left, lfoValue.right);

        if (numChannels > 0)
            outputBuffer.addSample(0, startSample + sample, filtered.left);
        if (numChannels > 1)
            outputBuffer.addSample(1, startSample + sample, filtered.right);
        for (int channel = 2; channel < numChannels; ++channel)
            outputBuffer.addSample(channel, startSample + sample, filtered.left);

        _phase += _phaseIncrement;
        if (_phase >= juce::MathConstants<double>::twoPi)
            _phase -= juce::MathConstants<double>::twoPi;

        if (!_envelope.isActive())
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
        _envelope.setSampleRate(newRate);
        _envelope.setParameters(readEnvelopeParameters());
        _lfo.setSampleRate(newRate);
        _lfo.setParameters(readLfoParameters(), _sharedState.hostBpm, _sharedState.freeLfoPhase01);
    }
}

void SineSynthVoice::prepareFilter(double sampleRate, int numChannels)
{
    _filter.prepare(sampleRate, numChannels);
}

DahdsrEnvelope::Parameters SineSynthVoice::readEnvelopeParameters() const
{
    return {
        .delayMs = _sharedState.envelopeDelayMs.load(std::memory_order_relaxed),
        .attackMs = _sharedState.envelopeAttackMs.load(std::memory_order_relaxed),
        .holdMs = _sharedState.envelopeHoldMs.load(std::memory_order_relaxed),
        .decayMs = _sharedState.envelopeDecayMs.load(std::memory_order_relaxed),
        .sustainLevel = _sharedState.envelopeSustainPercent.load(std::memory_order_relaxed) / 100.0f,
        .releaseMs = _sharedState.envelopeReleaseMs.load(std::memory_order_relaxed),
    };
}

VoiceFilter::BlockParameters SineSynthVoice::readFilterParameters() const
{
    const auto typeIndex = _sharedState.filterType.load(std::memory_order_relaxed);

    VoiceFilter::BlockParameters parameters;
    parameters.type = typeIndex == 1 ? juce::dsp::StateVariableTPTFilterType::highpass
                     : typeIndex == 2 ? juce::dsp::StateVariableTPTFilterType::bandpass
                     : juce::dsp::StateVariableTPTFilterType::lowpass;
    parameters.numStages = _sharedState.filterSlope.load(std::memory_order_relaxed) == 1 ? 2 : 1;
    parameters.baseCutoffHz = _sharedState.filterCutoffHz.load(std::memory_order_relaxed);
    parameters.resonance = _sharedState.filterResonance.load(std::memory_order_relaxed);
    parameters.driveDb = _sharedState.filterDriveDb.load(std::memory_order_relaxed);
    parameters.mixPercent = _sharedState.filterMixPercent.load(std::memory_order_relaxed);
    parameters.lfoDepthOctaves = (_sharedState.lfoDepthPercent.load(std::memory_order_relaxed) / 100.0f) * kMaxLfoDepthOctaves;
    parameters.keyTrackAmountPercent = _sharedState.filterKeyTrackPercent.load(std::memory_order_relaxed);

    return parameters;
}

Lfo::Parameters SineSynthVoice::readLfoParameters() const
{
    const auto shapeIndex = _sharedState.lfoShape.load(std::memory_order_relaxed);

    Lfo::Parameters parameters;
    parameters.shape = shapeIndex == 1 ? Lfo::Shape::Triangle
                      : shapeIndex == 2 ? Lfo::Shape::Saw
                      : shapeIndex == 3 ? Lfo::Shape::Square
                      : Lfo::Shape::Sine;
    parameters.freeRateHz = static_cast<double>(_sharedState.lfoRateHz.load(std::memory_order_relaxed));
    parameters.syncEnabled = _sharedState.lfoSyncEnabled.load(std::memory_order_relaxed);
    parameters.syncDivision = static_cast<ndsp::Timing::NoteTiming>(_sharedState.lfoSyncDivision.load(std::memory_order_relaxed));
    parameters.mode = _sharedState.lfoMode.load(std::memory_order_relaxed) == 1 ? Lfo::Mode::Free : Lfo::Mode::Trigger;
    parameters.smooth = _sharedState.lfoSmoothPercent.load(std::memory_order_relaxed) / 100.0f;
    parameters.delayMs = _sharedState.lfoDelayMs.load(std::memory_order_relaxed);
    parameters.stereoPercent = _sharedState.lfoStereoPercent.load(std::memory_order_relaxed);

    return parameters;
}

}
