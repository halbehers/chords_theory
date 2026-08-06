#pragma once

#include <vector>

#include <nierika_dsp/nierika_dsp.h>

#include "Theory/Key.h"
#include "Theory/Scale.h"

namespace component
{

// Two comboboxes (Key, Scale) driving the rest of the plugin's UI. ComboBox item ids are the
// enum's ordinal + 1 (juce::ComboBox reserves id 0 for "no selection").
class KeyScaleSelector : public nui::Component, public nelement::ComboBox::OnValueChangedListener
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void onKeyScaleChanged(theory::Key key, theory::Scale scale) = 0;
    };

    explicit KeyScaleSelector(const std::string& identifier);
    ~KeyScaleSelector() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    [[nodiscard]] theory::Key getKey() const { return _currentKey; }
    [[nodiscard]] theory::Scale getScale() const { return _currentScale; }

    // Sets the pickers' state without notifying listeners - used to restore saved session state.
    void setKeyAndScale(theory::Key key, theory::Scale scale);

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

private:
    void onSelectionChanged(const std::string& componentID, int selectedId) override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void notifyListeners();

    nelement::Text _keyLabel { "key-scale-selector-key-label" };
    nelement::ComboBox _keyPicker { "key-scale-selector-key-picker" };
    nelement::Text _scaleLabel { "key-scale-selector-scale-label" };
    nelement::ComboBox _scalePicker { "key-scale-selector-scale-picker" };

    theory::Key _currentKey = theory::Key::C;
    theory::Scale _currentScale = theory::Scale::Major;

    nlayout::GridLayout<nui::Component> _layout { *this };

    std::vector<Listener*> _listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeyScaleSelector)
};

}
