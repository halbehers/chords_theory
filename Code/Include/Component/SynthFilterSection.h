#pragma once

#include <string>

#include <nierika_dsp/nierika_dsp.h>

#include "SynthSection.h"

namespace component
{

class SynthFilterSection : public SynthSection
{
public:
    SynthFilterSection(const std::string& identifier, ndsp::ParameterManager& parameterManager);
    ~SynthFilterSection() override;

protected:
    std::string getSectionName() override;
    void refreshLabels() override;

private:
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
