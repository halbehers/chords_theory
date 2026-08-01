#pragma once

#include <string>

namespace theory
{

// Mirrors chords.json's "type" field 1:1, in the same order as generate.py's static POPULARITY
// priority list - this ordering is purely for a readable/typed name though: actual popularity
// ranking always comes from a chord's own "popularityOrder" int in the JSON, never re-derived
// from this enum's declaration order.
enum class ChordType
{
    Triad, Seventh, Ninth, Sus4, Sus2, Add9, Sixth, Sus4Seventh, Sus4Ninth, Power, Eleventh, Thirteenth, Inversion1, Inversion2
};

// Parses chords.json's "type" string (e.g. "7sus4", "inversion-1") into a ChordType.
ChordType parseChordType(const std::string& jsonType);

}
