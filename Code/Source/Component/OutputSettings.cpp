#include "Component/OutputSettings.h"
#include "AppSettings.h"
#include "AppLocalisation.h"

#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

#include <algorithm>
#include <vector>

namespace component
{

OutputSettings::OutputSettings(const std::string& identifier, PluginAudioProcessor& audioProcessor):
    Component(identifier),
    _audioProcessor(audioProcessor)
{
    _title.setFontSize(nui::Theme::SMALL);
    _title.setColor(nui::Theme::ThemeColor::DISABLED);
    _title.setJustificationType(juce::Justification::centredLeft);

    _trimLabel.setJustificationType(juce::Justification::centredLeft);
    
    const auto persistedDb = AppSettings::getInstance().getOutputTrimDb();
    const auto* match = std::find(std::begin(TRIM_VALUES_DB), std::end(TRIM_VALUES_DB), persistedDb);
    const auto initialIndex = match != std::end(TRIM_VALUES_DB) ? static_cast<int>(match - std::begin(TRIM_VALUES_DB)) : 0;
    _trimSwitch.setSelectedIndex(initialIndex, juce::dontSendNotification);
    _trimSwitch.addOnValueChangedListener(this);
    _trimSwitch.setSelectedInvertedTextColor(true);
    _trimSwitch.setHeightType(nui::Theme::HeightType::THIN);
    _trimSwitch.setRounded(true);
    _trimSwitch.setThumbColour(nui::Theme::newColor(nui::Theme::ThemeColor::TERTIARY_ACCENT).asJuce());

    
    AppLocalisation::getChangeBroadcaster().addChangeListener(this);
    
    _layout.setGap(8.f);
    _layout.setDisplayGrid(false);
    _layout.init({ 1, 1, 1, 1, 1, 1 }, { 3, 5 });

    _layout.setFixedRowHeight(0, 32.f);
    _layout.setFixedRowHeight(2, 20.f);

    _layout.addComponent(_title, 0, 0, 2, 1);
    _layout.addComponent(_trimLabel, 1, 0, 1, 1);
    _layout.addComponent(_trimSwitch, 1, 1, 1, 1);

    if (isStandalone())
    {
        _devicesLabel.setJustificationType(juce::Justification::centredLeft);
        _devicesComboBox.setHeightType(nui::Theme::HeightType::THIN);
        _sampleRateLabel.setJustificationType(juce::Justification::centredLeft);
        _sampleRateComboBox.setHeightType(nui::Theme::HeightType::THIN);
        _bufferSizeLabel.setJustificationType(juce::Justification::centredLeft);
        _bufferSizeComboBox.setHeightType(nui::Theme::HeightType::THIN);

        _layout.addComponent(_devicesLabel, 2, 0, 1, 1);
        _layout.addComponent(_devicesComboBox, 3, 0, 2, 1);
        _layout.addComponent(_sampleRateLabel, 4, 0, 1, 1);
        _layout.addComponent(_sampleRateComboBox, 4, 1, 1, 1);
        _layout.addComponent(_bufferSizeLabel, 5, 0, 1, 1);
        _layout.addComponent(_bufferSizeComboBox, 5, 1, 1, 1);

        if (auto* holder = juce::StandalonePluginHolder::getInstance())
        {
            _deviceManager = &holder->deviceManager;
            _devicesComboBox.setDeviceManager(_deviceManager);

            _devicesComboBox.getComboBox().refreshDevices(juce::dontSendNotification);
            syncDeviceComboBoxesFromCurrentState();

            _devicesComboBox.getComboBox().addOnValueChangedListener(this);
            _sampleRateComboBox.addOnValueChangedListener(this);
            _bufferSizeComboBox.addOnValueChangedListener(this);

            _devicesComboBox.setTickedColour(nui::Theme::newColor(nui::Theme::ThemeColor::TERTIARY_ACCENT).asJuce());
            _sampleRateComboBox.setTickedColour(nui::Theme::newColor(nui::Theme::ThemeColor::TERTIARY_ACCENT).asJuce());
            _bufferSizeComboBox.setTickedColour(nui::Theme::newColor(nui::Theme::ThemeColor::TERTIARY_ACCENT).asJuce());

            _deviceManager->addChangeListener(this);
        }
    }

    _layout.setBottomBorder(_title.getComponentID().toStdString(), nui::Theme::newColor(nui::Theme::ThemeColor::BORDER).asJuce());
}

OutputSettings::~OutputSettings()
{
    _trimSwitch.removeListener(this);
    _devicesComboBox.getComboBox().removeListener(this);
    _sampleRateComboBox.removeListener(this);
    _bufferSizeComboBox.removeListener(this);

    if (_deviceManager != nullptr)
        _deviceManager->removeChangeListener(this);

    AppLocalisation::getChangeBroadcaster().removeChangeListener(this);
}

void OutputSettings::paint(juce::Graphics& g)
{
    Component::paint(g);

    _layout.paint(g);
}

void OutputSettings::resized()
{
    Component::resized();

    _layout.resized();
}

float OutputSettings::getControlX(const juce::Rectangle<float>& cell, float controlWidth)
{
    return CONTROL_ALIGNMENT == ControlAlignment::Right ? cell.getRight() - controlWidth : cell.getX();
}

void OutputSettings::onSelectionChanged(const std::string& componentID, int selectedIndex)
{
    if (componentID == _trimSwitch.getComponentID())
    {
        const auto db = TRIM_VALUES_DB[selectedIndex];
        AppSettings::getInstance().setOutputTrimDb(db);
        _audioProcessor.setOutputTrimDb(db);
        return;
    }

    if (_deviceManager == nullptr)
        return;

    if (componentID == _sampleRateComboBox.getComponentID())
    {
        auto setup = _deviceManager->getAudioDeviceSetup();
        setup.sampleRate = _sampleRateComboBox.getSelectedSampleRate();
        _deviceManager->setAudioDeviceSetup(setup, true);
        syncDeviceComboBoxesFromCurrentState();
    }
    else if (componentID == _bufferSizeComboBox.getComponentID())
    {
        auto setup = _deviceManager->getAudioDeviceSetup();
        setup.bufferSize = _bufferSizeComboBox.getSelectedBufferSize();
        _deviceManager->setAudioDeviceSetup(setup, true);
        syncDeviceComboBoxesFromCurrentState();
    }
    else if (componentID == _devicesComboBox.getComboBox().getComponentID())
    {
        const auto& device = _devicesComboBox.getComboBox().getSelectedDevice();

        switch (device.kind)
        {
            case ndsp::AudioOutputDeviceKind::NO_DEVICE:
                _deviceManager->closeAudioDevice();
                break;

            case ndsp::AudioOutputDeviceKind::SYSTEM_DEFAULT:
            {
                // Snaps to whatever JUCE currently reports as the default device, at the moment
                // of selection - not continuous tracking of the OS default afterwards.
                const auto currentSetup = _deviceManager->getAudioDeviceSetup();
                _deviceManager->initialise(currentSetup.inputChannels.countNumberOfSetBits(),
                                            currentSetup.outputChannels.countNumberOfSetBits(),
                                            nullptr, true);
                break;
            }

            case ndsp::AudioOutputDeviceKind::DEVICE:
            {
                _deviceManager->setCurrentAudioDeviceType(device.typeName, true);

                auto setup = _deviceManager->getAudioDeviceSetup();
                setup.outputDeviceName = device.name;
                setup.inputDeviceName = device.numInputChannels > 0 ? device.name : juce::String();

                // Reset channel selection to sensible defaults for the *new* device - carrying
                // over the previous device's outputChannels/inputChannels bitmask (e.g. a stereo
                // pair picked via the channel-config popup) can point at channel indices the new
                // device doesn't have, leaving it "selected" in the UI but silently producing no
                // audio.
                setup.useDefaultOutputChannels = true;
                setup.useDefaultInputChannels = true;
                setup.outputChannels.clear();
                setup.inputChannels.clear();

                _deviceManager->setAudioDeviceSetup(setup, true);
                break;
            }
        }

        syncDeviceComboBoxesFromCurrentState();
    }
}

void OutputSettings::syncDeviceComboBoxesFromCurrentState()
{
    if (_deviceManager == nullptr)
        return;

    const auto setup = _deviceManager->getAudioDeviceSetup();

    _sampleRateComboBox.setSelectedSampleRate(setup.sampleRate, juce::dontSendNotification);
    _bufferSizeComboBox.setSelectedBufferSize(setup.bufferSize, juce::dontSendNotification);

    if (auto* currentDevice = _deviceManager->getCurrentAudioDevice())
        _devicesComboBox.getComboBox().setSelectedDevice(ndsp::AudioOutputDeviceKind::DEVICE, currentDevice->getName(),
                                                          currentDevice->getTypeName(), juce::dontSendNotification);
    else
        _devicesComboBox.getComboBox().setSelectedDevice(ndsp::AudioOutputDeviceKind::NO_DEVICE, {}, {}, juce::dontSendNotification);
}

void OutputSettings::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    Component::changeListenerCallback(source);

    if (source == &nui::Theme::getChangeBroadcaster())
    {
        _layout.setBottomBorder(_title.getComponentID().toStdString(), nui::Theme::newColor(nui::Theme::ThemeColor::BORDER).asJuce());
        repaint();
    }

    if (_deviceManager != nullptr && source == _deviceManager)
    {
        // AudioDeviceManager broadcasts this for its own settings changing AND for the OS-level
        // device list changing (a device being plugged/unplugged - see its own
        // audioDeviceListChanged()/sendChangeMessage()) - refresh the combo box's item list first
        // so a newly-connected device actually becomes selectable, then re-sync which item that
        // list's current selection should point at.
        _devicesComboBox.getComboBox().refreshDevices(juce::dontSendNotification);
        syncDeviceComboBoxesFromCurrentState();
        return;
    }

    if (source != &AppLocalisation::getChangeBroadcaster())
        return;

    _title.setText(juce::translate("output_settings_title").toStdString());
    _trimLabel.setText(juce::translate("output_settings_trim_label").toStdString());
    _trimSwitch.setLabels(juce::translate("output_settings_trim_0db").toStdString(), juce::translate("output_settings_trim_neg6db").toStdString(), juce::translate("output_settings_trim_neg12db").toStdString());

    if (isStandalone())
    {
        _devicesLabel.setText(juce::translate("output_settings_devices_label").toStdString());
        _sampleRateLabel.setText(juce::translate("output_settings_sample_rate_label").toStdString());
        _bufferSizeLabel.setText(juce::translate("output_settings_buffer_size_label").toStdString());
    }
    repaint();
}

}
