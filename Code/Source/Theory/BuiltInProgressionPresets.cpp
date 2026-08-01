#include "Theory/BuiltInProgressionPresets.h"

#include "Theory/ProgressionPresetFactory.h"

namespace theory
{

const std::vector<ProgressionPreset>& getBuiltInProgressionPresets()
{
    static const std::vector<ProgressionPreset> presets = {
        ProgressionPresetFactory::createBuiltIn("pop_i_v_vi_iv", "progression_preset_pop_i_v_vi_iv", {},
            { Degree::I, Degree::V, Degree::VI, Degree::IV }),

        ProgressionPresetFactory::createBuiltIn("fifty_progression", "progression_preset_fifty", {},
            { Degree::I, Degree::VI, Degree::IV, Degree::V }),

        ProgressionPresetFactory::createBuiltIn("pachelbel", "progression_preset_pachelbel", {},
            { Degree::I, Degree::V, Degree::VI, Degree::III, Degree::IV, Degree::I, Degree::IV, Degree::V }),

        ProgressionPresetFactory::createBuiltIn("jazz_ii_v_i", "progression_preset_jazz_ii_v_i", {},
            { Degree::II, Degree::V, Degree::I }),

        ProgressionPresetFactory::createBuiltIn("andalusian_cadence", "progression_preset_andalusian_cadence",
            { Scale::Minor, Scale::HarmonicMinor },
            { Degree::I, Degree::VII, Degree::VI, Degree::V }),

        ProgressionPresetFactory::createBuiltIn("minor_i_iv_v", "progression_preset_minor_i_iv_v", { Scale::Minor },
            { Degree::I, Degree::IV, Degree::V }),

        ProgressionPresetFactory::createBuiltIn("twelve_bar_blues", "progression_preset_twelve_bar_blues", { Scale::MinorBlues },
            { Degree::I, Degree::I, Degree::I, Degree::I,
              Degree::IV, Degree::IV, Degree::I, Degree::I,
              Degree::V, Degree::IV, Degree::I, Degree::I }),

        ProgressionPresetFactory::createBuiltIn("eight_bar_blues", "progression_preset_eight_bar_blues", { Scale::MinorBlues },
            { Degree::I, Degree::V, Degree::IV, Degree::IV,
              Degree::I, Degree::V, Degree::I, Degree::V }),
    };

    return presets;
}

}
