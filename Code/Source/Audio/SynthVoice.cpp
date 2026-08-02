#include "Audio/SynthVoice.h"

#include "Audio/SynthSound.h"

namespace audio
{

namespace
{
    constexpr float kVoiceGain = 0.2f; // headroom for a full chord (several voices summed at once)
    constexpr int kKeyTrackReferenceNote = 60; // middle C - filter-key-track-percent is relative to this
    constexpr float kMaxLfoDepthOctaves = 4.0f; // cutoff swing at lfo-depth-percent == 100
}

SynthVoice::SynthVoice(VoiceSharedState& sharedState):
    _sharedState(sharedState)
{
}

bool SynthVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<SynthSound*>(sound) != nullptr;
}

void SynthVoice::startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound* sound, int currentPitchWheelPosition)
{
    juce::ignoreUnused(velocity, sound, currentPitchWheelPosition);

    _envelope.reset();
    _envelope.setParameters(readEnvelopeParameters());
    _envelope.noteOn();

    _filter.reset();
    _filter.setKeyTrackedNote(midiNoteNumber, kKeyTrackReferenceNote);
    _filter.setBlockParameters(readFilterParameters());

    _oscillator1.noteOn(midiNoteNumber);
    _oscillator1.setBlockParameters(readOscillatorParameters(_sharedState.oscillator1));
    _oscillator2.noteOn(midiNoteNumber);
    _oscillator2.setBlockParameters(readOscillatorParameters(_sharedState.oscillator2));

    _lfo.noteOn();
}

void SynthVoice::stopNote(float velocity, bool allowTailOff)
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

void SynthVoice::pitchWheelMoved(int /*newPitchWheelValue*/) {}
void SynthVoice::controllerMoved(int /*controllerNumber*/, int /*newControllerValue*/) {}

void SynthVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (!_envelope.isActive())
        return;

    // Block-rate re-snapshot, separate from the once-at-startNote envelope/key-track snapshot -
    // this is what makes turning the cutoff/resonance/drive/mix/type/slope/LFO/oscillator knobs
    // audible on an already-sounding, held note (see VoiceSharedState's doc comment for the
    // cadence rationale).
    _filter.setBlockParameters(readFilterParameters());
    _lfo.setParameters(readLfoParameters(), _sharedState.hostBpm, _sharedState.freeLfoPhase01);
    _oscillator1.setBlockParameters(readOscillatorParameters(_sharedState.oscillator1));
    _oscillator2.setBlockParameters(readOscillatorParameters(_sharedState.oscillator2));

    const auto numChannels = outputBuffer.getNumChannels();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto envelopeValue = _envelope.getNextSample();

        const auto osc1Sample = _oscillator1.getNextSample();
        const auto osc2Sample = _oscillator2.getNextSample();
        // Simple addition for now - this is the single point a future combine-mode parameter
        // (parallel/serial/FM/...) would change; deliberately not abstracted further until that
        // parameter actually exists.
        const auto dryLeft = (osc1Sample.left + osc2Sample.left) * envelopeValue * kVoiceGain;
        const auto dryRight = (osc1Sample.right + osc2Sample.right) * envelopeValue * kVoiceGain;

        const auto lfoValue = _lfo.getNextSample();
        const auto filtered = _filter.processSample(dryLeft, dryRight, lfoValue.left, lfoValue.right);

        if (numChannels > 0)
            outputBuffer.addSample(0, startSample + sample, filtered.left);
        if (numChannels > 1)
            outputBuffer.addSample(1, startSample + sample, filtered.right);
        for (int channel = 2; channel < numChannels; ++channel)
            outputBuffer.addSample(channel, startSample + sample, filtered.left);

        if (!_envelope.isActive())
        {
            clearCurrentNote();
            break;
        }
    }
}

void SynthVoice::setCurrentPlaybackSampleRate(double newRate)
{
    SynthesiserVoice::setCurrentPlaybackSampleRate(newRate);

    if (newRate > 0.0)
    {
        _envelope.setSampleRate(newRate);
        _envelope.setParameters(readEnvelopeParameters());
        _lfo.setSampleRate(newRate);
        _lfo.setParameters(readLfoParameters(), _sharedState.hostBpm, _sharedState.freeLfoPhase01);
        _oscillator1.setSampleRate(newRate);
        _oscillator1.setBlockParameters(readOscillatorParameters(_sharedState.oscillator1));
        _oscillator2.setSampleRate(newRate);
        _oscillator2.setBlockParameters(readOscillatorParameters(_sharedState.oscillator2));
    }
}

void SynthVoice::prepareFilter(double sampleRate, int numChannels)
{
    _filter.prepare(sampleRate, numChannels);
}

Oscillator::Parameters SynthVoice::readOscillatorParameters(const OscillatorState& state) const
{
    const auto shapeIndex = state.shape.load(std::memory_order_relaxed);

    Oscillator::Parameters parameters;
    parameters.enabled = state.enabled.load(std::memory_order_relaxed);
    parameters.shape = shapeIndex == 1 ? Oscillator::Shape::Triangle
                      : shapeIndex == 2 ? Oscillator::Shape::Saw
                      : shapeIndex == 3 ? Oscillator::Shape::Square
                      : Oscillator::Shape::Sine;
    parameters.shapeNoisePercent = state.shapeNoisePercent.load(std::memory_order_relaxed);
    parameters.octave = state.octave.load(std::memory_order_relaxed);
    parameters.transposeSemitones = state.transposeSemitones.load(std::memory_order_relaxed);
    parameters.detuneCents = state.detuneCents.load(std::memory_order_relaxed);
    parameters.warpPercent = state.warpPercent.load(std::memory_order_relaxed);
    parameters.foldPercent = state.foldPercent.load(std::memory_order_relaxed);
    parameters.outputDb = state.outputDb.load(std::memory_order_relaxed);
    parameters.unisonVoices = state.unisonVoices.load(std::memory_order_relaxed);
    parameters.unisonDetuneCents = state.unisonDetuneCents.load(std::memory_order_relaxed);
    parameters.unisonStereoPercent = state.unisonStereoPercent.load(std::memory_order_relaxed);
    parameters.subLevelPercent = state.subLevelPercent.load(std::memory_order_relaxed);
    parameters.subOctaveDown2 = state.subOctaveDown2.load(std::memory_order_relaxed);
    parameters.phasePercent = state.phasePercent.load(std::memory_order_relaxed);
    parameters.phaseRandomizeEnabled = state.phaseRandomizeEnabled.load(std::memory_order_relaxed);

    return parameters;
}

DahdsrEnvelope::Parameters SynthVoice::readEnvelopeParameters() const
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

VoiceFilter::BlockParameters SynthVoice::readFilterParameters() const
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

Lfo::Parameters SynthVoice::readLfoParameters() const
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
