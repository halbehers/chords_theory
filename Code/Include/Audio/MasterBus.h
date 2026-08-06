#pragma once

#include <juce_dsp/juce_dsp.h>

#include "Audio/MasterCompressor.h"

namespace audio
{

// The master signal chain, applied once per block to the already-summed stereo mix (see
// ChordSynthEngine::renderNextBlock): Compressor -> Pan -> Output gain, in that order.
class MasterBus
{
public:
    struct BlockParameters
    {
        float compressorAmountPercent = 0.0f;
        float panPercent = 0.0f; // -100..100
        float outputDb = 0.0f;
    };

    void prepare(double sampleRate, int samplesPerBlock, int numChannels);
    void reset();
    void setBlockParameters(const BlockParameters& parameters) noexcept;
    void process(juce::AudioBuffer<float>& buffer) noexcept;

private:
    MasterCompressor _compressor;
    juce::dsp::Panner<float> _panner; // equal-power (squareRoot3dB) law - the correct choice for a single, important master control, unlike Oscillator's deliberately-simple linear unison pan
    float _outputGain = 1.0f;
};

}
