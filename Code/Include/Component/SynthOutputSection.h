#pragma once

#include <string>

#include <nierika_dsp/nierika_dsp.h>

#include "SynthSection.h"

namespace component
{

// The plugin's one master-bus section (see MasterBus/MasterCompressor) - no enabledParameterID
// (no power icon, matches Mixer/Filter), since there is no meaningful "bypass the whole output"
// toggle distinct from the individual controls already defaulting to no-ops.
class SynthOutputSection : public SynthSection
{
public:
    SynthOutputSection(const std::string& identifier, ndsp::ParameterManager& parameterManager,
                        ndsp::SingleChannelSampleFIFO<juce::AudioBuffer<float>>* leftWaveformFifo,
                        ndsp::SingleChannelSampleFIFO<juce::AudioBuffer<float>>* rightWaveformFifo);
    ~SynthOutputSection() override;

protected:
    std::string getSectionName() override;
    void refreshLabels() override;

private:
    nelement::CircularStereoWaveform _waveform;
    nelement::Dial _tuningDial;
    nelement::PercentageDial _compressorAmountDial;
    nelement::Dial _panDial;
    nelement::Dial _outputDial;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthOutputSection)
};

}
