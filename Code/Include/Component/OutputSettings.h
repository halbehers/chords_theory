#pragma once

#include <nierika_dsp/nierika_dsp.h>
#include <juce_graphics/juce_graphics.h>

#include "PluginProcessor.h"

namespace component
{

class OutputSettings : public nui::Component, public nelement::ThreeWaySwitch::OnValueChangedListener, public nelement::ComboBox::OnValueChangedListener
{
public:
    OutputSettings(const std::string& identifier, PluginAudioProcessor& audioProcessor);
    ~OutputSettings() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void onSelectionChanged(const std::string& componentID, int selectedIndex) override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    
    void syncDeviceComboBoxesFromCurrentState();

    enum class ControlAlignment { Left, Right };
    static constexpr ControlAlignment CONTROL_ALIGNMENT = ControlAlignment::Left;
    [[nodiscard]] static float getControlX(const juce::Rectangle<float>& cell, float controlWidth);

    [[nodiscard]] bool isStandalone() const { return _audioProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone; }

    static constexpr int TRIM_VALUES_DB[3] = { 0, -6, -12 };

    PluginAudioProcessor& _audioProcessor;

    nelement::Text _title { "output-settings-title", "", juce::translate("output_settings_title").toStdString() };

    nelement::Text _trimLabel { "output-settings-trim-label", "", juce::translate("output_settings_trim_label").toStdString() };
    nelement::ThreeWaySwitch _trimSwitch { "output-trim-switch", juce::translate("output_settings_trim_0db").toStdString(), juce::translate("output_settings_trim_neg6db").toStdString(), juce::translate("output_settings_trim_neg12db").toStdString() };

    nelement::Text _devicesLabel { "output-settings-devices-label", "", juce::translate("output_settings_devices_label").toStdString() };
    nelement::DevicesComboBoxWithConfig _devicesComboBox { "output-devices-combobox" };
    nelement::Text _sampleRateLabel { "output-settings-sample-rate-label", "", juce::translate("output_settings_sample_rate_label").toStdString() };
    nelement::SampleRateComboBox _sampleRateComboBox { "output-sample-rate-combobox" };
    nelement::Text _bufferSizeLabel { "output-settings-buffer-size-label", "", juce::translate("output_settings_buffer_size_label").toStdString() };
    nelement::BufferSizeComboBox _bufferSizeComboBox { "output-buffer-size-combobox" };

    juce::AudioDeviceManager* _deviceManager = nullptr;

    nlayout::GridLayout<nui::Component> _layout { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutputSettings)
};

}
