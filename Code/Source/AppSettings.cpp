#include "AppSettings.h"

namespace
{
    const char* SHOW_STANDALONE_TITLE_KEY = "showStandaloneTitle";
    const char* THEME_MODE_KEY = "themeMode";
    const char* LANGUAGE_KEY = "language";

    const char* THEME_MODE_LIGHT = "light";
    const char* THEME_MODE_DARK = "dark";

    juce::String getAppName()
    {
        if (auto* app = juce::JUCEApplication::getInstance())
            return app->getApplicationName();

        const auto pluginName = juce::PluginDescription().name;
        if (pluginName.isNotEmpty())
            return pluginName;

        return "Default";
    }

    juce::File getDefaultSettingsFile()
    {
        return AppSettings::getAppSupportDirectory().getChildFile("settings.xml");
    }

    juce::PropertiesFile::Options makeOptions(juce::InterProcessLock& processLock)
    {
        juce::PropertiesFile::Options options;
        options.storageFormat = juce::PropertiesFile::storeAsXML;
        options.processLock = &processLock;
        return options;
    }
}

AppSettings::AppSettings(const juce::File& settingsFile):
    _properties(settingsFile, makeOptions(_processLock))
{
}

bool AppSettings::getShowStandaloneTitle() const
{
    return _properties.getBoolValue(SHOW_STANDALONE_TITLE_KEY, true);
}

void AppSettings::setShowStandaloneTitle(bool show)
{
    _properties.setValue(SHOW_STANDALONE_TITLE_KEY, show);
    _properties.save();
}

nui::Theme::Mode AppSettings::getThemeMode() const
{
    return _properties.getValue(THEME_MODE_KEY, THEME_MODE_DARK) == THEME_MODE_LIGHT
        ? nui::Theme::Mode::LIGHT
        : nui::Theme::Mode::DARK;
}

void AppSettings::setThemeMode(nui::Theme::Mode mode)
{
    _properties.setValue(THEME_MODE_KEY, mode == nui::Theme::Mode::LIGHT ? THEME_MODE_LIGHT : THEME_MODE_DARK);
    _properties.save();
}

std::string AppSettings::getLanguage() const
{
    return _properties.getValue(LANGUAGE_KEY, "en").toStdString();
}

void AppSettings::setLanguage(const std::string& languageCode)
{
    _properties.setValue(LANGUAGE_KEY, juce::String(languageCode));
    _properties.save();
}

AppSettings& AppSettings::getInstance()
{
    static AppSettings& instance = *new AppSettings(getDefaultSettingsFile());
    return instance;
}

juce::File AppSettings::getAppSupportDirectory()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile(COMPANY_NAME)
        .getChildFile(getAppName());
}
