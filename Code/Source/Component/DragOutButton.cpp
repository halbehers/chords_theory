#include "Component/DragOutButton.h"

#include <algorithm>

namespace component
{

namespace
{
    // Same threshold ChordCard uses for its own click-vs-drag gesture.
    constexpr float kDragStartThreshold = 6.f;
    constexpr float kIconSize = 16.f;
}

DragOutButton::DragOutButton(const std::string& identifier):
    Component(identifier),
    _button(identifier + "-icon", nui::Icons::getHandle())
{
    _button.setIconSize(kIconSize);
    _button.setMouseCursor(juce::MouseCursor::DraggingHandCursor);

    // _button's own internal DrawableButton child is the actual clickable/draggable surface, not
    // _button itself and certainly not this wrapper - our own mouseDown/mouseDrag overrides below
    // only ever fire because they're registered here as a listener on a DIFFERENT object (_button).
    // Registering `this` on itself instead (self-registration) would double-dispatch every mouse
    // event landing directly on this component (Component IS-A MouseListener, so the same object
    // would receive both the normal virtual-override call AND a second call via the listener list) -
    // see MidiEditor.h's ChildBoundaryListener for the full writeup of that pitfall; registering on
    // a child, as done here, doesn't have that problem.
    _button.addMouseListener(this, true);

    addAndMakeVisible(_button);
}

DragOutButton::~DragOutButton()
{
    _button.removeMouseListener(this);
}

void DragOutButton::resized()
{
    Component::resized();

    _button.setBounds(getLocalBounds());
}

void DragOutButton::mouseDown(const juce::MouseEvent&)
{
    _dragGestureStarted = false;
}

void DragOutButton::mouseDrag(const juce::MouseEvent& event)
{
    if (_dragGestureStarted)
        return;

    if (static_cast<float>(event.getDistanceFromDragStart()) < kDragStartThreshold)
        return;

    _dragGestureStarted = true;

    for (auto* listener : _listeners)
        listener->onDragStarted();
}

void DragOutButton::addListener(Listener* listener)
{
    _listeners.push_back(listener);
}

void DragOutButton::removeListener(Listener* listener)
{
    _listeners.erase(std::remove(_listeners.begin(), _listeners.end(), listener), _listeners.end());
}

}
