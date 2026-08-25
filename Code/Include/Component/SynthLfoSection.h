#pragma once

#include <string>

#include <nierika_dsp/nierika_dsp.h>

#include "SynthSection.h"

namespace component
{

class SynthLfoSection : public SynthSection
{
public:
    SynthLfoSection(const std::string& identifier, ndsp::ParameterManager& parameterManager);
    ~SynthLfoSection() override;

protected:
    std::string getSectionName() override;
    void refreshLabels() override;

private:
    void onToggleValueChanged(const std::string& toggleID, bool isOn) override;

    nelement::Cycler _shapeCycler;
    nelement::LFOCurve _lfoCurve;
    nelement::SVGToggle _modeToggle;
    nelement::SVGToggle _syncToggle;
    nelement::TimingDial _syncDivisionDial;
    nelement::Dial _rateDial;
    nelement::PercentageDial _smoothDial;
    nelement::TimeInMsDial _delayDial;
    nelement::PercentageDial _stereoDial;
    nelement::PercentageDial _depthDial;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthLfoSection)
};

}
