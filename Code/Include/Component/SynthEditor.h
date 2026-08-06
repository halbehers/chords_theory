#pragma once

#include <string>

#include <nierika_dsp/nierika_dsp.h>

#include "Component/SynthAdsrSection.h"
#include "Component/SynthFilterSection.h"
#include "Component/SynthLfoSection.h"
#include "Component/SynthMixerSection.h"
#include "Component/SynthOscillatorSection.h"
#include "Component/SynthOutputSection.h"
#include "Component/SynthSubSection.h"

namespace component
{

class SynthEditor : public nui::Component
{
public:
    SynthEditor(ndsp::ParameterManager& parameterManager,
                ndsp::SingleChannelSampleFIFO<juce::AudioBuffer<float>>* leftWaveformFifo,
                ndsp::SingleChannelSampleFIFO<juce::AudioBuffer<float>>* rightWaveformFifo);
    ~SynthEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    SynthSubSection _subSection;
    SynthOscillatorSection _oscillator1Section;
    SynthOscillatorSection _oscillator2Section;
    SynthMixerSection _mixerSection;
    SynthAdsrSection _adsrSection;
    SynthLfoSection _lfoSection;
    SynthFilterSection _filterSection;
    SynthOutputSection _outputSection;

    nlayout::GridLayout<nui::Component> _layout { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthEditor)
};

}
