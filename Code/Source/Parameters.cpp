#include "Parameters.h"

#include <cmath>

void Parameters::registerPluginParameters(PluginAudioProcessor* audioProcessor)
{
    audioProcessor->registerParameter
           (
               PLUGIN_ENABLED_ID,
               "Plugin Enabled",
               PLUGIN_ENABLED_DEFAULT,
               [](bool) { /* TODO: Implement */ },
               "Bypass the whole plugin."
            );
}

void Parameters::registerOscillatorParameters(PluginAudioProcessor* audioProcessor, const std::string& idPrefix, audio::OscillatorState& state, float outputDbDefault)
{
    auto* statePtr = &state;

    audioProcessor->registerParameter<int>(
        std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { idPrefix + OSC_SHAPE_SUFFIX, 1 }, "Shape",
            juce::StringArray { "Sine", "Triangle", "Saw", "Square" }, OSC_SHAPE_DEFAULT),
        ndsp::IParameter::TYPE_INT, OSC_SHAPE_DEFAULT, 0, 3,
        [statePtr](int value) { statePtr->shape.store(value, std::memory_order_relaxed); },
        "Oscillator waveform shape.");

    audioProcessor->registerParameter(
        idPrefix + OSC_SHAPE_NOISE_PERCENT_SUFFIX, "Shape Noise", OSC_SHAPE_NOISE_PERCENT_DEFAULT, 0.0f, 100.0f,
        [statePtr](float value) { statePtr->shapeNoisePercent.store(value, std::memory_order_relaxed); },
        "Blends white noise into the waveform, as a percentage.");

    // Bound to a plain Dial (not a Cycler), which - like every Dial subclass - reads its own
    // min/max/default via ParameterManager::getParameterMinValue<float>/etc, so this is registered
    // through the float-typed template even though it's always a whole number of octaves.
    juce::NormalisableRange<float> octaveRange(OSC_OCTAVE_MIN, OSC_OCTAVE_MAX, 1.0f);
    audioProcessor->registerParameter<float>(
        std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { idPrefix + OSC_OCTAVE_SUFFIX, 1 }, "Octave", octaveRange, OSC_OCTAVE_DEFAULT),
        ndsp::IParameter::TYPE_FLOAT, OSC_OCTAVE_DEFAULT, OSC_OCTAVE_MIN, OSC_OCTAVE_MAX,
        [statePtr](float value) { statePtr->octave.store(static_cast<int>(std::round(value)), std::memory_order_relaxed); },
        "Coarse tuning, in whole octaves.");

    juce::NormalisableRange<float> transposeRange(OSC_TRANSPOSE_SEMITONES_MIN, OSC_TRANSPOSE_SEMITONES_MAX, 1.0f);
    audioProcessor->registerParameter<float>(
        std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { idPrefix + OSC_TRANSPOSE_SEMITONES_SUFFIX, 1 }, "Transpose", transposeRange, OSC_TRANSPOSE_SEMITONES_DEFAULT),
        ndsp::IParameter::TYPE_FLOAT, OSC_TRANSPOSE_SEMITONES_DEFAULT, OSC_TRANSPOSE_SEMITONES_MIN, OSC_TRANSPOSE_SEMITONES_MAX,
        [statePtr](float value) { statePtr->transposeSemitones.store(static_cast<int>(std::round(value)), std::memory_order_relaxed); },
        "Fine tuning on top of Octave, in semitones.");

    audioProcessor->registerParameter(
        idPrefix + OSC_DETUNE_CENTS_SUFFIX, "Detune", OSC_DETUNE_CENTS_DEFAULT, OSC_DETUNE_CENTS_MIN, OSC_DETUNE_CENTS_MAX,
        [statePtr](float value) { statePtr->detuneCents.store(value, std::memory_order_relaxed); },
        "Fine tuning, in cents.");

    audioProcessor->registerParameter(
        idPrefix + OSC_WARP_PERCENT_SUFFIX, "Warp", OSC_WARP_PERCENT_DEFAULT, OSC_WARP_PERCENT_MIN, OSC_WARP_PERCENT_MAX,
        [statePtr](float value) { statePtr->warpPercent.store(value, std::memory_order_relaxed); },
        "Squeezes the waveform into a shrinking window on one side of the cycle (negative: right, positive: left), zero-filling the rest.");

    audioProcessor->registerParameter(
        idPrefix + OSC_FOLD_PERCENT_SUFFIX, "Fold", OSC_FOLD_PERCENT_DEFAULT, 0.0f, 100.0f,
        [statePtr](float value) { statePtr->foldPercent.store(value, std::memory_order_relaxed); },
        "Wavefolds the shaped signal back onto itself, as a percentage.");

    audioProcessor->registerParameter(
        idPrefix + OSC_OUTPUT_DB_SUFFIX, "Output", outputDbDefault, OSC_OUTPUT_DB_MIN, OSC_OUTPUT_DB_MAX,
        [statePtr](float value) { statePtr->outputDb.store(value, std::memory_order_relaxed); },
        "Oscillator output level, in decibels.");

    juce::NormalisableRange<float> unisonVoicesRange(OSC_UNISON_VOICES_MIN, OSC_UNISON_VOICES_MAX, 1.0f);
    audioProcessor->registerParameter<float>(
        std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { idPrefix + OSC_UNISON_VOICES_SUFFIX, 1 }, "Unison Voices", unisonVoicesRange, OSC_UNISON_VOICES_DEFAULT),
        ndsp::IParameter::TYPE_FLOAT, OSC_UNISON_VOICES_DEFAULT, OSC_UNISON_VOICES_MIN, OSC_UNISON_VOICES_MAX,
        [statePtr](float value) { statePtr->unisonVoices.store(static_cast<int>(std::round(value)), std::memory_order_relaxed); },
        "Number of detuned/stereo-spread unison voices.");

    audioProcessor->registerParameter(
        idPrefix + OSC_UNISON_DETUNE_CENTS_SUFFIX, "Unison Detune", OSC_UNISON_DETUNE_CENTS_DEFAULT, 0.0f, OSC_UNISON_DETUNE_CENTS_MAX,
        [statePtr](float value) { statePtr->unisonDetuneCents.store(value, std::memory_order_relaxed); },
        "Spread between unison voices, in cents.");

    audioProcessor->registerParameter(
        idPrefix + OSC_UNISON_STEREO_PERCENT_SUFFIX, "Unison Stereo", OSC_UNISON_STEREO_PERCENT_DEFAULT, 0.0f, 100.0f,
        [statePtr](float value) { statePtr->unisonStereoPercent.store(value, std::memory_order_relaxed); },
        "Stereo width of the unison voice spread, as a percentage.");

    audioProcessor->registerParameter(
        idPrefix + OSC_SUB_LEVEL_PERCENT_SUFFIX, "Sub Level", OSC_SUB_LEVEL_PERCENT_DEFAULT, 0.0f, 100.0f,
        [statePtr](float value) { statePtr->subLevelPercent.store(value, std::memory_order_relaxed); },
        "Level of a plain sine sub-oscillator layer, as a percentage.");

    audioProcessor->registerParameter(
        idPrefix + OSC_SUB_OCTAVE_DOWN2_ENABLED_SUFFIX, "Sub Octave", OSC_SUB_OCTAVE_DOWN2_ENABLED_DEFAULT,
        [statePtr](bool value) { statePtr->subOctaveDown2.store(value, std::memory_order_relaxed); },
        "Off: sub-oscillator is one octave below the note. On: two octaves below.");

    audioProcessor->registerParameter(
        idPrefix + OSC_PHASE_PERCENT_SUFFIX, "Phase", OSC_PHASE_PERCENT_DEFAULT, 0.0f, 100.0f,
        [statePtr](float value) { statePtr->phasePercent.store(value, std::memory_order_relaxed); },
        "Starting phase for every new note, as a percentage of a full cycle.");

    audioProcessor->registerParameter(
        idPrefix + OSC_PHASE_RANDOMIZE_ENABLED_SUFFIX, "Phase Randomize", OSC_PHASE_RANDOMIZE_ENABLED_DEFAULT,
        [statePtr](bool value) { statePtr->phaseRandomizeEnabled.store(value, std::memory_order_relaxed); },
        "Ignore the Phase dial and start every new note at a random phase instead.");
}

void Parameters::registerEnvelopeParameters(PluginAudioProcessor* audioProcessor)
{
    // A raw pointer to the long-lived VoiceSharedState (owned by ChordSynthEngine, itself a
    // member of audioProcessor with the plugin's full lifetime) - captured by value into each
    // onChange lambda below. Capturing the reference returned by getSharedState() directly
    // (rather than through a pointer) would capture a copy of VoiceSharedState itself, which
    // doesn't compile (std::atomic is non-copyable) and wouldn't be what's wanted anyway.
    auto* sharedState = &audioProcessor->getSynthEngine().getSharedState();

    audioProcessor->registerParameter(
        ENVELOPE_DELAY_MS_ID, "Envelope Delay", ENVELOPE_DELAY_MS_DEFAULT, ENVELOPE_TIME_MS_MIN, ENVELOPE_TIME_MS_MAX,
        [sharedState](float value) { sharedState->envelopeDelayMs.store(value, std::memory_order_relaxed); },
        "Time before the envelope starts rising, in milliseconds.");

    audioProcessor->registerParameter(
        ENVELOPE_ATTACK_MS_ID, "Envelope Attack", ENVELOPE_ATTACK_MS_DEFAULT, ENVELOPE_TIME_MS_MIN, ENVELOPE_TIME_MS_MAX,
        [sharedState](float value) { sharedState->envelopeAttackMs.store(value, std::memory_order_relaxed); },
        "Time to rise from silence to full level, in milliseconds.");

    audioProcessor->registerParameter(
        ENVELOPE_HOLD_MS_ID, "Envelope Hold", ENVELOPE_HOLD_MS_DEFAULT, ENVELOPE_TIME_MS_MIN, ENVELOPE_TIME_MS_MAX,
        [sharedState](float value) { sharedState->envelopeHoldMs.store(value, std::memory_order_relaxed); },
        "Time to hold at full level before decaying, in milliseconds.");

    audioProcessor->registerParameter(
        ENVELOPE_DECAY_MS_ID, "Envelope Decay", ENVELOPE_DECAY_MS_DEFAULT, ENVELOPE_TIME_MS_MIN, ENVELOPE_TIME_MS_MAX,
        [sharedState](float value) { sharedState->envelopeDecayMs.store(value, std::memory_order_relaxed); },
        "Time to fall from full level to the sustain level, in milliseconds.");

    audioProcessor->registerParameter(
        ENVELOPE_SUSTAIN_PERCENT_ID, "Envelope Sustain", ENVELOPE_SUSTAIN_PERCENT_DEFAULT, 0.0f, 100.0f,
        [sharedState](float value) { sharedState->envelopeSustainPercent.store(value, std::memory_order_relaxed); },
        "Level held for as long as a note is held, as a percentage of full level.");

    audioProcessor->registerParameter(
        ENVELOPE_RELEASE_MS_ID, "Envelope Release", ENVELOPE_RELEASE_MS_DEFAULT, ENVELOPE_TIME_MS_MIN, ENVELOPE_TIME_MS_MAX,
        [sharedState](float value) { sharedState->envelopeReleaseMs.store(value, std::memory_order_relaxed); },
        "Time to fall to silence after a note is released, in milliseconds.");
}

void Parameters::registerFilterParameters(PluginAudioProcessor* audioProcessor)
{
    auto* sharedState = &audioProcessor->getSynthEngine().getSharedState();

    audioProcessor->registerParameter<int>(
        std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { FILTER_TYPE_ID, 1 }, "Filter Type",
            juce::StringArray { "Low-pass", "High-pass", "Band-pass" }, FILTER_TYPE_DEFAULT),
        ndsp::IParameter::TYPE_INT, FILTER_TYPE_DEFAULT, 0, 2,
        [sharedState](int value) { sharedState->filterType.store(value, std::memory_order_relaxed); },
        "Filter type: low-pass, high-pass, or band-pass.");

    audioProcessor->registerParameter<int>(
        std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { FILTER_SLOPE_ID, 1 }, "Filter Slope",
            juce::StringArray { "12 dB/oct", "24 dB/oct" }, FILTER_SLOPE_DEFAULT),
        ndsp::IParameter::TYPE_INT, FILTER_SLOPE_DEFAULT, 0, 1,
        [sharedState](int value) { sharedState->filterSlope.store(value, std::memory_order_relaxed); },
        "Filter steepness: 12 or 24 decibels per octave.");

    // setSkewForCentre puts 1kHz at the knob's midpoint - the standard "feels logarithmic"
    // convention for a frequency control, computed by JUCE rather than hand-derived skew math.
    juce::NormalisableRange<float> cutoffRange(FILTER_CUTOFF_HZ_MIN, FILTER_CUTOFF_HZ_MAX);
    cutoffRange.setSkewForCentre(1000.0f);
    audioProcessor->registerParameter<float>(
        std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { FILTER_CUTOFF_HZ_ID, 1 }, "Filter Cutoff", cutoffRange, FILTER_CUTOFF_HZ_DEFAULT),
        ndsp::IParameter::TYPE_FLOAT, FILTER_CUTOFF_HZ_DEFAULT, FILTER_CUTOFF_HZ_MIN, FILTER_CUTOFF_HZ_MAX,
        [sharedState](float value) { sharedState->filterCutoffHz.store(value, std::memory_order_relaxed); },
        "Filter cutoff frequency, in Hz.");

    audioProcessor->registerParameter(
        FILTER_RESONANCE_ID, "Filter Resonance", FILTER_RESONANCE_DEFAULT, FILTER_RESONANCE_MIN, FILTER_RESONANCE_MAX,
        [sharedState](float value) { sharedState->filterResonance.store(value, std::memory_order_relaxed); },
        "Filter resonance - how strongly the response peaks around the cutoff frequency.");

    audioProcessor->registerParameter(
        FILTER_DRIVE_DB_ID, "Filter Drive", FILTER_DRIVE_DB_DEFAULT, 0.0f, FILTER_DRIVE_DB_MAX,
        [sharedState](float value) { sharedState->filterDriveDb.store(value, std::memory_order_relaxed); },
        "Pre-filter saturation, in decibels.");

    audioProcessor->registerParameter(
        FILTER_MIX_PERCENT_ID, "Filter Mix", FILTER_MIX_PERCENT_DEFAULT, 0.0f, 100.0f,
        [sharedState](float value) { sharedState->filterMixPercent.store(value, std::memory_order_relaxed); },
        "Blend between the unfiltered and filtered signal, as a percentage.");

    audioProcessor->registerParameter(
        FILTER_KEY_TRACK_PERCENT_ID, "Filter Key Track", FILTER_KEY_TRACK_PERCENT_DEFAULT, 0.0f, 100.0f,
        [sharedState](float value) { sharedState->filterKeyTrackPercent.store(value, std::memory_order_relaxed); },
        "How much the cutoff frequency follows the played note's pitch, as a percentage.");
}

void Parameters::registerLfoParameters(PluginAudioProcessor* audioProcessor)
{
    auto* sharedState = &audioProcessor->getSynthEngine().getSharedState();

    audioProcessor->registerParameter<int>(
        std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { LFO_SHAPE_ID, 1 }, "LFO Shape",
            juce::StringArray { "Sine", "Triangle", "Saw", "Square" }, LFO_SHAPE_DEFAULT),
        ndsp::IParameter::TYPE_INT, LFO_SHAPE_DEFAULT, 0, 3,
        [sharedState](int value) { sharedState->lfoShape.store(value, std::memory_order_relaxed); },
        "LFO waveform shape.");

    juce::NormalisableRange<float> rateRange(LFO_RATE_HZ_MIN, LFO_RATE_HZ_MAX);
    rateRange.setSkewForCentre(1.0f);
    audioProcessor->registerParameter<float>(
        std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { LFO_RATE_HZ_ID, 1 }, "LFO Rate", rateRange, LFO_RATE_HZ_DEFAULT),
        ndsp::IParameter::TYPE_FLOAT, LFO_RATE_HZ_DEFAULT, LFO_RATE_HZ_MIN, LFO_RATE_HZ_MAX,
        [sharedState](float value) { sharedState->lfoRateHz.store(value, std::memory_order_relaxed); },
        "LFO rate, in Hz (used when tempo sync is off).");

    audioProcessor->registerParameter(
        LFO_SYNC_ENABLED_ID, "LFO Tempo Sync", LFO_SYNC_ENABLED_DEFAULT,
        [sharedState](bool value) { sharedState->lfoSyncEnabled.store(value, std::memory_order_relaxed); },
        "Sync the LFO rate to the host tempo instead of a free rate in Hz.");

    // Bound to a TimingDial (not a Cycler), which - like every Dial subclass - reads its own
    // min/max/default via ParameterManager::getParameterMinValue<float>/etc, so this must be
    // registered through the float-typed template even though the raw values are always whole
    // numbers. Uses the raw 1-based ndsp::Timing::NoteTiming range directly (interval=1 keeps it
    // stepped to whole values) - TimingDial::getTextFromValue expects that, not a 0-based index.
    juce::NormalisableRange<float> syncDivisionRange(LFO_SYNC_DIVISION_MIN, LFO_SYNC_DIVISION_MAX, 1.0f);
    audioProcessor->registerParameter<float>(
        std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { LFO_SYNC_DIVISION_ID, 1 }, "LFO Sync Division", syncDivisionRange, LFO_SYNC_DIVISION_DEFAULT),
        ndsp::IParameter::TYPE_FLOAT, LFO_SYNC_DIVISION_DEFAULT, LFO_SYNC_DIVISION_MIN, LFO_SYNC_DIVISION_MAX,
        [sharedState](float value) { sharedState->lfoSyncDivision.store(static_cast<int>(std::round(value)), std::memory_order_relaxed); },
        "Note division the LFO syncs to when tempo sync is on.");

    audioProcessor->registerParameter(
        LFO_MODE_ID, "LFO Mode", LFO_MODE_DEFAULT,
        [sharedState](bool value) { sharedState->lfoMode.store(value ? 1 : 0, std::memory_order_relaxed); },
        "Trigger: phase resets on every note. Free: phase runs continuously, shared across notes.");

    audioProcessor->registerParameter(
        LFO_SMOOTH_PERCENT_ID, "LFO Smooth", LFO_SMOOTH_PERCENT_DEFAULT, 0.0f, 100.0f,
        [sharedState](float value) { sharedState->lfoSmoothPercent.store(value, std::memory_order_relaxed); },
        "Rounds the LFO waveform's corners toward a sine shape, as a percentage.");

    audioProcessor->registerParameter(
        LFO_DELAY_MS_ID, "LFO Delay", LFO_DELAY_MS_DEFAULT, 0.0f, 10000.0f,
        [sharedState](float value) { sharedState->lfoDelayMs.store(value, std::memory_order_relaxed); },
        "Time after a note starts before the LFO begins modulating, in milliseconds.");

    audioProcessor->registerParameter(
        LFO_STEREO_PERCENT_ID, "LFO Stereo", LFO_STEREO_PERCENT_DEFAULT, 0.0f, 100.0f,
        [sharedState](float value) { sharedState->lfoStereoPercent.store(value, std::memory_order_relaxed); },
        "Phase offset between the left and right channels, as a percentage.");

    audioProcessor->registerParameter(
        LFO_DEPTH_PERCENT_ID, "LFO Depth", LFO_DEPTH_PERCENT_DEFAULT, 0.0f, 100.0f,
        [sharedState](float value) { sharedState->lfoDepthPercent.store(value, std::memory_order_relaxed); },
        "How strongly the LFO modulates the filter cutoff, as a percentage.");
}

void Parameters::registerSection(Section section, PluginAudioProcessor* audioProcessor)
{
    switch (section)
    {
        case PLUGIN:
            registerPluginParameters(audioProcessor);
            break;
        case OSCILLATOR:
        {
            auto* sharedState = &audioProcessor->getSynthEngine().getSharedState();
            registerOscillatorParameters(audioProcessor, OSC1_ID_PREFIX, sharedState->oscillator1, OSC_OUTPUT_DB_DEFAULT);
            registerOscillatorParameters(audioProcessor, OSC2_ID_PREFIX, sharedState->oscillator2, audio::VoiceSharedState::kOscillator2DefaultOutputDb);

            auto* oscillator2State = &sharedState->oscillator2;
            audioProcessor->registerParameter(
                OSC2_ENABLED_ID, "Oscillator 2 Enabled", OSC2_ENABLED_DEFAULT,
                [oscillator2State](bool value) { oscillator2State->enabled.store(value, std::memory_order_relaxed); },
                "Turns Oscillator 2 on or off.");
            break;
        }
        case ENVELOPE:
            registerEnvelopeParameters(audioProcessor);
            break;
        case FILTER:
            registerFilterParameters(audioProcessor);
            break;
        case LFO:
            registerLfoParameters(audioProcessor);
            break;
    }
}

void Parameters::registerAllSections(PluginAudioProcessor* audioProcessor)
{
    registerSection(Section::PLUGIN, audioProcessor);
    registerSection(Section::OSCILLATOR, audioProcessor);
    registerSection(Section::ENVELOPE, audioProcessor);
    registerSection(Section::FILTER, audioProcessor);
    registerSection(Section::LFO, audioProcessor);
}
