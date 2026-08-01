#include "Theory/ProgressionPresetFactory.h"

#include <juce_core/juce_core.h>

namespace theory
{

ProgressionPreset ProgressionPresetFactory::createBuiltIn(const std::string& id, const std::string& nameKey,
                                                            const std::vector<Scale>& applicableScales,
                                                            const std::vector<Degree>& degreesInOrder)
{
    ProgressionPreset preset;
    preset.id = id;
    preset.nameKey = nameKey;
    preset.applicableScales = applicableScales;

    preset.slots.reserve(degreesInOrder.size());
    for (const auto degree : degreesInOrder)
        preset.slots.push_back(ProgressionSlot { degree, 0 }); // unpinned - built-ins always resolve dynamically

    return preset;
}

ProgressionPreset ProgressionPresetFactory::createFromSlots(const std::string& displayName, const std::vector<ProgressionSlot>& slots, Scale builtOnScale)
{
    ProgressionPreset preset;
    preset.id = "user_" + juce::Uuid().toString().toStdString();
    preset.displayName = displayName;
    preset.slots = slots;

    if (builtOnScale == Scale::MinorBlues)
        preset.applicableScales = { Scale::MinorBlues };

    return preset;
}

}
