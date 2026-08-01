#include "Theory/Scale.h"

#include <stdexcept>

namespace theory
{

std::string getScaleJsonKey(Scale scale)
{
    switch (scale)
    {
        case Scale::Major:         return "Major";
        case Scale::Minor:         return "Minor";
        case Scale::HarmonicMinor: return "Harmonic Minor";
        case Scale::MelodicMinor:  return "Melodic Minor";
        case Scale::Dorian:        return "Dorian";
        case Scale::Phrygian:      return "Phrygian";
        case Scale::Lydian:        return "Lydian";
        case Scale::Mixolydian:    return "Mixolydian";
        case Scale::Locrian:       return "Locrian";
        case Scale::MinorBlues:    return "Minor Blues";
    }

    return "Major";
}

std::string getScaleTranslationKey(Scale scale)
{
    switch (scale)
    {
        case Scale::Major:         return "scale_major";
        case Scale::Minor:         return "scale_minor";
        case Scale::HarmonicMinor: return "scale_harmonic_minor";
        case Scale::MelodicMinor:  return "scale_melodic_minor";
        case Scale::Dorian:        return "scale_dorian";
        case Scale::Phrygian:      return "scale_phrygian";
        case Scale::Lydian:        return "scale_lydian";
        case Scale::Mixolydian:    return "scale_mixolydian";
        case Scale::Locrian:       return "scale_locrian";
        case Scale::MinorBlues:    return "scale_minor_blues";
    }

    return "scale_major";
}

Scale parseScale(const std::string& jsonKey)
{
    if (jsonKey == "Major")          return Scale::Major;
    if (jsonKey == "Minor")          return Scale::Minor;
    if (jsonKey == "Harmonic Minor") return Scale::HarmonicMinor;
    if (jsonKey == "Melodic Minor")  return Scale::MelodicMinor;
    if (jsonKey == "Dorian")         return Scale::Dorian;
    if (jsonKey == "Phrygian")       return Scale::Phrygian;
    if (jsonKey == "Lydian")         return Scale::Lydian;
    if (jsonKey == "Mixolydian")     return Scale::Mixolydian;
    if (jsonKey == "Locrian")        return Scale::Locrian;
    if (jsonKey == "Minor Blues")    return Scale::MinorBlues;

    throw std::invalid_argument("Unknown scale name: " + jsonKey);
}

}
