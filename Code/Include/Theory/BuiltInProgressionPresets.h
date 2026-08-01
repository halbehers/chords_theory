#pragma once

#include <vector>

#include "Theory/ProgressionPreset.h"

namespace theory
{

// The starter set of built-in progression presets shown in ProgressionPresetPicker, ahead of any
// user-saved ones (see ProgressionPresetLibrary). Adding a new one is adding one entry to the
// static table in the .cpp - see ProgressionPresetFactory::createBuiltIn.
const std::vector<ProgressionPreset>& getBuiltInProgressionPresets();

}
