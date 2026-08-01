#pragma once

#include <string>
#include <vector>

#include "Theory/ChordType.h"
#include "Theory/NoteName.h"

namespace theory
{

struct Chord
{
    std::string symbol;          // chords.json dict key, e.g. "Em7b9" - the internal/lookup id
    std::string readableName;    // chords.json "readableName" - display string, e.g. "C7"
    ChordType type = ChordType::Triad;
    int popularityOrder = 1;     // 1-based, 1 = most popular = default voicing
    std::vector<NoteName> notes; // chords.json array order (bass-first for inversions)
};

}
