#pragma once

#include <string>

namespace theory
{

enum class Degree
{
    I, II, III, IV, V, VI, VII
};

constexpr int kNumDegrees = 7;

// Matches chords.json's per-scale degree key, e.g. Degree::IV -> "IV". Also used as the display
// label - roman numerals are the standard notation for scale degrees, no separate translation needed.
std::string getDegreeLabel(Degree degree);

// Inverse of getDegreeLabel(). Throws std::invalid_argument if label isn't one of "I".."VII".
Degree parseDegree(const std::string& label);

}
