#include "Component/ProgressionPresetPicker.h"

#include <algorithm>

#include "Theory/ProgressionPresetLibrary.h"

namespace component
{

ProgressionPresetPicker::ProgressionPresetPicker(const std::string& identifier):
    Component(identifier)
{
    _picker.addOnValueChangedListener(this);
    _picker.setHeightType(nui::Theme::HeightType::THIN);

    _layout.setDisplayGrid(false);
    _layout.init({ 1 }, { 1 });
    _layout.addComponent(_picker, 0, 0, 1, 1);

    refreshForScale(_currentScale);
}

ProgressionPresetPicker::~ProgressionPresetPicker()
{
    _picker.removeListener(this);
}

void ProgressionPresetPicker::paint(juce::Graphics& g)
{
    Component::paint(g);

    _layout.paint(g);
}

void ProgressionPresetPicker::resized()
{
    Component::resized();

    _layout.resized();
}

void ProgressionPresetPicker::refreshForScale(theory::Scale scale)
{
    _currentScale = scale;

    const auto selectedIndex = _picker.getSelectedId() - 1;
    const std::string previouslySelectedId = (selectedIndex >= 0 && selectedIndex < static_cast<int>(_visiblePresets.size()))
        ? _visiblePresets[static_cast<std::size_t>(selectedIndex)].id
        : std::string();

    const auto allPresets = theory::ProgressionPresetLibrary::getInstance().getAllPresets();

    _visiblePresets.clear();
    for (const auto& preset : allPresets)
    {
        if (preset.isApplicableTo(scale))
            _visiblePresets.push_back(preset);
    }

    _picker.clear(juce::dontSendNotification);
    for (std::size_t i = 0; i < _visiblePresets.size(); ++i)
    {
        const auto& preset = _visiblePresets[i];
        const auto label = preset.isUserPreset() ? juce::String(preset.displayName) : juce::translate(preset.nameKey);
        _picker.addItem(label, static_cast<int>(i) + 1);
    }

    const auto restoredIt = std::find_if(_visiblePresets.begin(), _visiblePresets.end(),
        [&previouslySelectedId](const theory::ProgressionPreset& preset) { return preset.id == previouslySelectedId; });

    if (restoredIt != _visiblePresets.end())
        _picker.setSelectedId(static_cast<int>(std::distance(_visiblePresets.begin(), restoredIt)) + 1, juce::dontSendNotification);
}

void ProgressionPresetPicker::addListener(Listener* listener)
{
    _listeners.push_back(listener);
}

void ProgressionPresetPicker::removeListener(Listener* listener)
{
    _listeners.erase(std::remove(_listeners.begin(), _listeners.end(), listener), _listeners.end());
}

void ProgressionPresetPicker::onSelectionChanged(const std::string& componentID, int selectedId)
{
    juce::ignoreUnused(componentID);

    const auto index = selectedId - 1;
    if (index < 0 || index >= static_cast<int>(_visiblePresets.size()))
        return;

    const auto& preset = _visiblePresets[static_cast<std::size_t>(index)];

    for (auto* listener : _listeners)
        listener->onPresetSelected(preset);
}

}
