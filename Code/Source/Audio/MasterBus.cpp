#include "Audio/MasterBus.h"

namespace audio
{

void MasterBus::prepare(double sampleRate, int samplesPerBlock, int numChannels)
{
    _compressor.prepare(sampleRate, samplesPerBlock, numChannels);

    const juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32>(samplesPerBlock), static_cast<juce::uint32>(numChannels) };
    _panner.prepare(spec);
    _panner.setRule(juce::dsp::Panner<float>::Rule::squareRoot3dB);
}

void MasterBus::reset()
{
    _compressor.reset();
    _panner.reset();
}

void MasterBus::setBlockParameters(const BlockParameters& parameters) noexcept
{
    _compressor.setBlockParameters({ .amountPercent = parameters.compressorAmountPercent });
    _panner.setPan(juce::jlimit(-1.0f, 1.0f, parameters.panPercent / 100.0f));
    _outputGain = juce::Decibels::decibelsToGain(parameters.outputDb, -100.0f);
}

void MasterBus::process(juce::AudioBuffer<float>& buffer) noexcept
{
    _compressor.process(buffer);

    // Panner::process only acts when the output block has exactly 2 channels (its own internal
    // guard) - skip it entirely for anything else rather than relying on that silent no-op.
    if (buffer.getNumChannels() == 2)
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        _panner.process(context);
    }

    buffer.applyGain(_outputGain);
}

}
