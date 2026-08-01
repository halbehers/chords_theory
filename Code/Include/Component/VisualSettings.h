#pragma once

#include <nierika_dsp/nierika_dsp.h>
#include <juce_graphics/juce_graphics.h>

namespace component
{

class VisualSettings : public nui::Component, public nelement::TwoWaySwitch::OnValueChangedListener
{
public:
    explicit VisualSettings(const std::string& identifier);
    ~VisualSettings() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void onSelectionChanged(const std::string& componentID, int selectedIndex) override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    nelement::Text _title { "visual-settings-title", "", juce::translate("visual_settings_title").toStdString() };

    nelement::Text _themeLabel { "settings-theme-label", "", juce::translate("visual_settings_theme_label").toStdString() };
    nelement::TwoWaySwitch _themeSwitch { "settings-theme-toggle", juce::translate("visual_settings_dark_theme").toStdString(), juce::translate("visual_settings_light_theme").toStdString() };

    nlayout::GridLayout<nui::Component> _layout { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VisualSettings)
};

}
