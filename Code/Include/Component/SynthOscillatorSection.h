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
                            const std::string& parameterIdPrefix, const std::string& sectionTitleKey);
    ~SynthOscillatorSection() override;

protected:
    std::string getSectionName() override;
    void refreshLabels() override;

private:
    std::string _sectionTitleKey;

    nelement::Cycler _shapeCycler;
    nelement::OscillatorCurve _curve;
    nelement::PercentageDial _shapeNoiseDial;
    nelement::Dial _octaveDial;
    nelement::Dial _detuneDial;
    nelement::Dial _warpDial;
    nelement::PercentageDial _foldDial;
    nelement::Dial _outputDial;
    nelement::Dial _unisonVoicesDial;
    nelement::Dial _unisonDetuneDial;
    nelement::PercentageDial _unisonStereoDial;
    nelement::PercentageDial _subLevelDial;
    nelement::SVGToggle _subOctaveToggle;
    nelement::SVGToggle _phaseRandomizeToggle;
    nelement::PercentageDial _phaseDial;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthOscillatorSection)
};

}
