#pragma once

#include <string>

#include <nierika_dsp/nierika_dsp.h>

namespace component
{

// One "module" of the Synth tab's grid (see SynthEditor) - the DAHDSR envelope: a draggable
// EnvelopeCurve display plus six knobs, all APVTS-attached directly to Parameters::ENVELOPE_*.
class SynthAdsrSection : public nui::Section
{
public:
    SynthAdsrSection(const std::string& identifier, ndsp::ParameterManager& parameterManager);
    ~SynthAdsrSection() override;

private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void refreshDialLabels();

    nelement::EnvelopeCurve _envelopeCurve;
    nelement::TimeInMsDial _delayDial;
    nelement::TimeInMsDial _attackDial;
    nelement::TimeInMsDial _holdDial;
    nelement::TimeInMsDial _decayDial;
    nelement::PercentageDial _sustainDial;
    nelement::TimeInMsDial _releaseDial;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthAdsrSection)
};

}
