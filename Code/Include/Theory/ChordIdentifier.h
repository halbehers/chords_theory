#pragma once

#include <optional>
#include <vector>

#include "Theory/DetectedChord.h"
#include "Theory/Key.h"
#include "Theory/Scale.h"

namespace theory
{

// Stateless reverse lookup: given a set of MIDI notes sounding together, identifies which cataloged
// chords.json chord they form under a specific Key/Scale, if any. No instance state - same shape as
// NoteConvertor.
class ChordIdentifier
{
public:
    // Bass = the pitch class of the lowest note in midiNotes; the full pitch-class set is every
    // note's pitch class, deduplicated. Searches ChordDatabase::getInstance().get(key,scale)'s
    // degrees for a chord whose own bass-first notes (Chord::notes.front() as the bass, the full
    // array as the set) match exactly - confirmed unique (bass, pitch-class-set) pairs within any
    // single Key/Scale's full chord list, so at most one match is ever possible. Deliberately does
    // NOT fall back to searching other keys/scales if nothing matches - a non-diatonic note cluster
    // (relative to key/scale) simply has no detected chord.
    static std::optional<DetectedChord> identify(const std::vector<int>& midiNotes, Key key, Scale scale);
};

}
