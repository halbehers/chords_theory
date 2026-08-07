#include <catch2/catch_test_macros.hpp>

#include <string>

#include "Component/ProgressionEditor.h"
#include "Theory/ProgressionPreset.h"

using component::ProgressionEditor;
using theory::Chord;
using theory::Degree;
using theory::ProgressionPreset;
using theory::ProgressionSlot;

namespace
{
    Chord makeChord(const std::string& symbol, int popularityOrder)
    {
        Chord chord;
        chord.symbol = symbol;
        chord.readableName = symbol;
        chord.popularityOrder = popularityOrder;
        return chord;
    }

    struct RecordingListener : public ProgressionEditor::Listener
    {
        int slotsChangedCount = 0;
        int fileDroppedCount = 0;
        double lastFileDroppedBeat = -1.0;

        void onChordFileDropped(double startBeat, const juce::String&) override
        {
            ++fileDroppedCount;
            lastFileDroppedBeat = startBeat;
        }
        void onProgressionDragStarted() override {}
        void onSlotsChanged() override { ++slotsChangedCount; }
    };
}

TEST_CASE("ProgressionEditor::getPopulatedSlots bakes the resolver's currently-resolved popularityOrder into each slot", "[ProgressionEditor]")
{
    const Chord currentForI = makeChord("Cmaj7", 2);

    ProgressionEditor sequencer("test-sequencer",
        [&currentForI](const ProgressionSlot& slot) -> const Chord*
        {
            return slot.degree == Degree::I ? &currentForI : nullptr;
        });

    sequencer.setSlotDegree(0, Degree::I); // unpinned - resolves live via the resolver above

    const auto populated = sequencer.getPopulatedSlots();
    REQUIRE(populated.size() == 1);
    CHECK(populated.front().degree == Degree::I);
    CHECK(populated.front().popularityOrder == 2);
}

TEST_CASE("ProgressionEditor: a slot loaded via loadPreset with a pinned popularityOrder round-trips through getPopulatedSlots", "[ProgressionEditor]")
{
    const Chord pinnedChord = makeChord("Dm7", 2);

    ProgressionEditor sequencer("test-sequencer",
        [&pinnedChord](const ProgressionSlot& slot) -> const Chord*
        {
            // Only answers the exact pinned popularityOrder - anything else (e.g. an
            // accidentally-dropped pin) resolves to nullptr, so the test fails loudly rather than
            // silently passing.
            return slot.popularityOrder == 2 ? &pinnedChord : nullptr;
        });

    ProgressionPreset preset;
    preset.id = "test-preset";
    preset.displayName = "Test";
    preset.slots = { ProgressionSlot { Degree::II, 2 } };

    sequencer.loadPreset(preset);

    const auto populated = sequencer.getPopulatedSlots();
    REQUIRE(populated.size() == 1);
    CHECK(populated.front().degree == Degree::II);
    CHECK(populated.front().popularityOrder == 2);
}

TEST_CASE("ProgressionEditor::setSlotDegree always produces an unpinned slot, even overwriting a previously-pinned one", "[ProgressionEditor]")
{
    ProgressionEditor sequencer("test-sequencer",
        [](const ProgressionSlot&) -> const Chord* { return nullptr; });

    ProgressionPreset preset;
    preset.id = "test-preset";
    preset.slots = { ProgressionSlot { Degree::I, 2 } };
    sequencer.loadPreset(preset);

    REQUIRE(sequencer.getSlotDegree(0).has_value());

    sequencer.setSlotDegree(0, Degree::V); // drag-and-drop style overwrite - must clear any pin

    const auto populated = sequencer.getPopulatedSlots();
    REQUIRE(populated.size() == 1);
    CHECK(populated.front().degree == Degree::V);
    CHECK(populated.front().popularityOrder == 0);
}

TEST_CASE("ProgressionEditor::removeSlotAndShift removes the slot and shifts every later slot back by one", "[ProgressionEditor]")
{
    const Chord dummy = makeChord("X", 1);
    ProgressionEditor sequencer("test-sequencer",
        [&dummy](const ProgressionSlot&) -> const Chord* { return &dummy; });

    sequencer.setSlotDegree(0, Degree::I);
    sequencer.setSlotDegree(1, Degree::II);
    sequencer.setSlotDegree(2, Degree::III);
    sequencer.setSlotDegree(3, Degree::IV);

    sequencer.removeSlotAndShift(1); // remove II - III/IV should shift down to indices 1/2

    REQUIRE(sequencer.getSlotDegree(0).has_value());
    CHECK(*sequencer.getSlotDegree(0) == Degree::I);
    REQUIRE(sequencer.getSlotDegree(1).has_value());
    CHECK(*sequencer.getSlotDegree(1) == Degree::III);
    REQUIRE(sequencer.getSlotDegree(2).has_value());
    CHECK(*sequencer.getSlotDegree(2) == Degree::IV);
    CHECK_FALSE(sequencer.getSlotDegree(3).has_value()); // no gap left behind - the old last slot is now empty

    const auto populated = sequencer.getPopulatedSlots();
    REQUIRE(populated.size() == 3);
    CHECK(populated[0].degree == Degree::I);
    CHECK(populated[1].degree == Degree::III);
    CHECK(populated[2].degree == Degree::IV);
}

TEST_CASE("ProgressionEditor::removeSlotAndShift on an already-empty slot is a no-op", "[ProgressionEditor]")
{
    const Chord dummy = makeChord("X", 1);
    ProgressionEditor sequencer("test-sequencer",
        [&dummy](const ProgressionSlot&) -> const Chord* { return &dummy; });

    sequencer.setSlotDegree(0, Degree::I);

    sequencer.removeSlotAndShift(5); // slot 5 was never populated

    REQUIRE(sequencer.getSlotDegree(0).has_value());
    CHECK(*sequencer.getSlotDegree(0) == Degree::I);
    CHECK(sequencer.getPopulatedSlots().size() == 1);
}

TEST_CASE("ProgressionEditor::removeSlotAndShift out of range is a safe no-op", "[ProgressionEditor]")
{
    const Chord dummy = makeChord("X", 1);
    ProgressionEditor sequencer("test-sequencer",
        [&dummy](const ProgressionSlot&) -> const Chord* { return &dummy; });

    sequencer.setSlotDegree(0, Degree::I);

    REQUIRE_NOTHROW(sequencer.removeSlotAndShift(-1));
    REQUIRE_NOTHROW(sequencer.removeSlotAndShift(999));

    CHECK(sequencer.getPopulatedSlots().size() == 1);
}

TEST_CASE("ProgressionEditor: starts with the default step count", "[ProgressionEditor]")
{
    ProgressionEditor sequencer("test-sequencer",
        [](const ProgressionSlot&) -> const Chord* { return nullptr; });

    CHECK(sequencer.getSlotCount() == 4);
}

TEST_CASE("ProgressionEditor::setSlotDegree auto-grows the progression when given an out-of-range index", "[ProgressionEditor]")
{
    ProgressionEditor sequencer("test-sequencer",
        [](const ProgressionSlot&) -> const Chord* { return nullptr; });

    REQUIRE(sequencer.getSlotCount() == 4);

    sequencer.setSlotDegree(9, Degree::V); // well beyond the current 4-slot count

    CHECK(sequencer.getSlotCount() == 10);
    REQUIRE(sequencer.getSlotDegree(9).has_value());
    CHECK(*sequencer.getSlotDegree(9) == Degree::V);

    // Every intermediate slot created along the way stays unoccupied - only index 9 was written.
    for (int i = 4; i < 9; ++i)
        CHECK_FALSE(sequencer.getSlotDegree(i).has_value());
}

TEST_CASE("ProgressionEditor::loadPreset grows the step count to fit a longer preset", "[ProgressionEditor]")
{
    const Chord dummy = makeChord("X", 1);
    ProgressionEditor sequencer("test-sequencer",
        [&dummy](const ProgressionSlot&) -> const Chord* { return &dummy; });

    REQUIRE(sequencer.getSlotCount() == 4);

    ProgressionPreset preset;
    preset.id = "twelve-bar";
    preset.displayName = "Twelve Bar";
    for (int i = 0; i < 12; ++i)
        preset.slots.push_back(ProgressionSlot { Degree::I, 0 });

    sequencer.loadPreset(preset);

    CHECK(sequencer.getSlotCount() == 12);
    CHECK(sequencer.getPopulatedSlots().size() == 12);
}

TEST_CASE("ProgressionEditor::loadPreset shrinks the step count to fit a shorter preset after previously growing", "[ProgressionEditor]")
{
    const Chord dummy = makeChord("X", 1);
    ProgressionEditor sequencer("test-sequencer",
        [&dummy](const ProgressionSlot&) -> const Chord* { return &dummy; });

    ProgressionPreset longPreset;
    longPreset.id = "long";
    for (int i = 0; i < 8; ++i)
        longPreset.slots.push_back(ProgressionSlot { Degree::I, 0 });
    sequencer.loadPreset(longPreset);
    REQUIRE(sequencer.getSlotCount() == 8);

    ProgressionPreset shortPreset;
    shortPreset.id = "short";
    shortPreset.slots = { ProgressionSlot { Degree::V, 0 }, ProgressionSlot { Degree::IV, 0 } };
    sequencer.loadPreset(shortPreset);

    CHECK(sequencer.getSlotCount() == 2);
    REQUIRE(sequencer.getSlotDegree(0).has_value());
    CHECK(*sequencer.getSlotDegree(0) == Degree::V);
    REQUIRE(sequencer.getSlotDegree(1).has_value());
    CHECK(*sequencer.getSlotDegree(1) == Degree::IV);
}

TEST_CASE("ProgressionEditor::loadPreset with an empty slots list clears down to the minimum step count", "[ProgressionEditor]")
{
    const Chord dummy = makeChord("X", 1);
    ProgressionEditor sequencer("test-sequencer",
        [&dummy](const ProgressionSlot&) -> const Chord* { return &dummy; });

    sequencer.setSlotDegree(0, Degree::I);
    REQUIRE(sequencer.getSlotDegree(0).has_value());

    ProgressionPreset emptyPreset;
    emptyPreset.id = "empty";
    // Deliberately left with no slots.

    sequencer.loadPreset(emptyPreset);

    CHECK(sequencer.getSlotCount() == 1); // kMinSlotCount
    CHECK_FALSE(sequencer.getSlotDegree(0).has_value()); // regression: previously stale data survived here
}

TEST_CASE("ProgressionEditor: a chord file dropped on the MidiEditor bubbles up to this component's own listeners", "[ProgressionEditor]")
{
    ProgressionEditor sequencer("test-sequencer",
        [](const ProgressionSlot&) -> const Chord* { return nullptr; });
    sequencer.setBounds(0, 0, 800, 400);

    RecordingListener listener;
    sequencer.addListener(&listener);

    auto* midiEditor = sequencer.findChildWithID("progression-midi-editor");
    REQUIRE(midiEditor != nullptr);
    auto* dropTarget = dynamic_cast<juce::FileDragAndDropTarget*>(midiEditor);
    REQUIRE(dropTarget != nullptr);

    juce::StringArray files;
    files.add("/tmp/test-chord.mid");
    dropTarget->filesDropped(files, 100, 100);

    CHECK(listener.fileDroppedCount == 1);
    CHECK(listener.lastFileDroppedBeat >= 0.0);

    sequencer.removeListener(&listener);
}
