#pragma once

#include <nierika_dsp/nierika_dsp.h>

namespace component
{

class SettingsWindow : public nlayout::Window,
                        public nelement::SVGButton::OnClickListener
{
public:
    SettingsWindow(const std::string& identifier, nlayout::WindowsManager& windowsManager);
    ~SettingsWindow() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    bool keyPressed(const juce::KeyPress& key) override;

    void onButtonClick(const std::string& componentID) override;

private:
    void close();
    [[nodiscard]] juce::Rectangle<int> getCardBounds();
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    nlayout::WindowsManager& _windowsManager;

    nelement::Text _title { "settings-title", "", "Settings" };
    nelement::SVGButton _titleIcon { "settings-title-icon", nui::Icons::getGear() };
    nelement::SVGButton _closeButton { "settings-close-button", nui::Icons::getCross() };

    nlayout::GridLayout<nui::Component> _layout { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsWindow)
};

}
