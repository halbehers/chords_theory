#include "Component/LanguageSettings.h"
#include "AppSettings.h"
#include "AppLocalisation.h"

#include <algorithm>
#include <vector>

namespace component
{

namespace
{
    const std::string LANGUAGE_PICKER_ID = "language-settings-picker";

    struct LanguageOption { int id; std::string code; juce::String label; };

    const std::vector<LanguageOption>& getLanguageOptions()
    {
        static const std::vector<LanguageOption> options = {
            { 1, "en", "English" },
            { 2, "fr", juce::CharPointer_UTF8("Français") },
            { 3, "es", juce::CharPointer_UTF8("Español") },
            { 4, "de", "Deutsch" },
            { 5, "it", "Italiano" },
            { 6, "pt", juce::CharPointer_UTF8("Português") },
        };
        return options;
    }
}

LanguageSettings::LanguageSettings(const std::string& identifier):
    Component(identifier)
{
    _title.setFontSize(nui::Theme::SMALL);
    _title.setColor(nui::Theme::ThemeColor::DISABLED);
    _title.setJustificationType(juce::Justification::centredLeft);

    const auto& languageOptions = getLanguageOptions();
    for (const auto& option : languageOptions)
        _languagePicker.addItem(option.label, option.id);

    const auto currentCode = AppSettings::getInstance().getLanguage();
    const auto currentOption = std::find_if(languageOptions.begin(), languageOptions.end(),
        [&currentCode](const LanguageOption& option) { return option.code == currentCode; });
    _languagePicker.setSelectedId(currentOption != languageOptions.end() ? currentOption->id : languageOptions.front().id, juce::dontSendNotification);

    _languagePicker.addOnValueChangedListener(this);
    _languagePicker.setSelectedInvertedTextColor(true);
    _languagePicker.setHeightType(nui::Theme::HeightType::THIN);

    AppLocalisation::getChangeBroadcaster().addChangeListener(this);

    _layout.setGap(8.f);
    _layout.setDisplayGrid(false);
    _layout.init({ 1, 1, 1 }, { 1, 4 });

    _layout.setFixedRowHeight(0, 32.f);

    _layout.addComponent(_title, 0, 0, 2, 1);
    _layout.addComponent(LANGUAGE_PICKER_ID, _languagePicker, 1, 0, 2, 1);

    _layout.setBottomBorder(_title.getComponentID().toStdString(), nui::Theme::newColor(nui::Theme::ThemeColor::BORDER).asJuce());
}

LanguageSettings::~LanguageSettings()
{
    _languagePicker.removeListener(this);
    AppLocalisation::getChangeBroadcaster().removeChangeListener(this);
}

void LanguageSettings::paint(juce::Graphics& g)
{
    Component::paint(g);

    _layout.paint(g);
}

void LanguageSettings::resized()
{
    Component::resized();

    _layout.resized();
}

float LanguageSettings::getControlX(const juce::Rectangle<float>& cell, float controlWidth)
{
    return CONTROL_ALIGNMENT == ControlAlignment::Right ? cell.getRight() - controlWidth : cell.getX();
}

void LanguageSettings::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    Component::changeListenerCallback(source);

    if (source == &nui::Theme::getChangeBroadcaster())
    {
        _layout.setBottomBorder(_title.getComponentID().toStdString(), nui::Theme::newColor(nui::Theme::ThemeColor::BORDER).asJuce());
        repaint();
    }

    if (source != &AppLocalisation::getChangeBroadcaster())
        return;

    _title.setText(juce::translate("language_settings_title").toStdString());
    repaint();
}

void LanguageSettings::onSelectionChanged(const std::string& componentID, int selectedId)
{
    (void) componentID;

    const auto& options = getLanguageOptions();
    const auto selectedOption = std::find_if(options.begin(), options.end(),
        [selectedId](const LanguageOption& option) { return option.id == selectedId; });
    const std::string code = selectedOption != options.end() ? selectedOption->code : "en";

    AppSettings::getInstance().setLanguage(code);
    AppLocalisation::setLanguage(code);
}

}
