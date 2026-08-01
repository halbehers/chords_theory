#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "Theory/ProgressionSlot.h"
#include "Theory/Scale.h"

namespace theory
{

struct ProgressionPreset
{
    std::string id;                     // stable key, e.g. "twelve_bar_blues" or "user_<uuid>"
    std::string nameKey;                // .lang translation key - built-in presets only
    std::string displayName;            // literal name - user-saved presets only (nameKey empty)
    std::vector<Scale> applicableScales; // empty = applicable to any (non-Minor-Blues) scale
    std::vector<ProgressionSlot> slots;

    [[nodiscard]] bool isUserPreset() const { return nameKey.empty(); }

    // An empty applicableScales list means "any of the 9 standard 7-degree scales" - Minor Blues
    // only has I/IV/V, so a preset must opt in explicitly (list it in applicableScales) to be
    // usable there, rather than accidentally matching and then failing to resolve a II/III/VI/VII
    // slot at render time.
    [[nodiscard]] bool isApplicableTo(Scale scale) const
    {
        if (!applicableScales.empty())
            return std::find(applicableScales.begin(), applicableScales.end(), scale) != applicableScales.end();

        return scale != Scale::MinorBlues;
    }
};

}
