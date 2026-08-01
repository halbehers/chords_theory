#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <set>

#include "Theory/BuiltInProgressionPresets.h"
#include "Theory/ChordDatabase.h"
#include "Theory/ProgressionPresetFactory.h"
#include "Theory/ProgressionPresetLibrary.h"

using theory::ChordDatabase;
using theory::Degree;
using theory::getBuiltInProgressionPresets;
using theory::Key;
using theory::ProgressionPresetFactory;
using theory::ProgressionPresetLibrary;
using theory::Scale;

namespace
{
    juce::File makeTempUserPresetsFile()
    {
        auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("ProgressionPresetTests_" + juce::Uuid().toString() + ".xml");
        file.deleteFile();
        return file;
    }

    // All 12 keys x every scale in applicableScales (or all 9 standard scales if empty).
    std::vector<Scale> resolveScales(const theory::ProgressionPreset& preset)
    {
        if (!preset.applicableScales.empty())
            return preset.applicableScales;

        return { Scale::Major, Scale::Minor, Scale::HarmonicMinor, Scale::MelodicMinor, Scale::Dorian,
                 Scale::Phrygian, Scale::Lydian, Scale::Mixolydian, Scale::Locrian };
    }
}

TEST_CASE("Every built-in preset resolves against every scale it claims to be applicable to", "[ProgressionPreset]")
{
    const auto& database = ChordDatabase::getInstance();

    for (const auto& preset : getBuiltInProgressionPresets())
    {
        CAPTURE(preset.id);

        for (const auto scale : resolveScales(preset))
        {
            CAPTURE(static_cast<int>(scale));

            // Every key too, in case a scale/key combination is ever missing a degree (it isn't,
            // per ChordDatabaseTests, but this test's job is to catch exactly that kind of thing).
            for (int keyIndex = 0; keyIndex < theory::kNumKeys; ++keyIndex)
            {
                const auto key = static_cast<Key>(keyIndex);
                const auto& keyScaleData = database.get(key, scale);

                for (const auto& slot : preset.slots)
                    CHECK(keyScaleData.findDegree(slot.degree) != nullptr);
            }
        }
    }
}

TEST_CASE("Built-in preset ids are non-empty and mutually distinct", "[ProgressionPreset]")
{
    const auto& presets = getBuiltInProgressionPresets();

    std::vector<std::string> ids;
    for (const auto& preset : presets)
    {
        CHECK_FALSE(preset.id.empty());
        ids.push_back(preset.id);
    }

    const std::set<std::string> uniqueIds(ids.begin(), ids.end());
    CHECK(uniqueIds.size() == ids.size());
}

TEST_CASE("Twelve-bar blues preset has exactly 12 one-bar slots in the documented I-IV-V order", "[ProgressionPreset]")
{
    const auto& presets = getBuiltInProgressionPresets();
    const auto it = std::find_if(presets.begin(), presets.end(),
        [](const theory::ProgressionPreset& preset) { return preset.id == "twelve_bar_blues"; });

    REQUIRE(it != presets.end());
    REQUIRE(it->slots.size() == 12);

    const std::vector<Degree> expected = {
        Degree::I, Degree::I, Degree::I, Degree::I,
        Degree::IV, Degree::IV, Degree::I, Degree::I,
        Degree::V, Degree::IV, Degree::I, Degree::I,
    };

    for (std::size_t i = 0; i < expected.size(); ++i)
        CHECK(it->slots[i].degree == expected[i]);

    CHECK(it->isApplicableTo(Scale::MinorBlues));
    CHECK_FALSE(it->isApplicableTo(Scale::Major));
}

TEST_CASE("ProgressionPresetLibrary round-trips a user preset through a temp properties file", "[ProgressionPreset]")
{
    const auto file = makeTempUserPresetsFile();

    const auto preset = ProgressionPresetFactory::createFromSlots("My Progression",
        { { Degree::I, 0 }, { Degree::V, 0 }, { Degree::VI, 0 }, { Degree::IV, 0 } }, Scale::Major);

    {
        ProgressionPresetLibrary library(file);
        library.savePreset(preset);

        const auto all = library.getAllPresets();
        const auto it = std::find_if(all.begin(), all.end(),
            [&preset](const theory::ProgressionPreset& p) { return p.id == preset.id; });
        REQUIRE(it != all.end());
        CHECK(it->displayName == "My Progression");
        CHECK(it->slots.size() == 4);
    }

    // A second instance over the same file, exactly matching what happens across separate plugin
    // instances/DAW sessions reading the same global user-presets file.
    ProgressionPresetLibrary reloaded(file);
    const auto reloadedPresets = reloaded.getUserPresets();
    const auto it = std::find_if(reloadedPresets.begin(), reloadedPresets.end(),
        [&preset](const theory::ProgressionPreset& p) { return p.id == preset.id; });

    REQUIRE(it != reloadedPresets.end());
    CHECK(it->displayName == "My Progression");
    CHECK(it->slots.size() == 4);
    CHECK(it->isApplicableTo(Scale::Major));
    CHECK(it->isApplicableTo(Scale::Dorian));
    CHECK_FALSE(it->isApplicableTo(Scale::MinorBlues));

    file.deleteFile();
}

TEST_CASE("ProgressionPresetLibrary deletePreset removes a user preset but is a no-op for built-in ids", "[ProgressionPreset]")
{
    const auto file = makeTempUserPresetsFile();
    ProgressionPresetLibrary library(file);

    const auto preset = ProgressionPresetFactory::createFromSlots("Temp", { { Degree::I, 0 } }, Scale::Major);
    library.savePreset(preset);
    REQUIRE(library.getUserPresets().size() == 1);

    library.deletePreset("twelve_bar_blues"); // built-in id, must be a no-op
    CHECK(library.getUserPresets().size() == 1);

    library.deletePreset(preset.id);
    CHECK(library.getUserPresets().empty());

    file.deleteFile();
}

TEST_CASE("ProgressionPresetLibrary round-trips a pinned popularityOrder through a temp properties file", "[ProgressionPreset]")
{
    const auto file = makeTempUserPresetsFile();

    const auto preset = ProgressionPresetFactory::createFromSlots("Pinned Progression",
        { { Degree::I, 2 }, { Degree::V, 0 } }, Scale::Major);

    ProgressionPresetLibrary library(file);
    library.savePreset(preset);

    // A second instance over the same file, matching separate plugin instances/sessions.
    ProgressionPresetLibrary reloaded(file);
    const auto reloadedPresets = reloaded.getUserPresets();
    const auto it = std::find_if(reloadedPresets.begin(), reloadedPresets.end(),
        [&preset](const theory::ProgressionPreset& p) { return p.id == preset.id; });

    REQUIRE(it != reloadedPresets.end());
    REQUIRE(it->slots.size() == 2);
    CHECK(it->slots[0].degree == Degree::I);
    CHECK(it->slots[0].popularityOrder == 2);
    CHECK(it->slots[1].degree == Degree::V);
    CHECK(it->slots[1].popularityOrder == 0);

    file.deleteFile();
}

TEST_CASE("ProgressionPresetLibrary loads old-format XML (no popularityOrder attribute) as unpinned slots", "[ProgressionPreset]")
{
    // A literal juce::PropertiesFile-shaped XML fixture predating the popularityOrder attribute -
    // confirms additive-attribute backward compatibility for real, not just by inspection.
    const juce::String oldFormatXml =
        "<PROPERTIES>\n"
        "  <VALUE name=\"userProgressionPresets\">\n"
        "    <UserProgressionPresets>\n"
        "      <Preset id=\"user_old1\" displayName=\"Old Preset\" applicableScales=\"\">\n"
        "        <Slot degree=\"I\"/>\n"
        "        <Slot degree=\"V\"/>\n"
        "      </Preset>\n"
        "    </UserProgressionPresets>\n"
        "  </VALUE>\n"
        "</PROPERTIES>\n";

    const auto file = makeTempUserPresetsFile();
    REQUIRE(file.replaceWithText(oldFormatXml));

    ProgressionPresetLibrary library(file);
    const auto presets = library.getUserPresets();

    const auto it = std::find_if(presets.begin(), presets.end(),
        [](const theory::ProgressionPreset& p) { return p.id == "user_old1"; });

    REQUIRE(it != presets.end());
    REQUIRE(it->slots.size() == 2);
    CHECK(it->slots[0].degree == Degree::I);
    CHECK(it->slots[0].popularityOrder == 0);
    CHECK(it->slots[1].degree == Degree::V);
    CHECK(it->slots[1].popularityOrder == 0);

    file.deleteFile();
}
