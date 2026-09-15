#pragma once

#include <array>

#include "Theory/Key.h"
#include "Theory/KeyScaleData.h"
#include "Theory/ProgressionSlot.h"
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

    // Resolves a key/scale-independent ProgressionSlot (degree + popularityOrder) back to the
    // concrete Chord it names under this specific key/scale - nullptr if that degree doesn't exist
    // for this scale (e.g. II/III/VI/VII on Minor Blues) or no chord at that degree has a matching
    // popularityOrder (voicing counts can differ between scales at the same degree). Stateless
    // Theory-layer counterpart to ChordDegreeBrowser::resolveSlot, minus its UI-specific fallback to
    // whatever's currently live on screen.
    [[nodiscard]] const Chord* resolveChord(Key key, Scale scale, const ProgressionSlot& slot) const;

private:
    ChordDatabase();

    // Flat index, avoids hashing/string-keying at UI-interaction time - the JSON's string keys
    // only get touched once, during this constructor's parse.
    std::array<KeyScaleData, static_cast<std::size_t>(kNumKeys) * static_cast<std::size_t>(kNumScales)> _index;
};

}
