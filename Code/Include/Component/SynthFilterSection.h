#pragma once

#include <string>

#include <nierika_dsp/nierika_dsp.h>

namespace component
{

// One "module" of the Synth tab's grid (see SynthEditor) - the filter: Type/Slope cyclers, a
// draggable FilterResponseCurve display, and Cutoff/Resonance/Drive/Mix/Key-Track knobs, all
// APVTS-attached directly to Parameters::FILTER_*.
class SynthFilterSection : public nui::Section
{
public:
    SynthFilterSection(const std::string& identifier, ndsp::ParameterManager& parameterManager);
    ~SynthFilterSection() override;

private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void refreshDialLabels();

    nelement::Cycler _typeCycler;
    nelement::Cycler _slopeCycler;
    nelement::FilterResponseCurve _responseCurve;
    nelement::FrequencyDial _cutoffDial;
    nelement::Dial _resonanceDial;
    nelement::Dial _driveDial;
    nelement::PercentageDial _mixDial;
    nelement::PercentageDial _keyTrackDial;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthFilterSection)
};

}
