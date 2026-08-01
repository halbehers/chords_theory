#pragma once

#include <string>
#include <vector>

#include "Theory/Degree.h"
#include "Theory/ProgressionPreset.h"
#include "Theory/ProgressionSlot.h"
#include "Theory/Scale.h"

namespace theory
{

class ProgressionPresetFactory
{
public:
    // Builds a built-in preset from a plain ordered degree list - the common case, covers every
    // entry in BuiltInProgressionPresets.cpp, including 12-bar blues as 12 individual degrees.
    static ProgressionPreset createBuiltIn(const std::string& id, const std::string& nameKey,
                                            const std::vector<Scale>& applicableScales,
                                            const std::vector<Degree>& degreesInOrder);

    // Builds a user-saved preset from the progression sequencer's current live slot contents.
    // builtOnScale scopes the preset: Minor Blues presets only ever reference I/IV/V, so they're
    // pinned to Minor Blues specifically; presets built on any other (7-degree) scale are left
    // applicable to all 7-degree scales, matching how the built-in generic presets behave, since
    // they only reference degrees, not concrete chords.
    static ProgressionPreset createFromSlots(const std::string& displayName, const std::vector<ProgressionSlot>& slots, Scale builtOnScale);
};

}
