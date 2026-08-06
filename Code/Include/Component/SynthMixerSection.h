#pragma once

#include <string>

#include <nierika_dsp/nierika_dsp.h>

#include "SynthSection.h"

namespace component
{

class SynthMixerSection : public SynthSection, public nelement::Cycler::OnValueChangedListener
{
public:
    SynthMixerSection(const std::string& identifier, ndsp::ParameterManager& parameterManager);
    ~SynthMixerSection() override;

    void onSelectionChanged(const std::string& componentID, int selectedIndex) override;

protected:
    std::string getSectionName() override;
    void refreshLabels() override;

private:
    // Shows/hides whichever of the four ValueSetters below matches selectedIndex - all four hidden
    // for Add (index 0), which has no parameter of its own.
    void updateAlgorithmParameterVisibility(int selectedIndex);

    nelement::Cycler _algorithmCycler;

    nelement::ValueSetter _fmAmountSetter;
    nelement::ValueSetter _ringModMixSetter;
    nelement::ValueSetter _amDepthSetter;
    nelement::ValueSetter _serialFoldAmountSetter;

    nelement::VerticalSlider _subLevelSlider;
    nelement::VerticalSlider _osc1LevelSlider;
    nelement::VerticalSlider _osc2LevelSlider;

    // Osc1 has no enable/disable toggle at all (see SynthOscillatorSection), so its slider always
    // stays enabled - only Sub/Osc2 need this. Independent attachments to the same sub-enabled/
    // osc2-enabled parameters SynthSubSection/SynthOscillatorSection themselves already bypass
    // from, rather than reaching sideways into those sibling components directly.
    std::unique_ptr<juce::ParameterAttachment> _subEnabledAttachment;
    std::unique_ptr<juce::ParameterAttachment> _osc2EnabledAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthMixerSection)
};

}
