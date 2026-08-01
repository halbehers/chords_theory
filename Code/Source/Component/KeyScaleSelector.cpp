#include "Component/KeyScaleSelector.h"

#include <algorithm>

#include "AppLocalisation.h"

namespace component
{

KeyScaleSelector::KeyScaleSelector(const std::string& identifier):
    Component(identifier)
{
    for (int i = 0; i < theory::kNumKeys; ++i)
        _keyPicker.addItem(theory::getKeyLabel(static_cast<theory::Key>(i)), i + 1);

    for (int i = 0; i < theory::kNumScales; ++i)
        _scalePicker.addItem(juce::translate(theory::getScaleTranslationKey(static_cast<theory::Scale>(i))), i + 1);

    _keyPicker.setSelectedId(static_cast<int>(_currentKey) + 1, juce::dontSendNotification);
    _scalePicker.setSelectedId(static_cast<int>(_currentScale) + 1, juce::dontSendNotification);

    _keyPicker.addOnValueChangedListener(this);
    _scalePicker.addOnValueChangedListener(this);

    _keyPicker.setHeightType(nui::Theme::HeightType::THIN);
    _scalePicker.setHeightType(nui::Theme::HeightType::THIN);

    AppLocalisation::getChangeBroadcaster().addChangeListener(this);

    _layout.setGap(8.f);
    _layout.setDisplayGrid(false);
    _layout.init({ 1 }, { 1, 1 });
    _layout.addComponent(_keyPicker, 0, 0, 1, 1);
    _layout.addComponent(_scalePicker, 0, 1, 1, 1);
}

KeyScaleSelector::~KeyScaleSelector()
{
    _keyPicker.removeListener(this);
    _scalePicker.removeListener(this);
    AppLocalisation::getChangeBroadcaster().removeChangeListener(this);
}

void KeyScaleSelector::paint(juce::Graphics& g)
{
    Component::paint(g);

    _layout.paint(g);
}

void KeyScaleSelector::resized()
{
    Component::resized();

    _layout.resized();
}

void KeyScaleSelector::setKeyAndScale(theory::Key key, theory::Scale scale)
{
    _currentKey = key;
    _currentScale = scale;

    _keyPicker.setSelectedId(static_cast<int>(_currentKey) + 1, juce::dontSendNotification);
    _scalePicker.setSelectedId(static_cast<int>(_currentScale) + 1, juce::dontSendNotification);
}

void KeyScaleSelector::addListener(Listener* listener)
{
    _listeners.push_back(listener);
}

void KeyScaleSelector::removeListener(Listener* listener)
{
    _listeners.erase(std::remove(_listeners.begin(), _listeners.end(), listener), _listeners.end());
}

void KeyScaleSelector::onSelectionChanged(const std::string& componentID, int selectedId)
{
    juce::ignoreUnused(componentID, selectedId);

    _currentKey = static_cast<theory::Key>(_keyPicker.getSelectedId() - 1);
    _currentScale = static_cast<theory::Scale>(_scalePicker.getSelectedId() - 1);

    notifyListeners();
}

void KeyScaleSelector::notifyListeners()
{
    for (auto* listener : _listeners)
        listener->onKeyScaleChanged(_currentKey, _currentScale);
}

void KeyScaleSelector::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    Component::changeListenerCallback(source);

    if (source != &AppLocalisation::getChangeBroadcaster())
        return;

    const auto selectedScale = _currentScale;

    _scalePicker.clear(juce::dontSendNotification);
    for (int i = 0; i < theory::kNumScales; ++i)
        _scalePicker.addItem(juce::translate(theory::getScaleTranslationKey(static_cast<theory::Scale>(i))), i + 1);
    _scalePicker.setSelectedId(static_cast<int>(selectedScale) + 1, juce::dontSendNotification);

    repaint();
}

}
