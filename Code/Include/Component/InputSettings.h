#pragma once

#include <nierika_dsp/nierika_dsp.h>
#include <juce_graphics/juce_graphics.h>

#include "PluginProcessor.h"

namespace component
{

// Mirrors OutputSettings' shape (plain nui::Component, own GridLayout, dim small-caps title with
// a bottom border) but for the input side: currently just MIDI input device selection, which -
// like OutputSettings' audio device/sample rate/buffer size controls - only makes sense in the
// Standalone build (a plugin format's MIDI input is host-routed, there's no device to pick).
class InputSettings : public nui::Component, public nelement::ComboBox::OnValueChangedListener
{
public:
    InputSettings(const std::string& identifier, PluginAudioProcessor& audioProcessor);
    ~InputSettings() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void onSelectionChanged(const std::string& selectorID, int selectedId) override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    // Re-populates _midiInputComboBox from juce::MidiInput::getAvailableDevices() (devices can be
    // hot-plugged, unlike audio devices there's no dedicated combo-box element that already does
    // this refresh for us - see OutputSettings' DevicesComboBoxWithConfig for that audio-side
    // equivalent).
    void refreshMidiInputDevices();
    void syncFromCurrentState();

    [[nodiscard]] bool isStandalone() const { return _audioProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone; }

    PluginAudioProcessor& _audioProcessor;

    nelement::Text _title { "input-settings-title", "", juce::translate("input_settings_title").toStdString() };

    nelement::Text _midiInputLabel { "input-settings-midi-input-label", "", juce::translate("input_settings_midi_input_label").toStdString() };
    nelement::ComboBox _midiInputComboBox { "input-midi-input-combobox" };

    // Index i's item id is always i + 2 (id 1 is reserved for "None") - kept in the same order
    // getAvailableDevices() returned them in, so onSelectionChanged can map an id straight back to
    // a device's identifier without a separate lookup structure.
    juce::Array<juce::MidiDeviceInfo> _availableMidiInputs;

    juce::AudioDeviceManager* _deviceManager = nullptr;

    nlayout::GridLayout<nui::Component> _layout { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InputSettings)
};

}
