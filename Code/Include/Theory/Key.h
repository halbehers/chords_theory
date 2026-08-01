#pragma once

#include <string>

namespace theory
{

enum class Key
{
    C, Db, D, Eb, E, F, Gb, G, Ab, A, Bb, B
};

constexpr int kNumKeys = 12;

// Matches chords.json's top-level key string for this Key, e.g. Key::Db -> "Db".
std::string getKeyJsonKey(Key key);

// Display label shown in the UI - identical to the JSON key for all 12 entries, since chords.json
// already picks one canonical spelling per pitch class (no enharmonic duplicates like C#/Db both
// present), but kept as a separate accessor so UI code never assumes that will always be true.
std::string getKeyLabel(Key key);

// Inverse of getKeyJsonKey(). Throws std::invalid_argument if jsonKey isn't one of the 12 recognized keys.
Key parseKey(const std::string& jsonKey);

}
