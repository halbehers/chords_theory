#include "Audio/ChordSynthEngine.h"

#include "Audio/SineSynthSound.h"
#include "Audio/SineSynthVoice.h"

namespace audio
{

ChordSynthEngine::ChordSynthEngine()
{
    _synth.addSound(new SineSynthSound());

    for (int i = 0; i < kNumVoices; ++i)
        _synth.addVoice(new SineSynthVoice());
}

void ChordSynthEngine::prepare(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    _synth.setCurrentPlaybackSampleRate(sampleRate);
}

void ChordSynthEngine::renderNextBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages, int startSample, int numSamples)
{
    _keyboardState.processNextMidiBuffer(midiMessages, startSample, numSamples, true);
    _synth.renderNextBlock(buffer, midiMessages, startSample, numSamples);
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

}
