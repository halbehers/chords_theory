#include "Theory/ChordType.h"

#include <stdexcept>

namespace theory
{

ChordType parseChordType(const std::string& jsonType)
{
    if (jsonType == "triad")        return ChordType::Triad;
    if (jsonType == "seventh")      return ChordType::Seventh;
    if (jsonType == "ninth")        return ChordType::Ninth;
    if (jsonType == "sus4")         return ChordType::Sus4;
    if (jsonType == "sus2")         return ChordType::Sus2;
    if (jsonType == "add9")         return ChordType::Add9;
    if (jsonType == "sixth")        return ChordType::Sixth;
    if (jsonType == "7sus4")        return ChordType::Sus4Seventh;
    if (jsonType == "9sus4")        return ChordType::Sus4Ninth;
    if (jsonType == "power")        return ChordType::Power;
    if (jsonType == "eleventh")     return ChordType::Eleventh;
    if (jsonType == "thirteenth")   return ChordType::Thirteenth;
    if (jsonType == "inversion-1")  return ChordType::Inversion1;
    if (jsonType == "inversion-2")  return ChordType::Inversion2;

    throw std::invalid_argument("Unknown chord type in chord database: " + jsonType);
}

}
