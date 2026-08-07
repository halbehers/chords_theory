#pragma once

#include <array>
#include <atomic>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

namespace audio
{

// One note in a scheduled progression - startBeat/lengthBeats mirror component::MidiEditor's own
// note timing exactly (1 beat == 1 quarter note). Deliberately Audio-local rather than
// theory::MidiEditorNoteState, so Audio doesn't depend on Theory/Component - same layering
// rationale as ChordSynthEngine's own duplicated kFallbackBpm constant.
struct ScheduledNote
{
    int midiNote = 60;
    double startBeat = 0.0;
    double lengthBeats = 1.0;
};

// Sample-accurate loop playback of a fixed set of notes, injecting real note-on/off
// juce::MidiMessages directly into the same MidiBuffer ChordSynthEngine::renderNextBlock already
// renders each block - reuses the existing polyphonic Synthesiser path unchanged, no new
// voice-management code needed.
//
// Cross-thread contract: setNotes/setLoopBounds/play/stop/isPlaying/getPlayheadBeat are message-
// thread only; renderNextBlock is audio-thread only, called from ChordSynthEngine::renderNextBlock.
// The note list is a fixed-capacity, wait-free double-buffer (message thread writes the *inactive*
// buffer, then flips an atomic index with release ordering; the audio thread acquire-loads that
// index once per block and reads whichever buffer it currently names) - no heap allocation, no
// locking, on either side. Loop bounds/play-state/playhead position are plain atomics; play()
// writes the playhead before the isPlaying flag (release ordering) so the audio thread's
// acquire-load of isPlaying is guaranteed to see the seeked playhead too.
class ProgressionPlayer
{
public:
    ProgressionPlayer();

    // Message thread only. Replaces the whole note list - takes effect on the next
    // renderNextBlock call. Notes beyond kMaxScheduledNotes are silently dropped (far above any
    // realistic progression's note count).
    void setNotes(const std::vector<ScheduledNote>& notes);

    // Message thread only. Takes effect on the next renderNextBlock call - if called while
    // already playing, a live resize is picked up immediately, not just on the next play().
    void setLoopBounds(double loopStartBeat, double loopEndBeat);

    // Message thread only. Seeks the playhead to the current loop start and begins playback.
    void play();

    // Message thread only. The audio thread force-releases any still-sounding notes on its next
    // renderNextBlock call.
    void stop();

    [[nodiscard]] bool isPlaying() const;
    [[nodiscard]] double getPlayheadBeat() const;

    // Audio thread only - called once per block from ChordSynthEngine::renderNextBlock, right
    // after bpm has been resolved (host tempo if available, else the caller's own fallback).
    void renderNextBlock(juce::MidiBuffer& midiMessages, int numSamples, double sampleRate, double bpm);

private:
    static constexpr int kMidiChannel = 1;
    static constexpr float kPlaybackVelocity = 0.9f;
    static constexpr int kMaxScheduledNotes = 512;

    std::array<ScheduledNote, kMaxScheduledNotes> _noteBuffers[2];
    std::atomic<int> _noteCounts[2] { 0, 0 };
    std::atomic<int> _activeBufferIndex { 0 };

    std::atomic<double> _loopStartBeat { 0.0 };
    std::atomic<double> _loopEndBeat { 4.0 };
    std::atomic<bool> _isPlaying { false };

    // The authoritative position within the current loop pass - an exact sample count, never
    // itself touched by floating-point arithmetic, so it can't accumulate rounding drift no matter
    // how many blocks/spans it's advanced across or how many passes it wraps through. A beat
    // position (for scheduling and for the UI's getPlayheadBeat()) is always *derived* fresh from
    // this via a single multiplication (loopStart + samplesIntoLoop * beatsPerSample), never by
    // repeatedly adding fractional beats to a running double - that repeated-addition approach was
    // the original bug: std::ceil's necessary round-up at each block boundary, compounded with
    // ordinary floating-point rounding in the beatsPerSample division, made the wrap land a hair
    // late (and the lateness itself compound pass over pass) purely from accumulated error.
    std::atomic<juce::int64> _samplesIntoLoop { 0 };
    std::atomic<double> _playheadBeat { 0.0 };

    // Audio-thread-only - which notes are currently sounding, so note-offs (both natural and
    // forced, at stop()/loop-wrap) know exactly what to release. Never touched by the message
    // thread. Reserved to kMaxScheduledNotes up front so it never reallocates on the audio thread.
    std::vector<int> _currentlySoundingNotes;
};

}
