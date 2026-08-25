#include "Component/SavePresetPrompt.h"

namespace component
{

namespace
{
    constexpr float kWidth = 240.f;
    constexpr float kHeight = 40.f;
}

SavePresetPrompt::SavePresetPrompt(const std::string& identifier, std::function<void(const std::string&)> onSave):
    Component(identifier),
    _saveButton("save-preset-prompt-button", juce::translate("progression_save_preset_confirm").toStdString()),
    _onSave(std::move(onSave))
{
    _nameInput.setPlaceholder(juce::translate("progression_save_preset_placeholder").toStdString());
    _nameInput.setHeightType(nui::Theme::HeightType::THIN);
    _nameInput.addOnReturnKeyPressedListener(this);

    _saveButton.setHeightType(nui::Theme::HeightType::THIN);
    _saveButton.addOnClickListener(this);

    addAndMakeVisible(_nameInput);
    addAndMakeVisible(_saveButton);

    _layout.setGap(6.f);
    _layout.init({ 1 }, { 3, 1 });
    _layout.addComponent(_nameInput, 0, 0, 1, 1);
    _layout.addComponent(_saveButton, 0, 1, 1, 1);

    // Must come after the layout is fully populated above: setSize() triggers resized() (via
    // setBounds()) synchronously and immediately, which just forwards to _layout.resized() - if
    // called before addComponent(), the grid has nothing registered yet, so that first (and only)
    // layout pass positions nothing, leaving _nameInput/_saveButton at their construction-time
    // zero bounds. GridLayout::addComponent() only registers a component, it doesn't itself
    // trigger a re-layout, so nothing corrects this after the fact.
    setSize(static_cast<int>(kWidth), static_cast<int>(kHeight));

    _nameInput.grabKeyboardFocus();
}

SavePresetPrompt::~SavePresetPrompt()
{
    _nameInput.removeReturnKeyPressedListener(this);
    _saveButton.removeListener(this);
}

void SavePresetPrompt::resized()
{
    Component::resized();

    _layout.resized();
}

void SavePresetPrompt::onReturnKeyPressed(const std::string& textEditorID, const std::string& newValue)
{
    juce::ignoreUnused(textEditorID, newValue);

    confirm();
}

void SavePresetPrompt::onButtonClick(const std::string& buttonID)
{
    juce::ignoreUnused(buttonID);

    confirm();
}

void SavePresetPrompt::confirm()
{
    const auto name = _nameInput.getText();

    if (name.empty())
        return;

    if (_onSave)
        _onSave(name);

    if (auto* callOutBox = findParentComponentOfClass<juce::CallOutBox>())
        callOutBox->dismiss();
}

void SavePresetPrompt::show(juce::Rectangle<int> areaToPointTo, juce::Component* parentComponent, std::function<void(const std::string&)> onSave)
{
    auto content = std::make_unique<SavePresetPrompt>("save-preset-prompt", std::move(onSave));
    nelement::PopupPanel::show(std::move(content), areaToPointTo, parentComponent);
}

}
