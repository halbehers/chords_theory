#pragma once

#include <string>

#include <nierika_dsp/nierika_dsp.h>

#include "SynthSection.h"

namespace component
{

class SynthOscillatorSection : public SynthSection
{
public:
    SynthOscillatorSection(const std::string& identifier, ndsp::ParameterManager& parameterManager,
                            const std::string& parameterIdPrefix, const std::string& sectionTitleKey,
                            const std::string& enabledParameterID = "");
    ~SynthOscillatorSection() override;

protected:
    std::string getSectionName() override;
    void refreshLabels() override;

private:
    std::string _sectionTitleKey;

    nelement::Cycler _shapeCycler;
    nelement::OscillatorCurveWithParameters _curve;
    nelement::Dial _octaveDial;
    nelement::Dial _detuneDial;
    nelement::Dial _transposeDial;
    nelement::Dial _unisonVoicesDial;
    nelement::Dial _unisonDetuneDial;
    nelement::PercentageDial _unisonStereoDial;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthOscillatorSection)
};

}
