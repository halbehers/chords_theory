#pragma once

#include <string>

#include "Theory/ProgressionSlot.h"

namespace theory
{

// ChordIdentifier::identify's result: the matched Chord::readableName plus the ProgressionSlot
// (degree + popularityOrder) it corresponds to under the Key/Scale that was searched.
struct DetectedChord
{
    std::string label;
    ProgressionSlot slot;
};

}
