#include <catch2/catch_test_macros.hpp>

#include "Component/ChordDegreeBrowser.h"
#include "Component/ProgressionSequencer.h"
#include "Theory/ChordDatabase.h"
#include "Theory/ProgressionPresetFactory.h"
#include "Theory/ProgressionPresetLibrary.h"

using component::ChordDegreeBrowser;
using component::ProgressionSequencer;
using theory::ChordDatabase;
using theory::Degree;
using theory::Key;
using theory::ProgressionPresetFactory;
using theory::ProgressionPresetLibrary;
using theory::Scale;

TEST_CASE("INTEGRATION: end-to-end, exactly as AppLayout wires it - pick non-default voicing, drag into slot, save, reload", "[Integration]")
{
    ChordDegreeBrowser browser("test-browser");
    browser.setKeyAndScale(Key::C, Scale::Major);

    ProgressionSequencer sequencer("test-sequencer",
        [&browser](const theory::ProgressionSlot& slot) { return browser.resolveSlot(slot); });
    sequencer.setScale(Scale::Major);

    // Step 1: user picks a non-default voicing for degree I via the voicing selector.
    const auto& cDegreeIData = ChordDatabase::getInstance().get(Key::C, Scale::Major).degrees.front();
    REQUIRE(cDegreeIData.chords.size() > 1);
    const auto& pickedChord = cDegreeIData.chords[1]; // not the default
    browser.selectVoicing(Degree::I, pickedChord);

    CAPTURE(pickedChord.symbol);
    const auto* liveNow = browser.getCurrentChord(Degree::I);
    REQUIRE(liveNow != nullptr);
    REQUIRE(liveNow->symbol == pickedChord.symbol);

    // Step 2: user drags that ChordCard into slot 0 (degree-only, exactly like AppLayout::onSlotFileDropped).
    sequencer.setSlotDegree(0, Degree::I);

    // Step 3: user clicks "Save Preset" - exactly ProgressionSequencer::onButtonClick's body.
    const auto populatedSlots = sequencer.getPopulatedSlots();
    REQUIRE(populatedSlots.size() == 1);
    CAPTURE(populatedSlots.front().popularityOrder);
    REQUIRE(populatedSlots.front().popularityOrder == pickedChord.popularityOrder); // <-- the crux of the whole feature

    const auto preset = ProgressionPresetFactory::createFromSlots("Integration Test", populatedSlots, Scale::Major);
    REQUIRE(preset.slots.front().popularityOrder == pickedChord.popularityOrder);

    // Step 4: persist to a real temp file and reload through a fresh library instance.
    auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("IntegrationCheck_" + juce::Uuid().toString() + ".xml");
    file.deleteFile();

    {
        ProgressionPresetLibrary library(file);
        library.savePreset(preset);
    }

    ProgressionPresetLibrary reloaded(file);
    const auto& userPresets = reloaded.getUserPresets();
    REQUIRE(userPresets.size() == 1);
    CAPTURE(userPresets.front().slots.front().popularityOrder);
    CHECK(userPresets.front().slots.front().popularityOrder == pickedChord.popularityOrder);

    // Step 5: the user's actual reported repro - change the KEY (not just re-pick a degree's
    // voicing within the same key), then load the reloaded preset back into a fresh sequencer and
    // confirm it shows the voicing at the SAME relative position, not the new key's plain default.
    // (A symbol-based pin would fail here: "Cmaj7" doesn't exist anywhere in D Major's chord
    // lists, since every symbol is spelled with its own key's letter names.)
    browser.setKeyAndScale(Key::D, Scale::Major);

    const auto& dDegreeIData = ChordDatabase::getInstance().get(Key::D, Scale::Major).degrees.front();
    const auto& expectedInD = dDegreeIData.chords[static_cast<std::size_t>(pickedChord.popularityOrder - 1)];
    REQUIRE(expectedInD.symbol != dDegreeIData.chords.front().symbol); // sanity: still not the plain default

    ProgressionSequencer sequencer2("test-sequencer-2",
        [&browser](const theory::ProgressionSlot& slot) { return browser.resolveSlot(slot); });
    sequencer2.loadPreset(userPresets.front());

    const auto reloadedSlot0Degree = sequencer2.getSlotDegree(0);
    REQUIRE(reloadedSlot0Degree.has_value());
    CHECK(*reloadedSlot0Degree == Degree::I);

    const auto* resolvedAfterKeyChange = browser.resolveSlot(userPresets.front().slots.front());
    REQUIRE(resolvedAfterKeyChange != nullptr);
    CHECK(resolvedAfterKeyChange->symbol == expectedInD.symbol);
    CHECK(resolvedAfterKeyChange->symbol != dDegreeIData.chords.front().symbol);

    file.deleteFile();
}
