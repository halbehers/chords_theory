#include "Audio/DahdsrEnvelope.h"

#include <juce_core/juce_core.h>

namespace audio
{

void DahdsrEnvelope::setSampleRate(double sampleRate) noexcept
{
    _sampleRate = sampleRate;
}

void DahdsrEnvelope::setParameters(const Parameters& parameters) noexcept
{
    _parameters = parameters;

    _delaySamples = millisecondsToSamples(parameters.delayMs);
    _attackSamples = millisecondsToSamples(parameters.attackMs);
    _holdSamples = millisecondsToSamples(parameters.holdMs);
    _decaySamples = millisecondsToSamples(parameters.decayMs);
    _releaseSamples = millisecondsToSamples(parameters.releaseMs);
}

void DahdsrEnvelope::reset() noexcept
{
    _stage = Stage::Idle;
    _value = 0.0f;
    _stageSampleIndex = 0;
    _stageLengthSamples = 0;
}

void DahdsrEnvelope::noteOn() noexcept
{
    _value = 0.0f;
    enterStage(Stage::Delay);
}

void DahdsrEnvelope::noteOff() noexcept
{
    if (_stage == Stage::Idle || _stage == Stage::Release)
        return;

    enterStage(Stage::Release);
}

float DahdsrEnvelope::getNextSample() noexcept
{
    switch (_stage)
    {
        case Stage::Idle:
            return 0.0f;

        case Stage::Sustain:
            _value = _parameters.sustainLevel;
            return _value;

        case Stage::Delay:
        case Stage::Attack:
        case Stage::Hold:
        case Stage::Decay:
        case Stage::Release:
        default:
        {
            ++_stageSampleIndex;
            const auto t = static_cast<float>(_stageSampleIndex) / static_cast<float>(_stageLengthSamples);
            _value = _stageStartValue + (_stageEndValue - _stageStartValue) * t;

            if (_stageSampleIndex >= _stageLengthSamples)
                enterStage(nextStage(_stage));

            return _value;
        }
    }
}

void DahdsrEnvelope::enterStage(Stage stage) noexcept
{
    _stage = stage;
    _stageSampleIndex = 0;

    switch (stage)
    {
        case Stage::Delay:
            _stageLengthSamples = _delaySamples;
            _stageStartValue = 0.0f;
            _stageEndValue = 0.0f;
            break;
        case Stage::Attack:
            _stageLengthSamples = _attackSamples;
            _stageStartValue = _value;
            _stageEndValue = 1.0f;
            break;
        case Stage::Hold:
            _stageLengthSamples = _holdSamples;
            _stageStartValue = 1.0f;
            _stageEndValue = 1.0f;
            break;
        case Stage::Decay:
            _stageLengthSamples = _decaySamples;
            _stageStartValue = 1.0f;
            _stageEndValue = _parameters.sustainLevel;
            break;
        case Stage::Sustain:
            _stageLengthSamples = 0;
            _value = _parameters.sustainLevel;
            return;
        case Stage::Release:
            _stageLengthSamples = _releaseSamples;
            _stageStartValue = _value;
            _stageEndValue = 0.0f;
            break;
        case Stage::Idle:
        default:
            _stageLengthSamples = 0;
            _value = 0.0f;
            return;
    }

    if (_stageLengthSamples <= 0)
    {
        // Zero-duration stage - jump straight to the end value and chain into the next stage
        // immediately, within the same call, so noteOn()/noteOff() never "lose" a sample to a
        // zero-length stage (e.g. today's decay=0/hold=0 defaults must behave exactly as before).
        _value = _stageEndValue;
        enterStage(nextStage(stage));
    }
}

int DahdsrEnvelope::millisecondsToSamples(float ms) const noexcept
{
    return juce::jmax(0, static_cast<int>(static_cast<double>(ms) * 0.001 * _sampleRate));
}

DahdsrEnvelope::Stage DahdsrEnvelope::nextStage(Stage stage) noexcept
{
    switch (stage)
    {
        case Stage::Delay: return Stage::Attack;
        case Stage::Attack: return Stage::Hold;
        case Stage::Hold: return Stage::Decay;
        case Stage::Decay: return Stage::Sustain;
        case Stage::Release: return Stage::Idle;
        case Stage::Sustain:
        case Stage::Idle:
        default:
            return Stage::Idle;
    }
}

}
