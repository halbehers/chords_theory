#pragma once

#include <array>

#include "Theory/Key.h"
#include "Theory/KeyScaleData.h"
#include "Theory/Scale.h"

namespace theory
{

// Parses the bundled chords.json once (via juce::JSON::parse) and caches the result in a
// process-wide singleton, so multiple plugin instances loaded into the same host process share
// one parse rather than repeating it per instance.
class ChordDatabase
{
public:
    static const ChordDatabase& getInstance();

    [[nodiscard]] const KeyScaleData& get(Key key, Scale scale) const;

private:
    ChordDatabase();

    // Flat index, avoids hashing/string-keying at UI-interaction time - the JSON's string keys
    // only get touched once, during this constructor's parse.
    std::array<KeyScaleData, static_cast<std::size_t>(kNumKeys) * static_cast<std::size_t>(kNumScales)> _index;
};

}
