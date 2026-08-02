#pragma once

#include <string>

#include <nierika_dsp/nierika_dsp.h>

#include "SynthSection.h"

namespace component
{

class SynthAdsrSection : public SynthSection
{
public:
    SynthAdsrSection(const std::string& identifier, ndsp::ParameterManager& parameterManager);
    ~SynthAdsrSection() override;

protected:
    std::string getSectionName() override;
    void refreshLabels() override;

private:
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
