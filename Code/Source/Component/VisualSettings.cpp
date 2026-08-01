#include "Component/VisualSettings.h"
#include "AppSettings.h"
#include "AppLocalisation.h"

namespace component
{

VisualSettings::VisualSettings(const std::string& identifier):
    Component(identifier)
{
    _title.setFontSize(nui::Theme::SMALL);
    _title.setColor(nui::Theme::ThemeColor::DISABLED);
    _title.setJustificationType(juce::Justification::centredLeft);

    _themeLabel.setJustificationType(juce::Justification::centredLeft);

    _themeSwitch.addOnValueChangedListener(this);
    _themeSwitch.setSelectedIndex(AppSettings::getInstance().getThemeMode() == nui::Theme::Mode::LIGHT ? 1 : 0, juce::dontSendNotification);
    _themeSwitch.setSelectedInvertedTextColor(true);
    _themeSwitch.setHeightType(nui::Theme::HeightType::THIN);
    _themeSwitch.setRounded(true);

    AppLocalisation::getChangeBroadcaster().addChangeListener(this);

    _layout.setGap(8.f);
    _layout.setDisplayGrid(false);
    _layout.init({ 1, 1 }, { 1, 4 });

    _layout.setFixedRowHeight(0, 32.f);

    _layout.addComponent(_title, 0, 0, 2, 1);
    _layout.addComponent(_themeLabel, 1, 0, 2, 1);
    _layout.addComponent(_themeSwitch, 1, 1, 1, 1, 10);

    _layout.setBottomBorder(_title.getComponentID().toStdString(), nui::Theme::newColor(nui::Theme::ThemeColor::BORDER).asJuce());
}

VisualSettings::~VisualSettings()
{
    _themeSwitch.removeListener(this);
    AppLocalisation::getChangeBroadcaster().removeChangeListener(this);
}

void VisualSettings::paint(juce::Graphics& g)
{
    Component::paint(g);

    _layout.paint(g);
}

void VisualSettings::resized()
{
    Component::resized();

    _layout.resized();
}

void VisualSettings::onSelectionChanged(const std::string& componentID, int selectedIndex)
{
    if (componentID != _themeSwitch.getComponentID())
        return;

    const auto mode = selectedIndex == 1 ? nui::Theme::Mode::LIGHT : nui::Theme::Mode::DARK;
    AppSettings::getInstance().setThemeMode(mode);
    nui::Theme::setMode(mode);

    if (auto* top = getTopLevelComponent())
        top->repaint();
}

void VisualSettings::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    Component::changeListenerCallback(source);

    if (source == &nui::Theme::getChangeBroadcaster())
    {
        _layout.setBottomBorder(_title.getComponentID().toStdString(), nui::Theme::newColor(nui::Theme::ThemeColor::BORDER).asJuce());
        repaint();
    }

    if (source != &AppLocalisation::getChangeBroadcaster())
        return;

    _title.setText(juce::translate("visual_settings_title").toStdString());
    _themeLabel.setText(juce::translate("visual_settings_theme_label").toStdString());
    _themeSwitch.setLabels(juce::translate("visual_settings_dark_theme").toStdString(), juce::translate("visual_settings_light_theme").toStdString());

    repaint();
}

}
