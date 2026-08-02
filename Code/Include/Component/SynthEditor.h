#pragma once

#include <string>

#include <nierika_dsp/nierika_dsp.h>

#include "Component/SynthAdsrSection.h"
#include "Component/SynthFilterSection.h"
#include "Component/SynthLfoSection.h"
#include "Component/SynthOscillatorSection.h"

namespace component
{

class SynthEditor : public nui::Component
{
public:
    explicit SynthEditor(ndsp::ParameterManager& parameterManager);
    ~SynthEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    SynthOscillatorSection _oscillator1Section;
    SynthOscillatorSection _oscillator2Section;
    SynthAdsrSection _adsrSection;
    SynthLfoSection _lfoSection;
    SynthFilterSection _filterSection;

    nlayout::GridLayout<nui::Component> _layout { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthEditor)
};

}
