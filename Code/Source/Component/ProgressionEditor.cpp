#include "Component/ProgressionEditor.h"

#include <algorithm>

#include "AppLocalisation.h"
#include "Component/SavePresetPrompt.h"
#include "Theory/ProgressionPresetFactory.h"
#include "Theory/ProgressionPresetLibrary.h"

namespace component
{

namespace
{
    constexpr float kHeaderRowHeight = 42.f;
}

ProgressionEditor::ProgressionEditor(const std::string& identifier, ChordResolver chordResolver, audio::ProgressionPlayer* progressionPlayer):
    Component(identifier),
    _chordResolver(std::move(chordResolver)),
    _presetPicker("progression-preset-picker-wrapper"),
    _savePresetButton("progression-save-preset-button", nui::Icons::getPlus()),
    _dragOutButton("progression-drag-out-button"),
    _midiEditor("progression-midi-editor", progressionPlayer)
{
    _presetPicker.addListener(this);

    _savePresetButton.setIconSize(16.f);
    _savePresetButton.addOnClickListener(this);

    _playButton.setIconSize(16.f);
    _playButton.addOnClickListener(this);

    _dragOutButton.addListener(this);

    _presetsLabel.setText(juce::translate("progression_presets_label").toStdString());
    _presetsLabel.setFontSize(nui::Theme::LABEL);
    _presetsLabel.setJustificationType(juce::Justification::centredRight);

    _midiEditor.addListener(this);

    AppLocalisation::getChangeBroadcaster().addChangeListener(this);

    _layout.setGap(8.f);
    _layout.setDisplayGrid(false);
    _layout.setMargin(0.f, 0.f, 0.f, 16.f);

    _layout.init({ 1, 1, 1 }, { 1, 1, 4, 1, 1, 1 });

    _layout.setFixedColumnWidth(0, 32.f);
    _layout.setFixedColumnWidth(1, 32.f);
    _layout.setFixedColumnWidth(5, 32.f);
    _layout.setFixedColumnWidth(4, 250.f);
    _layout.setFixedColumnWidth(3, 175.f);

    _layout.setFixedRowHeight(0, kHeaderRowHeight);
    _layout.setFixedRowHeight(1, 12.f);

    _layout.addComponent(_playButton, 0, 0, 1, 1);
    _layout.addComponent(_dragOutButton, 0, 1, 1, 1);
    _layout.addComponent(_presetsLabel, 0, 3, 1, 1);
    _layout.addComponent(_presetPicker, 0, 4, 1, 1);
    _layout.addComponent(_savePresetButton, 0, 5, 1, 1);
    _layout.addComponent(_midiEditor, 2, 0, 6, 1);
}

ProgressionEditor::~ProgressionEditor()
{
    AppLocalisation::getChangeBroadcaster().removeChangeListener(this);

    _presetPicker.removeListener(this);
    _savePresetButton.removeListener(this);
    _playButton.removeListener(this);
    _dragOutButton.removeListener(this);
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

void ProgressionEditor::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    Component::changeListenerCallback(source);

    if (source != &AppLocalisation::getChangeBroadcaster())
        return;

    _presetsLabel.setText(juce::translate("progression_presets_label").toStdString());
}

void ProgressionEditor::setKeyAndScale(theory::Key key, theory::Scale scale)
{
    _currentKey = key;
    _currentScale = scale;
    _presetPicker.refreshForScale(scale);
    _midiEditor.setKeyAndScale(key, scale);
}

void ProgressionEditor::loadPreset(const theory::ProgressionPreset& preset)
{
    _midiEditor.clear();

    for (int i = 0; i < static_cast<int>(preset.slots.size()); ++i)
    {
        const auto& slot = preset.slots[static_cast<std::size_t>(i)];
        if (const auto* chord = _chordResolver ? _chordResolver(slot) : nullptr)
            _midiEditor.addChordAtBeat(static_cast<double>(i) * MidiEditor::kBeatsPerBar, *chord);
    }
}

std::vector<theory::ProgressionSlot> ProgressionEditor::getPopulatedSlots() const
{
    struct IndexedSlot
    {
        double startBeat;
        theory::ProgressionSlot slot;
    };

    std::vector<IndexedSlot> indexed;
    indexed.reserve(static_cast<std::size_t>(_midiEditor.getChordBlockCount()));

    for (int i = 0; i < _midiEditor.getChordBlockCount(); ++i)
    {
        const auto startBeat = _midiEditor.getChordBlockStartBeat(i);
        const auto slot = _midiEditor.getChordBlockSlot(i);
        if (startBeat && slot)
            indexed.push_back({ *startBeat, *slot });
    }

    std::sort(indexed.begin(), indexed.end(), [](const auto& a, const auto& b) { return a.startBeat < b.startBeat; });

    std::vector<theory::ProgressionSlot> result;
    result.reserve(indexed.size());
    for (const auto& entry : indexed)
        result.push_back(entry.slot);

    return result;
}

void ProgressionEditor::addChordAtBeat(double startBeat, const theory::Chord& chord)
{
    _midiEditor.addChordAtBeat(startBeat, chord);
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

void ProgressionEditor::onContentChanged()
{
    for (auto* listener : _listeners)
        listener->onContentChanged();
}

void ProgressionEditor::onPlaybackStateChanged(bool isPlaying)
{
    // Not the click handler's job to set this directly - it needs to stay correct even when
    // playback stops "from underneath" (e.g. clear()/restoreState() via a preset load), not just
    // in response to this button's own click.
    _playButton.setIconBinary(isPlaying ? nui::Icons::getStop() : nui::Icons::getPlay());

    for (auto* listener : _listeners)
        listener->onPlaybackStateChanged(isPlaying);
}

void ProgressionEditor::onChordBlockDragStarted(int chordBlockIndex)
{
    for (auto* listener : _listeners)
        listener->onChordBlockDragStarted(chordBlockIndex); // pure bubble-up, never resolves itself
}

void ProgressionEditor::onPresetSelected(const theory::ProgressionPreset& preset)
{
    loadPreset(preset);
}

void ProgressionEditor::onDragStarted()
{
    for (auto* listener : _listeners)
        listener->onProgressionDragStarted();
}

void ProgressionEditor::onButtonClick(const std::string& componentID)
{
    if (componentID == _playButton.getComponentID())
    {
        if (_midiEditor.isPlaying())
            _midiEditor.stopPlayback();
        else
            _midiEditor.startPlayback();
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
