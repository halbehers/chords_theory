#include "Component/ProgressionEditor.h"

#include <algorithm>

#include "Component/SavePresetPrompt.h"
#include "Theory/ProgressionPresetFactory.h"
#include "Theory/ProgressionPresetLibrary.h"

namespace component
{

namespace
{
    constexpr float kHeaderRowHeight = 32.f;

    constexpr int kMinSlotCount = 1;
    constexpr int kDefaultSlotCount = 4;
}

ProgressionEditor::ProgressionEditor(const std::string& identifier, ChordResolver chordResolver):
    Component(identifier),
    _chordResolver(std::move(chordResolver)),
    _presetPicker("progression-preset-picker-wrapper"),
    _savePresetButton("progression-save-preset-button", nui::Icons::getPlus()),
    _dragHandle("progression-drag-handle")
{
    _presetPicker.addListener(this);

    _savePresetButton.setIconSize(16.f);
    _savePresetButton.addOnClickListener(this);

    _dragHandle.addListener(this);

    _presetsLabel.setText(juce::translate("progression_presets_label").toStdString());
    _presetsLabel.setFontSize(nui::Theme::LABEL);
    _presetsLabel.setJustificationType(juce::Justification::centredRight);

    _midiEditor.addListener(this);

    for (int i = 0; i < kDefaultSlotCount; ++i)
        addStep();

    _layout.setGap(8.f);
    _layout.setDisplayGrid(false);

    _layout.init({ 1, 1, 1 }, { 1, 1, 1, 4, 1, 1, 1 });

    _layout.setFixedColumnWidth(0, 32.f);
    _layout.setFixedColumnWidth(1, 12.f);
    _layout.setFixedColumnWidth(2, 150.f);
    _layout.setFixedColumnWidth(6, 32.f);
    _layout.setFixedColumnWidth(5, 250.f);
    _layout.setFixedColumnWidth(4, 175.f);

    _layout.setFixedRowHeight(0, kHeaderRowHeight);
    _layout.setFixedRowHeight(1, 12.f);

    _layout.addComponent(_dragHandle, 0, 2, 1, 1);
    _layout.addComponent(_presetsLabel, 0, 4, 1, 1);
    _layout.addComponent(_presetPicker, 0, 5, 1, 1);
    _layout.addComponent(_savePresetButton, 0, 6, 1, 1);
    _layout.addComponent(_midiEditor, 2, 0, 7, 1);
}

ProgressionEditor::~ProgressionEditor()
{
    _presetPicker.removeListener(this);
    _savePresetButton.removeListener(this);
    _dragHandle.removeListener(this);
    _midiEditor.removeListener(this);
}

void ProgressionEditor::paint(juce::Graphics& g)
{
    Component::paint(g);

    _layout.paint(g);
}

void ProgressionEditor::resized()
{
    Component::resized();

    _layout.resized();
}

void ProgressionEditor::setScale(theory::Scale scale)
{
    _currentScale = scale;
    _presetPicker.refreshForScale(scale);
}

void ProgressionEditor::setSlotDegree(int slotIndex, theory::Degree degree)
{
    setSlot(slotIndex, theory::ProgressionSlot { degree, 0 }); // unpinned - keeps tracking the degree's live voicing
}

void ProgressionEditor::setSlot(int slotIndex, const theory::ProgressionSlot& slot)
{
    if (slotIndex < 0)
        return;

    while (slotIndex >= static_cast<int>(_slotData.size()))
        addStep();

    _slotData[static_cast<std::size_t>(slotIndex)] = slot;
    _slotOccupied[static_cast<std::size_t>(slotIndex)] = true;

    for (auto* listener : _listeners)
        listener->onSlotsChanged();
}

void ProgressionEditor::clearSlot(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= static_cast<int>(_slotData.size()))
        return;

    _slotOccupied[static_cast<std::size_t>(slotIndex)] = false;

    for (auto* listener : _listeners)
        listener->onSlotsChanged();
}

void ProgressionEditor::removeSlotAndShift(int slotIndex)
{
    const auto count = static_cast<int>(_slotData.size());
    if (slotIndex < 0 || slotIndex >= count || !_slotOccupied[static_cast<std::size_t>(slotIndex)])
        return;

    for (int i = slotIndex; i < count - 1; ++i)
    {
        _slotData[static_cast<std::size_t>(i)] = _slotData[static_cast<std::size_t>(i + 1)];
        _slotOccupied[static_cast<std::size_t>(i)] = _slotOccupied[static_cast<std::size_t>(i + 1)];
    }

    if (count > kMinSlotCount)
        removeLastSlot();
    else
        _slotOccupied[static_cast<std::size_t>(count - 1)] = false;

    for (auto* listener : _listeners)
        listener->onSlotsChanged();
}

std::optional<theory::Degree> ProgressionEditor::getSlotDegree(int slotIndex) const
{
    if (slotIndex < 0 || slotIndex >= static_cast<int>(_slotData.size()) || !_slotOccupied[static_cast<std::size_t>(slotIndex)])
        return std::nullopt;

    return _slotData[static_cast<std::size_t>(slotIndex)].degree;
}

std::vector<theory::ProgressionSlot> ProgressionEditor::getPopulatedSlots() const
{
    std::vector<theory::ProgressionSlot> populated;

    for (int i = 0; i < static_cast<int>(_slotData.size()); ++i)
    {
        if (!_slotOccupied[static_cast<std::size_t>(i)])
            continue;

        auto slot = _slotData[static_cast<std::size_t>(i)];

        // Bakes in the currently-resolved chord's popularityOrder - this is what lets "Save as
        // preset" pin the exact voicing the user had selected, rather than saving just the bare degree.
        if (const auto* chord = _chordResolver ? _chordResolver(slot) : nullptr)
            slot.popularityOrder = chord->popularityOrder;

        populated.push_back(slot);
    }

    return populated;
}

void ProgressionEditor::loadPreset(const theory::ProgressionPreset& preset)
{
    const auto numToLoad = static_cast<int>(preset.slots.size());

    for (int i = 0; i < numToLoad; ++i)
        setSlot(i, preset.slots[static_cast<std::size_t>(i)]);

    while (static_cast<int>(_slotData.size()) > juce::jmax(numToLoad, kMinSlotCount))
        removeLastSlot();

    for (int i = numToLoad; i < static_cast<int>(_slotData.size()); ++i)
        clearSlot(i); // only non-empty when numToLoad < kMinSlotCount, i.e. an empty preset
}

void ProgressionEditor::addChordAtBeat(double startBeat, const theory::Chord& chord)
{
    _midiEditor.addChordAtBeat(startBeat, chord);
}

void ProgressionEditor::addStep()
{
    _slotData.push_back(theory::ProgressionSlot {});
    _slotOccupied.push_back(false);
}

void ProgressionEditor::removeLastSlot()
{
    if (static_cast<int>(_slotData.size()) <= kMinSlotCount)
        return;

    _slotData.pop_back();
    _slotOccupied.pop_back();
}

void ProgressionEditor::addListener(Listener* listener)
{
    _listeners.push_back(listener);
}

void ProgressionEditor::removeListener(Listener* listener)
{
    _listeners.erase(std::remove(_listeners.begin(), _listeners.end(), listener), _listeners.end());
}

void ProgressionEditor::onChordFileDropped(double startBeat, const juce::String& filePath)
{
    for (auto* listener : _listeners)
        listener->onChordFileDropped(startBeat, filePath); // pure bubble-up, never resolves itself
}

void ProgressionEditor::onPresetSelected(const theory::ProgressionPreset& preset)
{
    loadPreset(preset);
}

void ProgressionEditor::onProgressionDragStarted()
{
    for (auto* listener : _listeners)
        listener->onProgressionDragStarted();
}

void ProgressionEditor::onButtonClick(const std::string& componentID)
{
    if (componentID != _savePresetButton.getComponentID())
        return;

    const auto populatedSlots = getPopulatedSlots();
    if (populatedSlots.empty())
        return;

    // areaToPointTo must be relative to parentComponent (`this`) when parentComponent is non-null,
    // per juce::CallOutBox::launchAsynchronously's documented contract - _savePresetButton.getBounds()
    // is already in that space (it's this component's own direct child), so no conversion is needed.
    SavePresetPrompt::show(_savePresetButton.getBounds(), this,
        [this, populatedSlots](const std::string& name)
        {
            const auto preset = theory::ProgressionPresetFactory::createFromSlots(name, populatedSlots, _currentScale);
            theory::ProgressionPresetLibrary::getInstance().savePreset(preset);
            _presetPicker.refreshForScale(_currentScale);
        });
}

}
