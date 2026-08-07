#pragma once

#include <vector>

#include <juce_audio_basics/juce_audio_basics.h> // includes juce::AudioPlayHead
#include <juce_events/juce_events.h>
#include <nierika_dsp/nierika_dsp.h>

#include "Audio/MasterBus.h"
#include "Audio/MasterBusState.h"
#include "Audio/ProgressionPlayer.h"
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

    // Written from Parameters.cpp's onChange callbacks, read once per block by _masterBus - see
    // MasterBusState's own doc comment for why this is separate from VoiceSharedState.
    [[nodiscard]] MasterBusState& getMasterBusState() { return _masterBusState; }

    // Fed from the master output (post-MasterBus, the plugin's actual final audio) at the end of
    // every renderNextBlock - SynthOutputSection's CircularStereoWaveform reads these on the
    // message thread. Owned here (not on PluginAudioProcessor) so AppLayout's existing
    // ChordSynthEngine& reference is already enough to reach them, with no change needed to
    // AppLayout's/PluginEditor's own construction signatures.
    [[nodiscard]] ndsp::SingleChannelSampleFIFO<juce::AudioBuffer<float>>& getLeftWaveformFifo() { return _leftWaveformFifo; }
    [[nodiscard]] ndsp::SingleChannelSampleFIFO<juce::AudioBuffer<float>>& getRightWaveformFifo() { return _rightWaveformFifo; }

    // Sample-accurate progression-loop playback (see its own class doc comment for the
    // cross-thread contract) - MidiEditor reaches this via a raw pointer, same pattern as the
    // waveform FIFOs above.
    [[nodiscard]] ProgressionPlayer& getProgressionPlayer() { return _progressionPlayer; }

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
    ProgressionPlayer _progressionPlayer;

    MasterBusState _masterBusState;
    MasterBus _masterBus;
    ndsp::SingleChannelSampleFIFO<juce::AudioBuffer<float>> _leftWaveformFifo { ndsp::Channel::LEFT };
    ndsp::SingleChannelSampleFIFO<juce::AudioBuffer<float>> _rightWaveformFifo { ndsp::Channel::RIGHT };
};

}
