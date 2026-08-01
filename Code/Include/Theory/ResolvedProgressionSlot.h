#pragma once

#include "Theory/Chord.h"
#include "Theory/ProgressionSlot.h"

namespace theory
{

// Pairs a ProgressionSlot with the actual Chord it resolves to given the current per-degree
// voicing map, at the moment of export - MidiExporter::writeProgressionMidiFile's input, so it
// never needs to know about ChordDegreeBrowser/voicing-resolution itself.
struct ResolvedProgressionSlot
{
    ProgressionSlot slot;
    Chord chord;
};

}
