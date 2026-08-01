#pragma once

#include <vector>

#include "Theory/Chord.h"
#include "Theory/Degree.h"

namespace theory
{

struct ScaleDegreeData
{
    Degree degree = Degree::I;
    std::vector<Chord> chords; // sorted ascending by popularityOrder; chords[0] is the default voicing
};

}
