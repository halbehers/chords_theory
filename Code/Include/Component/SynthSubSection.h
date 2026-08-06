#pragma once

#include <string>

#include <nierika_dsp/nierika_dsp.h>

#include "SynthSection.h"

namespace component
{

class SynthSubSection : public SynthSection
{
public:
    SynthSubSection(const std::string& identifier, ndsp::ParameterManager& parameterManager, const std::string& enabledParameterID);
    ~SynthSubSection() override;

protected:
    std::string getSectionName() override;
    void refreshLabels() override;

private:
    nelement::Dial _octaveDial;
    nelement::Dial _transposeDial;
    nelement::PercentageDial _toneDial;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthSubSection)
};

}
