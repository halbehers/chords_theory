#pragma once

#include <string>
#include <vector>

#include <juce_core/juce_core.h>

#include "Theory/ProgressionSlot.h"

namespace theory
{

// Pure-data mirror of component::MidiEditor's private note/chord-block state - lets MidiExporter
// and SessionStateSerializer (both Theory layer) consume MidiEditor's content without depending on
// nui::Component, same rationale as SessionState itself. MidiEditor::getState()/restoreState()
// bridge to/from this.
struct MidiEditorNoteState
{
    int midiNote = 60;
    double startBeat = 0.0;
    double lengthBeats = 1.0;
    int sourceChordId = -1; // which chord block created this note; -1 = created directly (double-click)

    bool operator==(const MidiEditorNoteState& other) const
    {
        return midiNote == other.midiNote
            && juce::approximatelyEqual(startBeat, other.startBeat)
            && juce::approximatelyEqual(lengthBeats, other.lengthBeats)
            && sourceChordId == other.sourceChordId;
    }
};

struct MidiEditorChordBlockState
{
    int id = -1;
    std::string label;       // frozen snapshot of Chord::readableName at drop time
    double startBeat = 0.0;
    double lengthBeats = 1.0;
    ProgressionSlot sourceSlot; // frozen at drop time - degree + the resolved chord's popularityOrder

    bool operator==(const MidiEditorChordBlockState& other) const
    {
        return id == other.id && label == other.label
            && juce::approximatelyEqual(startBeat, other.startBeat)
            && juce::approximatelyEqual(lengthBeats, other.lengthBeats)
            && sourceSlot == other.sourceSlot;
    }
};

struct MidiEditorState
{
    std::vector<MidiEditorNoteState> notes;
    std::vector<MidiEditorChordBlockState> chordBlocks;
    int nextChordBlockId = 0;

    bool operator==(const MidiEditorState& other) const
    {
        return notes == other.notes && chordBlocks == other.chordBlocks && nextChordBlockId == other.nextChordBlockId;
    }
};

}
