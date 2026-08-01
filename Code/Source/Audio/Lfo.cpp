#include "Audio/Lfo.h"

#include <cmath>

namespace audio
{

void Lfo::setSampleRate(double sampleRate) noexcept
{
    _sampleRate = sampleRate;
}

void Lfo::setParameters(const Parameters& parameters, double hostBpm, double freePhaseSeed01) noexcept
{
    _parameters = parameters;

    const double rateHz = parameters.syncEnabled
        ? static_cast<double>(ndsp::Timing::getRate(hostBpm, parameters.syncDivision))
        : parameters.freeRateHz;

    _phaseIncrementPerSample = _sampleRate > 0.0 ? rateHz / _sampleRate : 0.0;
    _delaySamples = juce::jmax(0, static_cast<int>(static_cast<double>(parameters.delayMs) * 0.001 * _sampleRate));

    if (parameters.mode == Mode::Free)
        _freePhase01 = freePhaseSeed01;
}

void Lfo::noteOn() noexcept
{
    _triggerPhase01 = 0.0;
    _samplesSinceNoteOn = 0;
}

void Lfo::noteOff() noexcept
{
    // Intentionally empty - see class doc comment.
}

Lfo::StereoSample Lfo::getNextSample() noexcept
{
    auto& phase = (_parameters.mode == Mode::Free) ? _freePhase01 : _triggerPhase01;

    const auto basePhase01 = phase;
    phase += _phaseIncrementPerSample;
    if (phase >= 1.0)
        phase -= std::floor(phase);

    const auto isDelayed = _samplesSinceNoteOn < _delaySamples;
    ++_samplesSinceNoteOn;

    if (isDelayed)
        return {};

    const auto stereoOffset01 = static_cast<double>(juce::jlimit(0.0f, 100.0f, _parameters.stereoPercent)) / 100.0 * 0.5;

    const auto wrap01 = [](double p) { return p - std::floor(p); };

    const auto leftPhase = wrap01(basePhase01);
    const auto rightPhase = wrap01(basePhase01 + stereoOffset01);

    return { computeShapeValue(leftPhase), computeShapeValue(rightPhase) };
}

float Lfo::computeShapeValue(double phase01) const noexcept
{
    double raw;

    switch (_parameters.shape)
    {
        case Shape::Triangle:
            raw = 2.0 * std::abs(2.0 * (phase01 - std::floor(phase01 + 0.5))) - 1.0;
            break;
        case Shape::Saw:
            raw = 2.0 * phase01 - 1.0;
            break;
        case Shape::Square:
            raw = phase01 < 0.5 ? 1.0 : -1.0;
            break;
        case Shape::Sine:
        default:
            raw = std::sin(juce::MathConstants<double>::twoPi * phase01);
            break;
    }

    const auto sine = std::sin(juce::MathConstants<double>::twoPi * phase01);
    const auto smooth = static_cast<double>(juce::jlimit(0.0f, 1.0f, _parameters.smooth));

    return static_cast<float>(raw * (1.0 - smooth) + sine * smooth);
}

}
