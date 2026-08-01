#pragma once

#include <vector>

#include <juce_data_structures/juce_data_structures.h>

#include "Theory/ProgressionPreset.h"

namespace theory
{

// Combined registry of built-in (see BuiltInProgressionPresets) and user-saved progression
// presets. User presets persist across every plugin instance and DAW project on the machine,
// exactly like AppSettings - same juce::PropertiesFile + juce::InterProcessLock pattern, its own
// file in AppSettings::getAppSupportDirectory().
class ProgressionPresetLibrary
{
public:
    // Exposed (rather than only reachable via getInstance()) so tests can point a library at a
    // throwaway temp file instead of the real per-app user-presets file - same pattern as AppSettings.
    explicit ProgressionPresetLibrary(const juce::File& userPresetsFile);

    static ProgressionPresetLibrary& getInstance();

    // Built-in presets first, then user-saved ones, in save order.
    [[nodiscard]] std::vector<ProgressionPreset> getAllPresets() const;
    [[nodiscard]] const std::vector<ProgressionPreset>& getUserPresets() const { return _userPresets; }

    // Adds a new user preset, or overwrites an existing one with the same id. Persists to disk
    // immediately. No-op (silently ignored) if preset.id doesn't look like a user preset id.
    void savePreset(const ProgressionPreset& preset);

    // No-op for built-in preset ids - only ever removes from the user-saved list.
    void deletePreset(const std::string& id);

private:
    void loadUserPresets();
    void persistUserPresets();

    juce::InterProcessLock _processLock { "chords-theory-user-presets" };
    juce::PropertiesFile _propertiesFile;

    std::vector<ProgressionPreset> _userPresets;

    JUCE_DECLARE_NON_COPYABLE(ProgressionPresetLibrary)
};

}
