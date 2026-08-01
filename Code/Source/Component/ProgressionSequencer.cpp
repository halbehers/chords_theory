#include "Component/ProgressionSequencer.h"

#include <algorithm>

#include "Component/SavePresetPrompt.h"
#include "Theory/ProgressionPresetFactory.h"
#include "Theory/ProgressionPresetLibrary.h"

namespace component
{

namespace
{
    constexpr float kHeaderRowHeight = 32.f;
    constexpr float kSlotRowHeight = 56.f;
    constexpr float kDragHandleRowHeight = 40.f;

    constexpr int kMinSlotCount = 1;
    constexpr int kDefaultSlotCount = 4;

    constexpr int kSlotWidth = 80;
    constexpr int kScrollbarThickness = 8; // matches the LookAndFeel's default scrollbar width
    constexpr int kScrollbarGap = 6; // additional vertical space between the slot row and the scrollbar below it
    constexpr int kGap = 8;
    constexpr int kStepButtonGap = 4; // vertical gap between the stacked "+"/"-" buttons
}

ProgressionSequencer::SlotsRow::SlotsRow(ProgressionSequencer& owner):
    Component("progression-slots-row"),
    _owner(owner)
{
}

bool ProgressionSequencer::SlotsRow::isInterestedInFileDrag(const juce::StringArray& files)
{
    return files.size() == 1 && files[0].endsWithIgnoreCase(".mid");
}

void ProgressionSequencer::SlotsRow::fileDragEnter(const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused(files, x, y);
}

void ProgressionSequencer::SlotsRow::fileDragExit(const juce::StringArray& files)
{
    juce::ignoreUnused(files);
}

void ProgressionSequencer::SlotsRow::filesDropped(const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused(x, y);

    if (!files.isEmpty())
        _owner.onSlotsRowFileDropped(files[0]);
}

ProgressionSequencer::ProgressionSequencer(const std::string& identifier, ChordResolver chordResolver):
    Component(identifier),
    _chordResolver(std::move(chordResolver)),
    _presetPicker("progression-preset-picker-wrapper"),
    _savePresetButton("progression-save-preset-button", juce::translate("progression_save_preset_button").toStdString()),
    _dragHandle("progression-drag-handle")
{
    _presetPicker.addListener(this);

    _savePresetButton.setHeightType(nui::Theme::HeightType::THIN);
    _savePresetButton.addOnClickListener(this);

    _dragHandle.addListener(this);

    addAndMakeVisible(_presetPicker);
    addAndMakeVisible(_savePresetButton);
    addAndMakeVisible(_dragHandle);

    addAndMakeVisible(_slotsViewport);
    _slotsViewport.setComponentID("progression-slots-viewport");
    _slotsViewport.setViewedComponent(&_slotsRow, false); // false: _slotsRow is an owned member, not viewport-owned
    _slotsViewport.setScrollBarsShown(false, true); // horizontal only

    _addStepButton.addOnClickListener(this);
    _slotsRow.addAndMakeVisible(_addStepButton);

    _removeStepButton.addOnClickListener(this);
    _slotsRow.addAndMakeVisible(_removeStepButton);

    for (int i = 0; i < kDefaultSlotCount; ++i)
        addStep();

    _layout.setGap(8.f);
    _layout.setDisplayGrid(false);

    _layout.init({ 1, 1, 1 }, { 4, 1 }); // row0: picker | save-button, row1: slots viewport, row2: drag handle

    _layout.setFixedRowHeight(0, kHeaderRowHeight);
    _layout.setFixedRowHeight(1, kSlotRowHeight);
    _layout.setFixedRowHeight(2, kDragHandleRowHeight);

    _layout.addComponent(_presetPicker, 0, 0, 1, 1);
    _layout.addComponent(_savePresetButton, 0, 1, 1, 1);
    _layout.addComponent("progression-slots-viewport", _slotsViewport, 1, 0, 2, 1); // explicit-identifier overload - juce::Viewport isn't a nui::Component
    _layout.addComponent(_dragHandle, 2, 0, 2, 1);
}

ProgressionSequencer::~ProgressionSequencer()
{
    _presetPicker.removeListener(this);
    _savePresetButton.removeListener(this);
    _dragHandle.removeListener(this);
    _addStepButton.removeListener(this);
    _removeStepButton.removeListener(this);

    for (auto& slot : _slots)
        slot->removeListener(this);
}

void ProgressionSequencer::paint(juce::Graphics& g)
{
    Component::paint(g);

    _layout.paint(g);
}

void ProgressionSequencer::resized()
{
    Component::resized();

    _layout.resized();
    layoutSlotsRow();
}

void ProgressionSequencer::setScale(theory::Scale scale)
{
    _currentScale = scale;
    _presetPicker.refreshForScale(scale);
}

void ProgressionSequencer::setSlotDegree(int slotIndex, theory::Degree degree)
{
    setSlot(slotIndex, theory::ProgressionSlot { degree, 0 }); // unpinned - keeps tracking the degree's live voicing
}

void ProgressionSequencer::setSlot(int slotIndex, const theory::ProgressionSlot& slot)
{
    if (slotIndex < 0)
        return;

    while (slotIndex >= static_cast<int>(_slots.size()))
        addStep();

    _slotData[static_cast<std::size_t>(slotIndex)] = slot;
    _slotOccupied[static_cast<std::size_t>(slotIndex)] = true;

    refreshSlotView(slotIndex);

    for (auto* listener : _listeners)
        listener->onSlotsChanged();
}

void ProgressionSequencer::clearSlot(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= static_cast<int>(_slots.size()))
        return;

    _slotOccupied[static_cast<std::size_t>(slotIndex)] = false;
    _slots[static_cast<std::size_t>(slotIndex)]->clear();

    for (auto* listener : _listeners)
        listener->onSlotsChanged();
}

void ProgressionSequencer::removeSlotAndShift(int slotIndex)
{
    const auto count = static_cast<int>(_slots.size());
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

    for (int i = slotIndex; i < static_cast<int>(_slots.size()); ++i)
    {
        if (_slotOccupied[static_cast<std::size_t>(i)])
            refreshSlotView(i);
        else
            _slots[static_cast<std::size_t>(i)]->clear();
    }

    for (auto* listener : _listeners)
        listener->onSlotsChanged();
}

std::optional<theory::Degree> ProgressionSequencer::getSlotDegree(int slotIndex) const
{
    if (slotIndex < 0 || slotIndex >= static_cast<int>(_slots.size()) || !_slotOccupied[static_cast<std::size_t>(slotIndex)])
        return std::nullopt;

    return _slotData[static_cast<std::size_t>(slotIndex)].degree;
}

void ProgressionSequencer::refreshSlotView(int slotIndex)
{
    const auto& slotData = _slotData[static_cast<std::size_t>(slotIndex)];

    if (const auto* chord = _chordResolver ? _chordResolver(slotData) : nullptr)
        _slots[static_cast<std::size_t>(slotIndex)]->setChord(slotData.degree, *chord);
    else
        _slots[static_cast<std::size_t>(slotIndex)]->clear();
}

std::vector<theory::ProgressionSlot> ProgressionSequencer::getPopulatedSlots() const
{
    std::vector<theory::ProgressionSlot> populated;

    for (int i = 0; i < static_cast<int>(_slots.size()); ++i)
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

void ProgressionSequencer::loadPreset(const theory::ProgressionPreset& preset)
{
    const auto numToLoad = static_cast<int>(preset.slots.size());

    for (int i = 0; i < numToLoad; ++i)
        setSlot(i, preset.slots[static_cast<std::size_t>(i)]);

    while (static_cast<int>(_slots.size()) > juce::jmax(numToLoad, kMinSlotCount))
        removeLastSlot();

    for (int i = numToLoad; i < static_cast<int>(_slots.size()); ++i)
        clearSlot(i); // only non-empty when numToLoad < kMinSlotCount, i.e. an empty preset
}

void ProgressionSequencer::addStep()
{
    const auto newIndex = static_cast<int>(_slots.size());
    auto slotView = std::make_unique<ProgressionSlotView>("progression-slot-" + std::to_string(newIndex), newIndex);
    slotView->addListener(this);
    _slotsRow.addAndMakeVisible(*slotView);
    _slots.push_back(std::move(slotView));
    _slotData.push_back(theory::ProgressionSlot {});
    _slotOccupied.push_back(false);

    layoutSlotsRow();
    _removeStepButton.setEnabled(static_cast<int>(_slots.size()) > kMinSlotCount);
    _slotsViewport.setViewPosition(juce::jmax(0, _slotsRow.getWidth() - _slotsViewport.getWidth()), 0); // scroll to reveal it
}

void ProgressionSequencer::removeLastSlot()
{
    if (static_cast<int>(_slots.size()) <= kMinSlotCount)
        return;

    _slots.back()->removeListener(this);
    _slots.pop_back();
    _slotData.pop_back();
    _slotOccupied.pop_back();

    layoutSlotsRow();
    _removeStepButton.setEnabled(static_cast<int>(_slots.size()) > kMinSlotCount);
}

void ProgressionSequencer::layoutSlotsRow()
{
    // Deliberately not using _slotsViewport.getMaximumVisibleHeight() here - see
    // VoicingSelector::layoutVoicingRow() for why that has a circular timing dependency on the
    // very content width this method is about to set. Reserving fixed space against the
    // viewport's own bounds instead sizes the row correctly on every call.
    const auto rowHeight = juce::jmax(0, _slotsViewport.getHeight() - kScrollbarThickness - kScrollbarGap);
    const auto stepButtonSize = juce::jmax(0, (rowHeight - kStepButtonGap) / 2); // stacked, so each takes half the row height

    const auto slotCount = static_cast<int>(_slots.size());
    const auto rowWidth = slotCount * kSlotWidth + slotCount * kGap + stepButtonSize + kGap; // one trailing kGap of breathing room after the stacked +/- buttons
    _slotsRow.setSize(rowWidth, rowHeight);

    auto x = 0;
    for (auto& slot : _slots)
    {
        slot->setBounds(x, 0, kSlotWidth, rowHeight);
        x += kSlotWidth + kGap;
    }

    _addStepButton.setIconSize(static_cast<float>(stepButtonSize) * 0.6f); // matches VoicingSelector's close-button icon-to-button ratio
    _removeStepButton.setIconSize(static_cast<float>(stepButtonSize) * 0.6f);

    _addStepButton.setBounds(x, 0, stepButtonSize, stepButtonSize);
    _removeStepButton.setBounds(x, stepButtonSize + kStepButtonGap, stepButtonSize, stepButtonSize);
}

void ProgressionSequencer::addListener(Listener* listener)
{
    _listeners.push_back(listener);
}

void ProgressionSequencer::removeListener(Listener* listener)
{
    _listeners.erase(std::remove(_listeners.begin(), _listeners.end(), listener), _listeners.end());
}

void ProgressionSequencer::onFileDropped(int slotIndex, const juce::String& filePath)
{
    for (auto* listener : _listeners)
        listener->onSlotFileDropped(slotIndex, filePath);
}

void ProgressionSequencer::onSlotsRowFileDropped(const juce::String& filePath)
{
    if (filePath.isEmpty())
        return;

    const auto emptyIt = std::find(_slotOccupied.begin(), _slotOccupied.end(), false);
    int targetIndex;
    if (emptyIt != _slotOccupied.end())
    {
        targetIndex = static_cast<int>(std::distance(_slotOccupied.begin(), emptyIt));
    }
    else
    {
        targetIndex = static_cast<int>(_slots.size());
        addStep(); // the new slot is always unoccupied, so it's a valid drop target
    }

    for (auto* listener : _listeners)
        listener->onSlotFileDropped(targetIndex, filePath);
}

void ProgressionSequencer::onSlotPreviewRequested(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= static_cast<int>(_slots.size()) || !_slotOccupied[static_cast<std::size_t>(slotIndex)])
        return;

    const auto& slotData = _slotData[static_cast<std::size_t>(slotIndex)];

    if (const auto* chord = _chordResolver ? _chordResolver(slotData) : nullptr)
        for (auto* listener : _listeners)
            listener->onChordPreviewRequested(*chord);
}

void ProgressionSequencer::onSlotRemoveRequested(int slotIndex)
{
    removeSlotAndShift(slotIndex);
}

void ProgressionSequencer::onPresetSelected(const theory::ProgressionPreset& preset)
{
    loadPreset(preset);
}

void ProgressionSequencer::onProgressionDragStarted()
{
    for (auto* listener : _listeners)
        listener->onProgressionDragStarted();
}

void ProgressionSequencer::onButtonClick(const std::string& componentID)
{
    if (componentID == _addStepButton.getComponentID())
    {
        addStep();
        return;
    }

    if (componentID == _removeStepButton.getComponentID())
    {
        const auto wasOccupied = !_slotOccupied.empty() && _slotOccupied.back();
        removeLastSlot();

        if (wasOccupied)
            for (auto* listener : _listeners)
                listener->onSlotsChanged();

        return;
    }

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
