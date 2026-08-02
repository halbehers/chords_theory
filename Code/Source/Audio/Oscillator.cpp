#include "Audio/Oscillator.h"

#include <cmath>

namespace audio
{

namespace
{
    constexpr float kOscMinActiveWidth = 0.02f; // Warp's minimum active-window width - avoids a
                                                 // divide-by-zero/infinite-frequency edge case as
                                                 // |warp| approaches 100%.
    constexpr float kOscFoldMaxGain = 6.0f;     // tune-by-ear pre-gain multiplier at fold == 100%

    // Classic two-sample PolyBLEP correction (Valimaki/Huovilainen) - t is the phase in [0,1) at
    // the discontinuity's own local rate, dt is that rate's per-sample phase increment. Clamped to
    // 0.5 so Warp's phase-squeeze (which can inflate the local dt well past 1 at extreme settings)
    // degrades gracefully instead of producing a mathematically inverted correction region.
    float polyBlep(double t, double dt) noexcept
    {
        const auto clampedDt = juce::jlimit(1.0e-9, 0.5, dt);

        if (t < clampedDt)
        {
            const auto x = t / clampedDt;
            return static_cast<float>(x + x - x * x - 1.0);
        }
        if (t > 1.0 - clampedDt)
        {
            const auto x = (t - 1.0) / clampedDt;
            return static_cast<float>(x * x + x + x + 1.0);
        }
        return 0.0f;
    }
}

void Oscillator::setSampleRate(double sampleRate) noexcept
{
    _sampleRate = sampleRate;
}

void Oscillator::setBlockParameters(const Parameters& parameters) noexcept
{
    _parameters = parameters;
    recomputePhaseIncrements();
}

void Oscillator::noteOn(int midiNoteNumber) noexcept
{
    _baseFrequencyHz = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    recomputePhaseIncrements();

    const auto startPhase01 = static_cast<double>(juce::jlimit(0.0f, 100.0f, _parameters.phasePercent)) / 100.0;

    for (auto& voice : _unisonVoices)
        voice.phase01 = _parameters.phaseRandomizeEnabled ? static_cast<double>(voice.random.nextFloat()) : startPhase01;

    _subPhase01 = _parameters.phaseRandomizeEnabled ? static_cast<double>(_subRandom.nextFloat()) : startPhase01;
}

void Oscillator::recomputePhaseIncrements() noexcept
{
    if (_sampleRate <= 0.0)
        return;

    const auto octaveMultiplier = std::pow(2.0, static_cast<double>(_parameters.octave));
    const auto transposeMultiplier = std::pow(2.0, static_cast<double>(_parameters.transposeSemitones) / 12.0);
    const auto detuneMultiplier = std::pow(2.0, static_cast<double>(_parameters.detuneCents) / 1200.0);
    const auto centerFrequencyHz = _baseFrequencyHz * octaveMultiplier * transposeMultiplier * detuneMultiplier;

    const auto numVoices = juce::jlimit(1, kMaxUnisonVoices, _parameters.unisonVoices);

    for (int i = 0; i < kMaxUnisonVoices; ++i)
    {
        // Symmetric spread across -unisonDetuneCents..+unisonDetuneCents, centered at 0 for the
        // middle voice(s) - the classic supersaw layout. Every slot gets a spread offset even
        // beyond numVoices, since it may become active mid-note (see getNextSample).
        const auto voiceOffset01 = numVoices > 1
            ? (static_cast<double>(i) / static_cast<double>(numVoices - 1) - 0.5) * 2.0
            : 0.0;
        const auto voiceDetuneCents = voiceOffset01 * static_cast<double>(_parameters.unisonDetuneCents);
        const auto voiceFrequencyHz = centerFrequencyHz * std::pow(2.0, voiceDetuneCents / 1200.0);

        _unisonVoices[static_cast<std::size_t>(i)].phaseIncrement = voiceFrequencyHz / _sampleRate;
    }

    // Tracks the played note directly (not the main oscillator's own Octave/Detune) - a
    // sub-oscillator is conceptually a fixed offset below the note, independent of whatever
    // tuning tweaks the main shape has.
    const auto subOctaveMultiplier = _parameters.subOctaveDown2 ? 0.25 : 0.5;
    _subPhaseIncrement = (_baseFrequencyHz * subOctaveMultiplier) / _sampleRate;
}

Oscillator::StereoSample Oscillator::getNextSample() noexcept
{
    const auto numVoices = juce::jlimit(1, kMaxUnisonVoices, _parameters.unisonVoices);
    const auto shapeNoiseAmount = juce::jlimit(0.0f, 1.0f, _parameters.shapeNoisePercent / 100.0f);
    const auto stereoSpread01 = juce::jlimit(0.0f, 1.0f, _parameters.unisonStereoPercent / 100.0f);
    const auto foldAmount = juce::jlimit(0.0f, 1.0f, _parameters.foldPercent / 100.0f);
    const auto foldGain = 1.0f + foldAmount * kOscFoldMaxGain;

    float sumLeft = 0.0f;
    float sumRight = 0.0f;

    for (int i = 0; i < kMaxUnisonVoices; ++i)
    {
        auto& voice = _unisonVoices[static_cast<std::size_t>(i)];

        // Every slot's phase keeps advancing regardless of numVoices, so raising unisonVoices
        // mid-note brings in a voice whose phase has been running continuously, not one starting
        // from zero - click-free by construction (see class doc comment).
        const auto currentPhase01 = voice.phase01;
        voice.phase01 += voice.phaseIncrement;
        if (voice.phase01 >= 1.0)
            voice.phase01 -= std::floor(voice.phase01);

        if (i >= numVoices)
            continue;

        auto value = computeWarpedShapeValue(currentPhase01, voice.phaseIncrement);

        if (shapeNoiseAmount > 0.0f)
        {
            const auto noise = voice.random.nextFloat() * 2.0f - 1.0f;
            value = value * (1.0f - shapeNoiseAmount) + noise * shapeNoiseAmount;
        }

        value = foldSample(value * foldGain);

        // Simple linear pan across the unison spread (not an equal-power mix-bus law - this is a
        // unison-width effect, not a channel strip). Voice i's position runs -1..1 across the
        // stack, scaled by stereoSpread01.
        const auto pan = numVoices > 1
            ? (static_cast<float>(i) / static_cast<float>(numVoices - 1) - 0.5f) * 2.0f * stereoSpread01
            : 0.0f;
        const auto leftGain = 1.0f - juce::jmax(0.0f, pan);
        const auto rightGain = 1.0f + juce::jmin(0.0f, pan);

        sumLeft += value * leftGain;
        sumRight += value * rightGain;
    }

    // Gain-compensate so a unison stack's loudness roughly matches a single voice, not
    // numVoices-times louder - standard unison gain-compensation practice.
    const auto normalization = 1.0f / std::sqrt(static_cast<float>(numVoices));
    sumLeft *= normalization;
    sumRight *= normalization;

    const auto subLevel = juce::jlimit(0.0f, 1.0f, _parameters.subLevelPercent / 100.0f);
    if (subLevel > 0.0f)
    {
        const auto subValue = static_cast<float>(std::sin(juce::MathConstants<double>::twoPi * _subPhase01)) * subLevel;
        sumLeft += subValue;
        sumRight += subValue;
    }
    _subPhase01 += _subPhaseIncrement;
    if (_subPhase01 >= 1.0)
        _subPhase01 -= std::floor(_subPhase01);

    const auto outputGain = juce::Decibels::decibelsToGain(_parameters.outputDb, -100.0f);
    sumLeft *= outputGain;
    sumRight *= outputGain;

    // Gated last, after every phase (unison + sub) has still advanced normally above - toggling
    // enabled on/off mid-note is click-free by construction, same principle as the unison-voice-
    // count and Fold/Warp gating elsewhere in this class.
    if (!_parameters.enabled)
        return { 0.0f, 0.0f };

    return { sumLeft, sumRight };
}

float Oscillator::computeShapeValue(double phase01, double phaseIncrement) const noexcept
{
    switch (_parameters.shape)
    {
        case Shape::Triangle:
            // Naive triangle, not PolyBLAMP-corrected: its corner is a slope discontinuity, not a
            // value discontinuity, and aliases far less audibly than Saw/Square's hard jumps.
            return static_cast<float>(2.0 * std::abs(2.0 * (phase01 - std::floor(phase01 + 0.5))) - 1.0);

        case Shape::Saw:
        {
            auto raw = 2.0 * phase01 - 1.0;
            raw -= static_cast<double>(polyBlep(phase01, phaseIncrement));
            return static_cast<float>(raw);
        }

        case Shape::Square:
        {
            auto raw = phase01 < 0.5 ? 1.0 : -1.0;
            raw += static_cast<double>(polyBlep(phase01, phaseIncrement));
            raw -= static_cast<double>(polyBlep(std::fmod(phase01 + 0.5, 1.0), phaseIncrement));
            return static_cast<float>(raw);
        }

        case Shape::Sine:
        default:
            return static_cast<float>(std::sin(juce::MathConstants<double>::twoPi * phase01));
    }
}

float Oscillator::computeWarpedShapeValue(double outerPhase01, double outerPhaseIncrement) const noexcept
{
    const auto warp = juce::jlimit(-1.0f, 1.0f, _parameters.warpPercent / 100.0f);
    const auto activeWidth = juce::jmax(kOscMinActiveWidth, 1.0f - std::abs(warp));

    // warp <= 0 compacts the shape onto the right of the cycle (active window at the end); warp >
    // 0 compacts it onto the left (active window at the start). At warp == 0 both branches agree
    // (activeWidth == 1), so the whole cycle is active - identical to no warp at all.
    const auto activeStart = warp <= 0.0f ? (1.0 - static_cast<double>(activeWidth)) : 0.0;
    const auto activeEnd = activeStart + static_cast<double>(activeWidth);

    if (outerPhase01 < activeStart || outerPhase01 >= activeEnd)
        return 0.0f;

    const auto shapePhase01 = (outerPhase01 - activeStart) / static_cast<double>(activeWidth);
    const auto shapePhaseIncrement = outerPhaseIncrement / static_cast<double>(activeWidth);
    return computeShapeValue(shapePhase01, shapePhaseIncrement);
}

float Oscillator::foldSample(float x) noexcept
{
    // Reflects x back into [-1, 1] using a period-4 triangle-wave folding function - the same
    // technique Vital/Serum's Fold mode and West-coast/Buchla-style wavefolders use. Any value
    // already within [-1, 1] passes through unchanged (foldGain == 1 at foldPercent == 0).
    auto wrapped = std::fmod(x + 1.0f, 4.0f);
    if (wrapped < 0.0f)
        wrapped += 4.0f;
    if (wrapped > 2.0f)
        wrapped = 4.0f - wrapped;
    return wrapped - 1.0f;
}

}
