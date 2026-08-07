#include "Audio/ProgressionPlayer.h"

#include <algorithm>
#include <cmath>

namespace audio
{

ProgressionPlayer::ProgressionPlayer()
{
    _currentlySoundingNotes.reserve(kMaxScheduledNotes);
}

void ProgressionPlayer::setNotes(const std::vector<ScheduledNote>& notes)
{
    const auto inactiveIndex = 1 - _activeBufferIndex.load(std::memory_order_relaxed);
    auto& buffer = _noteBuffers[inactiveIndex];

    const auto count = juce::jmin(static_cast<int>(notes.size()), kMaxScheduledNotes);
    for (int i = 0; i < count; ++i)
        buffer[static_cast<std::size_t>(i)] = notes[static_cast<std::size_t>(i)];

    _noteCounts[inactiveIndex].store(count, std::memory_order_relaxed);
    _activeBufferIndex.store(inactiveIndex, std::memory_order_release);
}

void ProgressionPlayer::setLoopBounds(double loopStartBeat, double loopEndBeat)
{
    _loopStartBeat.store(loopStartBeat, std::memory_order_relaxed);
    _loopEndBeat.store(loopEndBeat, std::memory_order_relaxed);
}

void ProgressionPlayer::play()
{
    // Written before isPlaying (release) so the audio thread's acquire-load of isPlaying is
    // guaranteed to observe this seek too - see the class doc comment.
    _samplesIntoLoop.store(0, std::memory_order_relaxed);
    _playheadBeat.store(_loopStartBeat.load(std::memory_order_relaxed), std::memory_order_release);
    _isPlaying.store(true, std::memory_order_release);
}

void ProgressionPlayer::stop()
{
    _isPlaying.store(false, std::memory_order_release);
}

bool ProgressionPlayer::isPlaying() const
{
    return _isPlaying.load(std::memory_order_relaxed);
}

double ProgressionPlayer::getPlayheadBeat() const
{
    return _playheadBeat.load(std::memory_order_relaxed);
}

void ProgressionPlayer::renderNextBlock(juce::MidiBuffer& midiMessages, int numSamples, double sampleRate, double bpm)
{
    const auto playing = _isPlaying.load(std::memory_order_acquire);

    if (!playing)
    {
        // Force-release anything still sounding from the moment playback stopped - immediate, at
        // the very start of this block. A no-op (empty vector) on every subsequent stopped block.
        for (const int note : _currentlySoundingNotes)
            midiMessages.addEvent(juce::MidiMessage::noteOff(kMidiChannel, note), 0);
        _currentlySoundingNotes.clear();
        return;
    }

    if (bpm <= 0.0 || sampleRate <= 0.0)
        return; // defensive - avoid a divide-by-zero/nonsensical schedule

    const auto activeIndex = _activeBufferIndex.load(std::memory_order_acquire);
    const auto& notes = _noteBuffers[static_cast<std::size_t>(activeIndex)];
    const auto noteCount = _noteCounts[static_cast<std::size_t>(activeIndex)].load(std::memory_order_relaxed);

    const auto loopStart = _loopStartBeat.load(std::memory_order_relaxed);
    const auto loopEnd = _loopEndBeat.load(std::memory_order_relaxed);
    const auto loopLengthBeats = loopEnd - loopStart;
    if (loopLengthBeats <= 0.0)
        return; // defensive - a degenerate/unset loop region

    const auto beatsPerSample = (bpm / 60.0) / sampleRate;

    // The loop's length fixed as an exact sample count for this call - the authoritative wrap
    // point, so the within-block walk below is exact integer arithmetic with no std::ceil
    // ambiguity. Recomputed fresh every call (not persisted) so a live loop-bounds resize takes
    // effect immediately; see samplesIntoLoop's own doc comment for why *that* stays a persisted,
    // exact integer rather than ever being derived by repeated addition.
    const auto loopLengthSamples = juce::jmax(juce::int64 { 1 }, static_cast<juce::int64>(std::llround(loopLengthBeats / beatsPerSample)));

    auto samplesIntoLoop = _samplesIntoLoop.load(std::memory_order_relaxed);
    int sampleCursor = 0;
    int remainingSamples = numSamples;

    // Walks the block in one or more spans, splitting at the loop boundary whenever the block
    // straddles it, so a note right at the seam is always scheduled (or wrap-released) at the
    // correct sample offset rather than smeared across the wrap.
    while (remainingSamples > 0)
    {
        const auto samplesUntilWrap = static_cast<int>(juce::jlimit(juce::int64 { 1 }, static_cast<juce::int64>(remainingSamples),
            juce::jmax(juce::int64 { 1 }, loopLengthSamples - samplesIntoLoop)));
        const auto spanStartBeat = loopStart + static_cast<double>(samplesIntoLoop) * beatsPerSample;
        const auto spanEndBeat = loopStart + static_cast<double>(samplesIntoLoop + samplesUntilWrap) * beatsPerSample;

        for (int i = 0; i < noteCount; ++i)
        {
            const auto& note = notes[static_cast<std::size_t>(i)];
            const auto noteStart = note.startBeat;
            const auto noteEnd = note.startBeat + note.lengthBeats;

            if (noteStart >= spanStartBeat && noteStart < spanEndBeat)
            {
                const auto offset = sampleCursor + static_cast<int>(std::round((noteStart - spanStartBeat) / beatsPerSample));
                midiMessages.addEvent(juce::MidiMessage::noteOn(kMidiChannel, note.midiNote, kPlaybackVelocity), juce::jlimit(0, numSamples - 1, offset));
                _currentlySoundingNotes.push_back(note.midiNote);
            }

            if (noteEnd >= spanStartBeat && noteEnd < spanEndBeat)
            {
                const auto offset = sampleCursor + static_cast<int>(std::round((noteEnd - spanStartBeat) / beatsPerSample));
                midiMessages.addEvent(juce::MidiMessage::noteOff(kMidiChannel, note.midiNote), juce::jlimit(0, numSamples - 1, offset));
                _currentlySoundingNotes.erase(std::remove(_currentlySoundingNotes.begin(), _currentlySoundingNotes.end(), note.midiNote), _currentlySoundingNotes.end());
            }
        }

        sampleCursor += samplesUntilWrap;
        remainingSamples -= samplesUntilWrap;
        samplesIntoLoop += samplesUntilWrap;

        if (samplesIntoLoop >= loopLengthSamples)
        {
            // Never let a note ring past the loop boundary, even one that hasn't reached its own
            // natural end yet.
            for (const int note : _currentlySoundingNotes)
                midiMessages.addEvent(juce::MidiMessage::noteOff(kMidiChannel, note), juce::jlimit(0, numSamples - 1, sampleCursor));
            _currentlySoundingNotes.clear();

            samplesIntoLoop = 0; // exact integer reset - no fractional-beat remainder to carry, ever
        }
    }

    _samplesIntoLoop.store(samplesIntoLoop, std::memory_order_relaxed);
    _playheadBeat.store(loopStart + static_cast<double>(samplesIntoLoop) * beatsPerSample, std::memory_order_relaxed);
}

}
