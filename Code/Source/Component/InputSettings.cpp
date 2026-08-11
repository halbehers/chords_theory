#include "Component/InputSettings.h"
#include "AppLocalisation.h"

#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace component
{

namespace
{
    constexpr int kNoMidiInputId = 1;
}

InputSettings::InputSettings(const std::string& identifier, PluginAudioProcessor& audioProcessor):
    Component(identifier),
    _audioProcessor(audioProcessor)
{
    _title.setFontSize(nui::Theme::SMALL);
    _title.setColor(nui::Theme::ThemeColor::DISABLED);
    _title.setJustificationType(juce::Justification::centredLeft);

    AppLocalisation::getChangeBroadcaster().addChangeListener(this);

    _layout.setGap(8.f);
    _layout.setDisplayGrid(false);
    _layout.init({ 1, 1, 1 }, { 3, 5 });

    _layout.setFixedRowHeight(0, 32.f);

    _layout.addComponent(_title, 0, 0, 2, 1);

    if (isStandalone())
    {
        _midiInputLabel.setJustificationType(juce::Justification::centredLeft);
        _midiInputComboBox.setHeightType(nui::Theme::HeightType::THIN);

        _layout.addComponent(_midiInputLabel, 1, 0, 1, 1);
        _layout.addComponent(_midiInputComboBox, 1, 1, 1, 1);

        if (auto* holder = juce::StandalonePluginHolder::getInstance())
        {
            _deviceManager = &holder->deviceManager;

            syncFromCurrentState();

            _midiInputComboBox.addOnValueChangedListener(this);
            _deviceManager->addChangeListener(this);
        }
    }

    _layout.setBottomBorder(_title.getComponentID().toStdString(), nui::Theme::newColor(nui::Theme::ThemeColor::BORDER).asJuce());
}

InputSettings::~InputSettings()
{
    _midiInputComboBox.removeListener(this);

    if (_deviceManager != nullptr)
        _deviceManager->removeChangeListener(this);

    AppLocalisation::getChangeBroadcaster().removeChangeListener(this);
}

void InputSettings::paint(juce::Graphics& g)
{
    Component::paint(g);

    _layout.paint(g);
}

void InputSettings::resized()
{
    Component::resized();

    _layout.resized();
}

void InputSettings::refreshMidiInputDevices()
{
    _availableMidiInputs = juce::MidiInput::getAvailableDevices();

    _midiInputComboBox.clear(juce::dontSendNotification);
    _midiInputComboBox.addItem(juce::translate("input_settings_midi_input_none"), kNoMidiInputId);
    for (int i = 0; i < _availableMidiInputs.size(); ++i)
        _midiInputComboBox.addItem(_availableMidiInputs[i].name, i + kNoMidiInputId + 1);
}

void InputSettings::syncFromCurrentState()
{
    if (_deviceManager == nullptr)
        return;

    refreshMidiInputDevices();

    for (int i = 0; i < _availableMidiInputs.size(); ++i)
    {
        if (_deviceManager->isMidiInputDeviceEnabled(_availableMidiInputs[i].identifier))
        {
            _midiInputComboBox.setSelectedId(i + kNoMidiInputId + 1, juce::dontSendNotification);
            return;
        }
    }

    _midiInputComboBox.setSelectedId(kNoMidiInputId, juce::dontSendNotification);
}

void InputSettings::onSelectionChanged(const std::string& componentID, int selectedId)
{
    if (_deviceManager == nullptr || componentID != _midiInputComboBox.getComponentID())
        return;

    // Single-selection by design (mirrors the audio Devices combo box's UX) - picking a new MIDI
    // input replaces whichever one was previously enabled rather than layering it on top.
    for (const auto& device : _availableMidiInputs)
        _deviceManager->setMidiInputDeviceEnabled(device.identifier, false);

    const auto index = selectedId - kNoMidiInputId - 1;
    if (index >= 0 && index < _availableMidiInputs.size())
        _deviceManager->setMidiInputDeviceEnabled(_availableMidiInputs[index].identifier, true);
}

void InputSettings::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    Component::changeListenerCallback(source);

    if (source == &nui::Theme::getChangeBroadcaster())
    {
        _layout.setBottomBorder(_title.getComponentID().toStdString(), nui::Theme::newColor(nui::Theme::ThemeColor::BORDER).asJuce());
        repaint();
    }

    if (_deviceManager != nullptr && source == _deviceManager)
    {
        syncFromCurrentState();
        return;
    }

    if (source != &AppLocalisation::getChangeBroadcaster())
        return;

    _title.setText(juce::translate("input_settings_title").toStdString());

    if (isStandalone())
    {
        _midiInputLabel.setText(juce::translate("input_settings_midi_input_label").toStdString());
        refreshMidiInputDevices();
        syncFromCurrentState();
    }

    repaint();
}

}
