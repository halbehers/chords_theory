#pragma once

#include <string>

namespace theory
{

enum class Scale
{
    Major, Minor, HarmonicMinor, MelodicMinor, Dorian, Phrygian, Lydian, Mixolydian, Locrian, MinorBlues
};

constexpr int kNumScales = 10;

// Matches chords.json's per-key scale-name string, e.g. Scale::HarmonicMinor -> "Harmonic Minor".
std::string getScaleJsonKey(Scale scale);

// Inverse of getScaleJsonKey(). Throws std::invalid_argument if jsonKey isn't a recognized scale name.
Scale parseScale(const std::string& jsonKey);

// .lang translation key for this scale's display name, e.g. "scale_harmonic_minor".
std::string getScaleTranslationKey(Scale scale);

}
