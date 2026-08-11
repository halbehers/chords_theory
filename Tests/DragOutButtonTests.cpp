#include <catch2/catch_test_macros.hpp>

#include "Component/DragOutButton.h"

using component::DragOutButton;

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

    struct RecordingListener : public DragOutButton::Listener
    {
        int dragStartedCount = 0;

        void onDragStarted() override { ++dragStartedCount; }
    };
}

TEST_CASE("DragOutButton: dragging past the threshold fires onDragStarted", "[DragOutButton]")
{
    DragOutButton button("test-drag-out-button");
    button.setBounds(0, 0, 32, 32);

    RecordingListener listener;
    button.addListener(&listener);

    const juce::Point<float> startPos { 10.0f, 16.0f };
    const juce::Point<float> draggedPos { 30.0f, 16.0f }; // 20px away, past the 6px threshold

    button.mouseDown(makeMouseEvent(button, startPos, startPos));
    button.mouseDrag(makeMouseEvent(button, draggedPos, startPos));

    CHECK(listener.dragStartedCount == 1);

    button.removeListener(&listener);
}

TEST_CASE("DragOutButton: a plain click (no real movement) does not fire onDragStarted", "[DragOutButton]")
{
    DragOutButton button("test-drag-out-button");
    button.setBounds(0, 0, 32, 32);

    RecordingListener listener;
    button.addListener(&listener);

    const juce::Point<float> pos { 16.0f, 16.0f };

    button.mouseDown(makeMouseEvent(button, pos, pos));
    button.mouseDrag(makeMouseEvent(button, pos, pos)); // same position - below the drag threshold

    CHECK(listener.dragStartedCount == 0);

    button.removeListener(&listener);
}

TEST_CASE("DragOutButton: onDragStarted fires at most once per gesture", "[DragOutButton]")
{
    DragOutButton button("test-drag-out-button");
    button.setBounds(0, 0, 32, 32);

    RecordingListener listener;
    button.addListener(&listener);

    const juce::Point<float> startPos { 10.0f, 16.0f };
    const juce::Point<float> draggedPos { 30.0f, 16.0f };
    const juce::Point<float> draggedFurtherPos { 60.0f, 16.0f };

    button.mouseDown(makeMouseEvent(button, startPos, startPos));
    button.mouseDrag(makeMouseEvent(button, draggedPos, startPos));
    button.mouseDrag(makeMouseEvent(button, draggedFurtherPos, startPos));

    CHECK(listener.dragStartedCount == 1);

    button.removeListener(&listener);
}
