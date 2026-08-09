#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <nierika_dsp/nierika_dsp.h>

class AppSettings
{
public:
    explicit AppSettings(const juce::File& settingsFile);

    // Standalone-only: whether the app name is drawn centered over the native title bar (see
    // StandaloneApp.cpp) - has no effect on AU/VST3/AUv3, which don't have a title bar at all.
    [[nodiscard]] bool getShowStandaloneTitle() const;
    void setShowStandaloneTitle(bool show);

    [[nodiscard]] nui::Theme::Mode getThemeMode() const;
    void setThemeMode(nui::Theme::Mode mode);

    [[nodiscard]] int getOutputTrimDb() const;
    void setOutputTrimDb(int db);

    [[nodiscard]] std::string getLanguage() const;
    void setLanguage(const std::string& languageCode);

    static AppSettings& getInstance();

    // Writable per-app support directory (~/Library/<CompanyName>/<AppName> on macOS) this app
    // uses for all local, writable state - shared by settings storage and any other per-app
    // writable files a downstream plugin might add.
    [[nodiscard]] static juce::File getAppSupportDirectory();

private:
    juce::InterProcessLock _processLock { "chords-theory-settings" };
    juce::PropertiesFile _properties;
};
