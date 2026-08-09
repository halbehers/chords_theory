#pragma once

#include <vector>

#include <juce_core/juce_core.h>

namespace theory
{

// Pure-data mirror of component::MidiEditor's private note state - lets MidiExporter and
// SessionStateSerializer (both Theory layer) consume MidiEditor's content without depending on
// nui::Component, same rationale as SessionState itself. MidiEditor::getState()/restoreState()
// bridge to/from this. Deliberately carries no chord-block data - the chord lane is entirely
// derived from notes (see MidiEditor::recomputeChordBlocksFromNotes), so persisting it would be
// redundant and risk drifting from whatever the notes actually represent.
struct MidiEditorNoteState
{
    int midiNote = 60;
    double startBeat = 0.0;
    double lengthBeats = 1.0;

    bool operator==(const MidiEditorNoteState& other) const
    {
        return midiNote == other.midiNote
            && juce::approximatelyEqual(startBeat, other.startBeat)
            && juce::approximatelyEqual(lengthBeats, other.lengthBeats);
    }
};

struct MidiEditorState
{
    std::vector<MidiEditorNoteState> notes;

    bool operator==(const MidiEditorState& other) const
    {
        return notes == other.notes;
    }
};

}
