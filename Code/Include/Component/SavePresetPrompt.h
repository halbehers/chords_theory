#pragma once

#include <functional>

#include <nierika_dsp/nierika_dsp.h>

namespace component
{

// Popup content prompting for a preset name (used by ProgressionEditor's "Save as preset"
// affordance) - a text field plus a confirm button, either of which invokes the supplied callback
// with the entered name and closes the enclosing CallOutBox. Shown via the static show() helper,
// wrapping nelement::PopupPanel::show(...) - only ever the popup's content, never a standalone
// widget on the main layout.
class SavePresetPrompt : public nui::Component,
                          public nelement::TextInput::OnReturnKeyPressedListener,
                          public nelement::TextButton::OnClickListener
{
public:
    SavePresetPrompt(const std::string& identifier, std::function<void(const std::string&)> onSave);
    ~SavePresetPrompt() override;

    void resized() override;

    static void show(juce::Rectangle<int> areaToPointTo, juce::Component* parentComponent, std::function<void(const std::string&)> onSave);

private:
    void onReturnKeyPressed(const std::string& componentID, const std::string& newValue) override;
    void onButtonClick(const std::string& componentID) override;

    void confirm();

    nelement::TextInput _nameInput { "save-preset-prompt-input" };
    nelement::TextButton _saveButton;

    std::function<void(const std::string&)> _onSave;

    nlayout::GridLayout<nui::Component> _layout { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SavePresetPrompt)
};

}
