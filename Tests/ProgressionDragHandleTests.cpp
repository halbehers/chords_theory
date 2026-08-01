#include <catch2/catch_test_macros.hpp>

#include "Component/ProgressionDragHandle.h"

using component::ProgressionDragHandle;

namespace
{
    juce::MouseEvent makeMouseEvent(juce::Component& component, juce::Point<float> position,
        juce::Point<float> mouseDownPos)
    {
        return juce::MouseEvent(
            juce::Desktop::getInstance().getMainMouseSource(),
            position,
            juce::ModifierKeys(),
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            &component, &component,
            juce::Time::getCurrentTime(),
            mouseDownPos,
            juce::Time::getCurrentTime(),
            1,
            position != mouseDownPos);
    }

    struct RecordingListener : public ProgressionDragHandle::Listener
    {
        int dragStartedCount = 0;

        void onProgressionDragStarted() override { ++dragStartedCount; }
    };
}

TEST_CASE("ProgressionDragHandle's clickable area is the handle itself, not its decorative label", "[ProgressionDragHandle]")
{
    // Same regression as ChordCard/ProgressionSlotView - see the
    // setInterceptsMouseClicks(false, false) fix on _label in ProgressionDragHandle's constructor.
    ProgressionDragHandle handle("test-drag-handle");
    handle.setBounds(0, 0, 300, 40);
    handle.setVisible(true); // getComponentAt() hit-tests only visible components

    CHECK(handle.getComponentAt(150, 20) == &handle);
}

TEST_CASE("ProgressionDragHandle: dragging past the threshold fires onProgressionDragStarted", "[ProgressionDragHandle]")
{
    ProgressionDragHandle handle("test-drag-handle");
    handle.setBounds(0, 0, 300, 40);

    RecordingListener listener;
    handle.addListener(&listener);

    const juce::Point<float> startPos { 10.0f, 20.0f };
    const juce::Point<float> draggedPos { 40.0f, 20.0f }; // 30px away, past the 6px threshold

    handle.mouseDown(makeMouseEvent(handle, startPos, startPos));
    handle.mouseDrag(makeMouseEvent(handle, draggedPos, startPos));

    CHECK(listener.dragStartedCount == 1);

    handle.removeListener(&listener);
}
