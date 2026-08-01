#pragma once

#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

#include "Theory/Chord.h"
#include "Theory/ResolvedProgressionSlot.h"

namespace theory
{

// Writes temporary Standard MIDI Files used to get a chord out of the plugin and into a DAW (or
// one of this plugin's own progression slots) via juce::DragAndDropContainer::
// performExternalDragDropOfFiles - see AppLayout for how the resulting file is actually dragged.
// Stateless utility class, same shape as NoteConvertor.
class MidiExporter
{
public:
    static constexpr int kTicksPerQuarterNote = 960;
    static constexpr int kBeatsPerMeasure = 4;      // 4/4 fallback assumption
    static constexpr double kFallbackBpm = 120.0;
    static constexpr juce::uint8 kVelocity = 100;

    // Writes `chord`, closed-voiced near middle C, as a one-measure clip starting at tick 0 to a
    // fresh temp file under <tempDir>/ChordsTheory/. bpm only affects the file's declared tempo
    // meta-event, not the note's tick positions (a "measure" is always kBeatsPerMeasure quarter
    // notes' worth of ticks, independent of tempo).
    static juce::File writeSingleChordMidiFile(const Chord& chord, double bpm = kFallbackBpm);

    // Writes every slot's chord to one file, each starting at its own cumulative one-measure
    // offset (slots[0] at tick 0, slots[1] at tick kBeatsPerMeasure*kTicksPerQuarterNote, ...).
    static juce::File writeProgressionMidiFile(const std::vector<ResolvedProgressionSlot>& slots, double bpm = kFallbackBpm);
};

}
