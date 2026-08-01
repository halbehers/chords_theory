#pragma once

#include <nierika_dsp/nierika_dsp.h>
#include <juce_graphics/juce_graphics.h>

namespace component
{

class LanguageSettings : public nui::Component, public nelement::ComboBox::OnValueChangedListener
{
public:
    explicit LanguageSettings(const std::string& identifier);
    ~LanguageSettings() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void onSelectionChanged(const std::string& componentID, int selectedId) override;

    enum class ControlAlignment { Left, Right };
    static constexpr ControlAlignment CONTROL_ALIGNMENT = ControlAlignment::Left;
    [[nodiscard]] static float getControlX(const juce::Rectangle<float>& cell, float controlWidth);

    nelement::Text _title { "language-settings-title", "", juce::translate("language_settings_title").toStdString() };
    nelement::ComboBox _languagePicker { "language-settings-picker" };

    nlayout::GridLayout<nui::Component> _layout { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LanguageSettings)
};

}
