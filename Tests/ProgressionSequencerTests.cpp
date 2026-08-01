#include <catch2/catch_test_macros.hpp>

#include <string>

#include "Component/ProgressionSequencer.h"
#include "Theory/ProgressionPreset.h"

using component::ProgressionSequencer;
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

    struct RecordingListener : public ProgressionSequencer::Listener
    {
        int slotsChangedCount = 0;
        int fileDroppedCount = 0;
        int lastFileDroppedSlotIndex = -1;

        void onSlotFileDropped(int slotIndex, const juce::String&) override
        {
            ++fileDroppedCount;
            lastFileDroppedSlotIndex = slotIndex;
        }
        void onProgressionDragStarted() override {}
        void onSlotsChanged() override { ++slotsChangedCount; }
        void onChordPreviewRequested(const theory::Chord&) override {}
    };

    // Both nelement::SVGButton and nelement::TextButton wrap a single, always-first, internal
    // juce::Button child that actually receives clicks - triggerClick() drives their real
    // onClick handler without needing real mouse events or exact on-screen geometry.
    // juce::Button::triggerClick() only posts an async command message rather than invoking
    // onClick synchronously (and handleCommandMessage() re-checks isEnabled() before firing), so
    // the dispatch loop must be pumped briefly afterward - same pattern as VoicingSelectorTests.cpp.
    void triggerButtonClick(juce::Component& buttonWrapper)
    {
        auto* button = dynamic_cast<juce::Button*>(buttonWrapper.getChildComponent(0));
        REQUIRE(button != nullptr);
        button->triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
    }

    // _slotsViewport is a direct child of the sequencer (found via its componentID), and its
    // viewed component *is* the SlotsRow that hosts every ProgressionSlotView plus the +/- buttons.
    juce::Component* getSlotsRow(ProgressionSequencer& sequencer)
    {
        auto* viewport = dynamic_cast<juce::Viewport*>(sequencer.findChildWithID("progression-slots-viewport"));
        return viewport != nullptr ? viewport->getViewedComponent() : nullptr;
    }
}

TEST_CASE("ProgressionSequencer::getPopulatedSlots bakes the resolver's currently-resolved popularityOrder into each slot", "[ProgressionSequencer]")
{
    const Chord currentForI = makeChord("Cmaj7", 2);

    ProgressionSequencer sequencer("test-sequencer",
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

TEST_CASE("ProgressionSequencer: a slot loaded via loadPreset with a pinned popularityOrder round-trips through getPopulatedSlots", "[ProgressionSequencer]")
{
    const Chord pinnedChord = makeChord("Dm7", 2);

    ProgressionSequencer sequencer("test-sequencer",
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

TEST_CASE("ProgressionSequencer::setSlotDegree always produces an unpinned slot, even overwriting a previously-pinned one", "[ProgressionSequencer]")
{
    ProgressionSequencer sequencer("test-sequencer",
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

TEST_CASE("ProgressionSequencer::removeSlotAndShift removes the slot and shifts every later slot back by one", "[ProgressionSequencer]")
{
    const Chord dummy = makeChord("X", 1);
    ProgressionSequencer sequencer("test-sequencer",
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

TEST_CASE("ProgressionSequencer::removeSlotAndShift on an already-empty slot is a no-op", "[ProgressionSequencer]")
{
    const Chord dummy = makeChord("X", 1);
    ProgressionSequencer sequencer("test-sequencer",
        [&dummy](const ProgressionSlot&) -> const Chord* { return &dummy; });

    sequencer.setSlotDegree(0, Degree::I);

    sequencer.removeSlotAndShift(5); // slot 5 was never populated

    REQUIRE(sequencer.getSlotDegree(0).has_value());
    CHECK(*sequencer.getSlotDegree(0) == Degree::I);
    CHECK(sequencer.getPopulatedSlots().size() == 1);
}

TEST_CASE("ProgressionSequencer::removeSlotAndShift out of range is a safe no-op", "[ProgressionSequencer]")
{
    const Chord dummy = makeChord("X", 1);
    ProgressionSequencer sequencer("test-sequencer",
        [&dummy](const ProgressionSlot&) -> const Chord* { return &dummy; });

    sequencer.setSlotDegree(0, Degree::I);

    REQUIRE_NOTHROW(sequencer.removeSlotAndShift(-1));
    REQUIRE_NOTHROW(sequencer.removeSlotAndShift(999));

    CHECK(sequencer.getPopulatedSlots().size() == 1);
}

TEST_CASE("ProgressionSequencer: starts with the default step count", "[ProgressionSequencer]")
{
    ProgressionSequencer sequencer("test-sequencer",
        [](const ProgressionSlot&) -> const Chord* { return nullptr; });

    CHECK(sequencer.getSlotCount() == 4);
}

TEST_CASE("ProgressionSequencer: clicking the add-step button grows the step count by one, unoccupied", "[ProgressionSequencer]")
{
    ProgressionSequencer sequencer("test-sequencer",
        [](const ProgressionSlot&) -> const Chord* { return nullptr; });
    sequencer.setBounds(0, 0, 800, 200);

    auto* slotsRow = getSlotsRow(sequencer);
    REQUIRE(slotsRow != nullptr);
    auto* addButton = slotsRow->findChildWithID("progression-add-step");
    REQUIRE(addButton != nullptr);

    triggerButtonClick(*addButton);

    CHECK(sequencer.getSlotCount() == 5);
    CHECK_FALSE(sequencer.getSlotDegree(4).has_value());
}

TEST_CASE("ProgressionSequencer: clicking the remove-step button repeatedly shrinks down to the minimum and then stops", "[ProgressionSequencer]")
{
    ProgressionSequencer sequencer("test-sequencer",
        [](const ProgressionSlot&) -> const Chord* { return nullptr; });
    sequencer.setBounds(0, 0, 800, 200);

    auto* slotsRow = getSlotsRow(sequencer);
    REQUIRE(slotsRow != nullptr);
    auto* removeButton = slotsRow->findChildWithID("progression-remove-step");
    REQUIRE(removeButton != nullptr);

    for (int i = 0; i < 10; ++i)
        triggerButtonClick(*removeButton);

    CHECK(sequencer.getSlotCount() == 1); // kMinSlotCount - further clicks are no-ops (button disables)
}

TEST_CASE("ProgressionSequencer: clicking remove-step notifies listeners only when the removed trailing slot was occupied", "[ProgressionSequencer]")
{
    const Chord dummy = makeChord("X", 1);
    ProgressionSequencer sequencer("test-sequencer",
        [&dummy](const ProgressionSlot&) -> const Chord* { return &dummy; });
    sequencer.setBounds(0, 0, 800, 200);

    RecordingListener listener;
    sequencer.addListener(&listener);

    auto* slotsRow = getSlotsRow(sequencer);
    REQUIRE(slotsRow != nullptr);
    auto* removeButton = slotsRow->findChildWithID("progression-remove-step");
    REQUIRE(removeButton != nullptr);

    // Default 4 slots, all unoccupied - removing the trailing (empty) slot fires nothing.
    triggerButtonClick(*removeButton);
    CHECK(listener.slotsChangedCount == 0);
    CHECK(sequencer.getSlotCount() == 3);

    sequencer.setSlotDegree(2, Degree::I); // occupies the new trailing slot
    listener.slotsChangedCount = 0; // setSlotDegree itself notifies - reset before the click under test

    triggerButtonClick(*removeButton);
    CHECK(listener.slotsChangedCount == 1);
    CHECK(sequencer.getSlotCount() == 2);

    sequencer.removeListener(&listener);
}

TEST_CASE("ProgressionSequencer::setSlotDegree auto-grows the progression when given an out-of-range index", "[ProgressionSequencer]")
{
    ProgressionSequencer sequencer("test-sequencer",
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

TEST_CASE("ProgressionSequencer::loadPreset grows the step count to fit a longer preset", "[ProgressionSequencer]")
{
    const Chord dummy = makeChord("X", 1);
    ProgressionSequencer sequencer("test-sequencer",
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

TEST_CASE("ProgressionSequencer::loadPreset shrinks the step count to fit a shorter preset after previously growing", "[ProgressionSequencer]")
{
    const Chord dummy = makeChord("X", 1);
    ProgressionSequencer sequencer("test-sequencer",
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

TEST_CASE("ProgressionSequencer::loadPreset with an empty slots list clears down to the minimum step count", "[ProgressionSequencer]")
{
    const Chord dummy = makeChord("X", 1);
    ProgressionSequencer sequencer("test-sequencer",
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

TEST_CASE("ProgressionSequencer: dropping a file on the slots row appends a new step when every slot is occupied", "[ProgressionSequencer]")
{
    const Chord dummy = makeChord("X", 1);
    ProgressionSequencer sequencer("test-sequencer",
        [&dummy](const ProgressionSlot&) -> const Chord* { return &dummy; });
    sequencer.setBounds(0, 0, 800, 200);

    for (int i = 0; i < sequencer.getSlotCount(); ++i)
        sequencer.setSlotDegree(i, Degree::I); // occupy every default slot
    REQUIRE(sequencer.getSlotCount() == 4);

    RecordingListener listener;
    sequencer.addListener(&listener);

    auto* slotsRow = getSlotsRow(sequencer);
    REQUIRE(slotsRow != nullptr);
    auto* dropTarget = dynamic_cast<juce::FileDragAndDropTarget*>(slotsRow);
    REQUIRE(dropTarget != nullptr);

    juce::StringArray files;
    files.add("/tmp/test-chord.mid");
    dropTarget->filesDropped(files, 0, 0);

    CHECK(sequencer.getSlotCount() == 5);
    CHECK(listener.fileDroppedCount == 1);
    CHECK(listener.lastFileDroppedSlotIndex == 4); // the newly-appended step

    sequencer.removeListener(&listener);
}

TEST_CASE("ProgressionSequencer: dropping a file on the slots row lands on the first available empty slot", "[ProgressionSequencer]")
{
    const Chord dummy = makeChord("X", 1);
    ProgressionSequencer sequencer("test-sequencer",
        [&dummy](const ProgressionSlot&) -> const Chord* { return &dummy; });
    sequencer.setBounds(0, 0, 800, 200);

    sequencer.setSlotDegree(0, Degree::I);
    sequencer.setSlotDegree(1, Degree::II);
    // slots 2 and 3 stay empty (default count is 4)

    RecordingListener listener;
    sequencer.addListener(&listener);

    auto* slotsRow = getSlotsRow(sequencer);
    REQUIRE(slotsRow != nullptr);
    auto* dropTarget = dynamic_cast<juce::FileDragAndDropTarget*>(slotsRow);
    REQUIRE(dropTarget != nullptr);

    juce::StringArray files;
    files.add("/tmp/test-chord.mid");
    dropTarget->filesDropped(files, 0, 0);

    CHECK(sequencer.getSlotCount() == 4); // no growth needed
    CHECK(listener.fileDroppedCount == 1);
    CHECK(listener.lastFileDroppedSlotIndex == 2); // first empty slot

    sequencer.removeListener(&listener);
}
