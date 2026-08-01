#include <catch2/catch_test_macros.hpp>

#include "Component/ProgressionSlotView.h"
#include "Theory/ChordDatabase.h"

using component::ProgressionSlotView;
using theory::Chord;
using theory::ChordDatabase;
using theory::Degree;
using theory::Key;
using theory::Scale;

namespace
{
    juce::MouseEvent makeMouseEvent(juce::Component& component, juce::Point<float> position)
    {
        return juce::MouseEvent(
            juce::Desktop::getInstance().getMainMouseSource(),
            position,
            juce::ModifierKeys(),
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            &component, &component,
            juce::Time::getCurrentTime(),
            position,
            juce::Time::getCurrentTime(),
            1,
            false);
    }

    struct RecordingListener : public ProgressionSlotView::Listener
    {
        int fileDroppedCount = 0;
        int previewCount = 0;
        int lastPreviewSlotIndex = -1;
        int removeRequestedCount = 0;
        int lastRemoveRequestedSlotIndex = -1;

        void onFileDropped(int, const juce::String&) override { ++fileDroppedCount; }
        void onSlotPreviewRequested(int slotIndex) override
        {
            ++previewCount;
            lastPreviewSlotIndex = slotIndex;
        }
        void onSlotRemoveRequested(int slotIndex) override
        {
            ++removeRequestedCount;
            lastRemoveRequestedSlotIndex = slotIndex;
        }
    };

    const Chord& getTestChord()
    {
        return ChordDatabase::getInstance().get(Key::C, Scale::Major).degrees.front().chords.front();
    }
}

TEST_CASE("ProgressionSlotView's clickable area is the slot itself, not its decorative label", "[ProgressionSlotView]")
{
    // Same regression as ChordCard's - see setInterceptsMouseClicks(false, false) on _label in
    // ProgressionSlotView's constructor.
    ProgressionSlotView slot("test-slot", 0);
    slot.setBounds(0, 0, 80, 56);
    slot.setVisible(true); // getComponentAt() hit-tests only visible components

    CHECK(slot.getComponentAt(40, 28) == &slot);
}

TEST_CASE("ProgressionSlotView: clicking a populated slot fires onSlotPreviewRequested with its own index", "[ProgressionSlotView]")
{
    ProgressionSlotView slot("test-slot", 3);
    slot.setBounds(0, 0, 80, 56);
    slot.setChord(Degree::I, getTestChord());

    RecordingListener listener;
    slot.addListener(&listener);

    const juce::Point<float> pos { 40.0f, 28.0f };
    slot.mouseUp(makeMouseEvent(slot, pos));

    CHECK(listener.previewCount == 1);
    CHECK(listener.lastPreviewSlotIndex == 3);
    CHECK(listener.fileDroppedCount == 0);

    slot.removeListener(&listener);
}

TEST_CASE("ProgressionSlotView: clicking an empty slot fires nothing", "[ProgressionSlotView]")
{
    ProgressionSlotView slot("test-slot", 0);
    slot.setBounds(0, 0, 80, 56);
    // Deliberately left empty - never called setChord().

    RecordingListener listener;
    slot.addListener(&listener);

    const juce::Point<float> pos { 40.0f, 28.0f };
    slot.mouseUp(makeMouseEvent(slot, pos));

    CHECK(listener.previewCount == 0);

    slot.removeListener(&listener);
}

TEST_CASE("ProgressionSlotView: double-clicking a populated slot fires onSlotRemoveRequested with its own index", "[ProgressionSlotView]")
{
    ProgressionSlotView slot("test-slot", 5);
    slot.setBounds(0, 0, 80, 56);
    slot.setChord(Degree::I, getTestChord());

    RecordingListener listener;
    slot.addListener(&listener);

    const juce::Point<float> pos { 40.0f, 28.0f };
    slot.mouseDoubleClick(makeMouseEvent(slot, pos));

    CHECK(listener.removeRequestedCount == 1);
    CHECK(listener.lastRemoveRequestedSlotIndex == 5);

    slot.removeListener(&listener);
}

TEST_CASE("ProgressionSlotView: double-clicking an empty slot fires nothing", "[ProgressionSlotView]")
{
    ProgressionSlotView slot("test-slot", 0);
    slot.setBounds(0, 0, 80, 56);
    // Deliberately left empty - never called setChord().

    RecordingListener listener;
    slot.addListener(&listener);

    const juce::Point<float> pos { 40.0f, 28.0f };
    slot.mouseDoubleClick(makeMouseEvent(slot, pos));

    CHECK(listener.removeRequestedCount == 0);

    slot.removeListener(&listener);
}
