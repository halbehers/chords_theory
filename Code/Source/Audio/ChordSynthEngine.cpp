#include "Audio/ChordSynthEngine.h"

#include <cmath>

#include "Audio/SynthSound.h"
#include "Audio/SynthVoice.h"

namespace audio
{

ChordSynthEngine::ChordSynthEngine()
{
    _synth.addSound(new SynthSound());

    for (int i = 0; i < kNumVoices; ++i)
        _synth.addVoice(new SynthVoice(_sharedState));
}

void ChordSynthEngine::prepare(double sampleRate, int samplesPerBlock, int numChannels)
{
    _sampleRate = sampleRate;
    _synth.setCurrentPlaybackSampleRate(sampleRate);

    // juce::SynthesiserVoice has no channel-count-aware prepare hook - each voice's VoiceFilter
    // needs the negotiated channel count directly, so it's pushed in here explicitly.
    for (int i = 0; i < _synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<SynthVoice*>(_synth.getVoice(i)))
            voice->prepareFilter(sampleRate, numChannels);

    _masterBus.prepare(sampleRate, samplesPerBlock, numChannels);
    _leftWaveformFifo.prepare(samplesPerBlock);
    _rightWaveformFifo.prepare(samplesPerBlock);
}

void ChordSynthEngine::renderNextBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages, int startSample, int numSamples, juce::AudioPlayHead* playHead)
{
    auto bpm = kFallbackBpm;
    if (playHead != nullptr)
        if (const auto position = playHead->getPosition())
            if (const auto hostBpm = position->getBpm())
                bpm = *hostBpm;

    _sharedState.hostBpm = bpm;

    _progressionPlayer.renderNextBlock(midiMessages, numSamples, _sampleRate, bpm);

    // Advanced unconditionally, whether or not anything is currently sounding - this is what
    // makes the LFO's Free mode a genuinely continuous, globally-shared clock rather than
    // something tied to any one note (see Lfo's and VoiceSharedState's doc comments for how each
    // voice re-seeds its own per-sample-stepped copy from this value at the top of every block).
    advanceFreeLfoPhase(numSamples);

    _keyboardState.processNextMidiBuffer(midiMessages, startSample, numSamples, true);
    _synth.renderNextBlock(buffer, midiMessages, startSample, numSamples);

    // A non-owning view over exactly the [startSample, startSample+numSamples) range that was
    // just rendered into - the master bus and waveform taps operate on precisely the same range
    // the voices did, not necessarily the whole buffer (today's only caller always passes the
    // whole buffer with startSample == 0, but this stays correct even if that ever changes).
    juce::AudioBuffer<float> outputBlock(buffer.getArrayOfWritePointers(), buffer.getNumChannels(), startSample, numSamples);

    _masterBus.setBlockParameters({
        .compressorAmountPercent = _masterBusState.compressorAmountPercent.load(std::memory_order_relaxed),
        .panPercent = _masterBusState.panPercent.load(std::memory_order_relaxed),
        .outputDb = _masterBusState.outputDb.load(std::memory_order_relaxed),
    });
    _masterBus.process(outputBlock);

    if (outputBlock.getNumChannels() > 1)
    {
        _leftWaveformFifo.update(outputBlock);
        _rightWaveformFifo.update(outputBlock);
    }
}

void ChordSynthEngine::previewChord(const std::vector<int>& midiNotes)
{
    releaseActiveNotes();

    if (midiNotes.empty())
        return;

    _activeNotes = midiNotes;

    for (const int note : _activeNotes)
        _keyboardState.noteOn(kMidiChannel, note, kPreviewVelocity);

    startTimer(kPreviewDurationMs);
}

void ChordSynthEngine::reset()
{
    stopTimer();
    releaseActiveNotes();
    _progressionPlayer.stop();
}

void ChordSynthEngine::timerCallback()
{
    releaseActiveNotes();
    stopTimer();
}

void ChordSynthEngine::releaseActiveNotes()
{
    for (const int note : _activeNotes)
        _keyboardState.noteOff(kMidiChannel, note, 0.0f);

    _activeNotes.clear();
}

void ChordSynthEngine::advanceFreeLfoPhase(int numSamples)
{
    if (_sampleRate <= 0.0)
        return;

    const auto syncEnabled = _sharedState.lfoSyncEnabled.load(std::memory_order_relaxed);
    const auto rateHz = syncEnabled
        ? static_cast<double>(ndsp::Timing::getRate(_sharedState.hostBpm, static_cast<ndsp::Timing::NoteTiming>(_sharedState.lfoSyncDivision.load(std::memory_order_relaxed))))
        : static_cast<double>(_sharedState.lfoRateHz.load(std::memory_order_relaxed));

    const auto phaseIncrement = rateHz / _sampleRate * static_cast<double>(numSamples);
    _sharedState.freeLfoPhase01 = std::fmod(_sharedState.freeLfoPhase01 + phaseIncrement, 1.0);
}

}
