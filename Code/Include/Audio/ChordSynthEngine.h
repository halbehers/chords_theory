#pragma once

#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_events/juce_events.h>

namespace audio
{

// Owns the polyphonic sine synth and bridges UI-thread "preview this chord" clicks into the
// audio-thread Synthesiser via juce::MidiKeyboardState - JUCE's standard, safety-appropriate
// pattern for this exact cross-thread scenario (its critical sections only ever do trivial
// bitmask/MidiBuffer::addEvent work, unlike Synthesiser::noteOn/noteOff, which lock around an
// entire buffer's render). previewChord() plays a fixed-duration audition (~1s) rather than
// tracking press-and-hold, since the UI's drag-to-export gesture can block mouse-up delivery -
// see ChordCard.
class ChordSynthEngine : private juce::Timer
{
public:
    ChordSynthEngine();

    void prepare(double sampleRate, int samplesPerBlock);

    // Audio thread only.
    void renderNextBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages, int startSample, int numSamples);

    // Message/UI thread only. Releases any still-sounding previous preview immediately, then
    // plays midiNotes together for a fixed duration. Empty input is a safe no-op (past releasing
    // whatever was previously sounding).
    void previewChord(const std::vector<int>& midiNotes);

    // Stops any still-sounding preview immediately - call on teardown.
    void reset();

private:
    void timerCallback() override;
    void releaseActiveNotes();

    static constexpr int kMidiChannel = 1;
    static constexpr float kPreviewVelocity = 0.9f;
    static constexpr int kPreviewDurationMs = 1000;
    static constexpr int kNumVoices = 16;

    juce::Synthesiser _synth;
    juce::MidiKeyboardState _keyboardState;
    std::vector<int> _activeNotes;
};

}
