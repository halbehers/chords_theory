#pragma once

#include <vector>

#include <nierika_dsp/nierika_dsp.h>

#include "Theory/ProgressionPreset.h"
#include "Theory/Scale.h"

namespace component
{

// ComboBox listing every progression preset (built-in, then user-saved - see
// ProgressionPresetLibrary) applicable to the current scale. Presets that reference a degree the
// current scale doesn't have (e.g. a generic preset while Minor Blues is selected) are filtered
// out entirely rather than shown disabled - nelement::ComboBox has no per-item disabled state to
// build on.
class ProgressionPresetPicker : public nui::Component, public nelement::ComboBox::OnValueChangedListener
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void onPresetSelected(const theory::ProgressionPreset& preset) = 0;
    };

    explicit ProgressionPresetPicker(const std::string& identifier);
    ~ProgressionPresetPicker() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // Repopulates the list for the given scale - call whenever Key/Scale changes, and whenever a
    // preset is saved/deleted so newly-saved presets show up immediately.
    void refreshForScale(theory::Scale scale);

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

private:
    void onSelectionChanged(const std::string& componentID, int selectedId) override;

    nelement::ComboBox _picker { "progression-preset-picker" };
    std::vector<theory::ProgressionPreset> _visiblePresets; // combobox item id (1-based) == index + 1
    theory::Scale _currentScale = theory::Scale::Major;

    nlayout::GridLayout<nui::Component> _layout { *this };

    std::vector<Listener*> _listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProgressionPresetPicker)
};

}
