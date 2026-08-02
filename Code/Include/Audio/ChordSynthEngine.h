#pragma once

#include <vector>

#include <juce_audio_basics/juce_audio_basics.h> // includes juce::AudioPlayHead
#include <juce_events/juce_events.h>

#include "Audio/VoiceSharedState.h"

namespace audio
{

// Owns the polyphonic synth and bridges UI-thread "preview this chord" clicks into the
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

    void prepare(double sampleRate, int samplesPerBlock, int numChannels = 2);

    // Audio thread only. playHead is nullable (the host may not provide one at all - notably the
    // Standalone target, which has no transport) and even when present may not report a tempo
    // (AudioPlayHead::getPosition()/PositionInfo::getBpm() are both Optional) - kFallbackBpm
    // covers both cases.
    void renderNextBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages, int startSample, int numSamples, juce::AudioPlayHead* playHead = nullptr);

    // Message/UI thread only. Releases any still-sounding previous preview immediately, then
    // plays midiNotes together for a fixed duration. Empty input is a safe no-op (past releasing
    // whatever was previously sounding).
    void previewChord(const std::vector<int>& midiNotes);

    // Stops any still-sounding preview immediately - call on teardown.
    void reset();

    // Written from Parameters.cpp's onChange callbacks (registerParameter), read by every voice -
    // see VoiceSharedState's own doc comment for the cross-thread contract.
    [[nodiscard]] VoiceSharedState& getSharedState() { return _sharedState; }

private:
    void timerCallback() override;
    void releaseActiveNotes();
    void advanceFreeLfoPhase(int numSamples);

    static constexpr int kMidiChannel = 1;
    static constexpr float kPreviewVelocity = 0.9f;
    static constexpr int kPreviewDurationMs = 1000;
    static constexpr int kNumVoices = 16;
    // Matches Theory::MidiExporter::kFallbackBpm's precedent (Audio doesn't depend on Theory, so
    // this is its own constant rather than a cross-layer include).
    static constexpr double kFallbackBpm = 120.0;

    // Declared before _synth deliberately - every SynthVoice added to _synth holds a
    // reference to _sharedState, so it must be constructed first and destroyed last (C++ member
    // destruction order is the reverse of declaration order).
    VoiceSharedState _sharedState;
    juce::Synthesiser _synth;
    juce::MidiKeyboardState _keyboardState;
    std::vector<int> _activeNotes;
    double _sampleRate = 44100.0;
};

}
