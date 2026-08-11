#include "Component/ProgressionDragHandle.h"

#include <algorithm>

#include "AppLocalisation.h"

namespace component
{

namespace
{
    constexpr float kDragStartThreshold = 6.f;
    constexpr float kIconSize = 12.f;
    constexpr float kIconLeftOffset = 16.f;
}

ProgressionDragHandle::ProgressionDragHandle(const std::string& identifier):
    Component(identifier),
    _icon(identifier + "-icon", nui::Icons::getHandle()),
    _label(identifier + "-label", "", juce::translate("progression_drag_handle_label").toStdString())
{
    _icon.setInterceptsMouseClicks(false, false); // decorative - clicks/drags must reach the handle
    _icon.setIconSize(kIconSize);

    _label.setFontSize(nui::Theme::PARAGRAPH);
    _label.setJustificationType(juce::Justification::centred);
    _label.setInterceptsMouseClicks(false, false); // decorative - clicks/drags must reach the handle

    displayBorder(nui::Theme::ThemeColor::ACCENT, 1.f, nui::Theme::getBorderRadius());
    displayBackground(nui::Theme::newColor(nui::Theme::ThemeColor::ACCENT).asJuce().withAlpha(.2f), nui::Theme::getBorderRadius());

    setMouseCursor(juce::MouseCursor::DraggingHandCursor);

    addAndMakeVisible(_icon);
    addAndMakeVisible(_label);

    AppLocalisation::getChangeBroadcaster().addChangeListener(this);
}

ProgressionDragHandle::~ProgressionDragHandle()
{
    AppLocalisation::getChangeBroadcaster().removeChangeListener(this);
}

void ProgressionDragHandle::paint(juce::Graphics& g)
{
    Component::paint(g);
}

void ProgressionDragHandle::resized()
{
    Component::resized();

    const auto bounds = getLocalBounds().toFloat();

    _icon.setBounds(juce::Rectangle<float>(kIconLeftOffset, bounds.getCentreY() - kIconSize * 0.5f, kIconSize, kIconSize).toNearestInt());

    // Text fills whatever's left after the icon's own fixed offset/width, centred within that
    // remaining space (rather than the icon+label being centred together as one unit).
    _label.setBounds(bounds.withTrimmedLeft(kIconLeftOffset + kIconSize).toNearestInt());
}

void ProgressionDragHandle::mouseDown(const juce::MouseEvent&)
{
    _dragGestureStarted = false;
}

void ProgressionDragHandle::mouseDrag(const juce::MouseEvent& event)
{
    if (_dragGestureStarted)
        return;

    if (static_cast<float>(event.getDistanceFromDragStart()) < kDragStartThreshold)
        return;

    _dragGestureStarted = true;

    for (auto* listener : _listeners)
        listener->onProgressionDragStarted();
}

void ProgressionDragHandle::addListener(Listener* listener)
{
    _listeners.push_back(listener);
}

void ProgressionDragHandle::removeListener(Listener* listener)
{
    _listeners.erase(std::remove(_listeners.begin(), _listeners.end(), listener), _listeners.end());
}

void ProgressionDragHandle::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    Component::changeListenerCallback(source);

    if (source != &AppLocalisation::getChangeBroadcaster())
        return;

    _label.setText(juce::translate("progression_drag_handle_label").toStdString());
    repaint();
}

}
