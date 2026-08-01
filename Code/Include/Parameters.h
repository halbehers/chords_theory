#pragma once

#include "PluginProcessor.h"

struct Parameters
{
    // General.
    static constexpr char PLUGIN_ENABLED_ID[] = "plugin-enabled";
    static constexpr bool PLUGIN_ENABLED_DEFAULT = true;

    // Synth envelope (Delay-Attack-Hold-Decay-Sustain-Release).
    static constexpr char ENVELOPE_DELAY_MS_ID[] = "envelope-delay-ms";
    static constexpr float ENVELOPE_DELAY_MS_DEFAULT = 0.0f;
    static constexpr char ENVELOPE_ATTACK_MS_ID[] = "envelope-attack-ms";
    static constexpr float ENVELOPE_ATTACK_MS_DEFAULT = 5.0f;
    static constexpr char ENVELOPE_HOLD_MS_ID[] = "envelope-hold-ms";
    static constexpr float ENVELOPE_HOLD_MS_DEFAULT = 0.0f;
    static constexpr char ENVELOPE_DECAY_MS_ID[] = "envelope-decay-ms";
    static constexpr float ENVELOPE_DECAY_MS_DEFAULT = 0.0f;
    static constexpr char ENVELOPE_SUSTAIN_PERCENT_ID[] = "envelope-sustain-percent";
    static constexpr float ENVELOPE_SUSTAIN_PERCENT_DEFAULT = 100.0f;
    static constexpr char ENVELOPE_RELEASE_MS_ID[] = "envelope-release-ms";
    static constexpr float ENVELOPE_RELEASE_MS_DEFAULT = 200.0f;
    static constexpr float ENVELOPE_TIME_MS_MIN = 0.0f;
    static constexpr float ENVELOPE_TIME_MS_MAX = 10000.0f;

    // Synth filter. filterType choices: Low-pass/High-pass/Band-pass. filterSlope choices:
    // 12dB/octave (one juce::dsp::StateVariableTPTFilter stage), 24dB/octave (two cascaded).
    static constexpr char FILTER_TYPE_ID[] = "filter-type";
    static constexpr int FILTER_TYPE_DEFAULT = 0;
    static constexpr char FILTER_SLOPE_ID[] = "filter-slope";
    static constexpr int FILTER_SLOPE_DEFAULT = 0;
    static constexpr char FILTER_CUTOFF_HZ_ID[] = "filter-cutoff-hz";
    static constexpr float FILTER_CUTOFF_HZ_DEFAULT = 20000.0f;
    static constexpr float FILTER_CUTOFF_HZ_MIN = 20.0f;
    static constexpr float FILTER_CUTOFF_HZ_MAX = 20000.0f;
    static constexpr char FILTER_RESONANCE_ID[] = "filter-resonance";
    static constexpr float FILTER_RESONANCE_DEFAULT = 0.70710678f; // 1/sqrt(2) - "flat" 12dB/octave response
    static constexpr float FILTER_RESONANCE_MIN = 0.1f;
    static constexpr float FILTER_RESONANCE_MAX = 10.0f;
    static constexpr char FILTER_DRIVE_DB_ID[] = "filter-drive-db";
    static constexpr float FILTER_DRIVE_DB_DEFAULT = 0.0f;
    static constexpr float FILTER_DRIVE_DB_MAX = 24.0f;
    static constexpr char FILTER_MIX_PERCENT_ID[] = "filter-mix-percent";
    static constexpr float FILTER_MIX_PERCENT_DEFAULT = 100.0f;
    static constexpr char FILTER_KEY_TRACK_PERCENT_ID[] = "filter-key-track-percent";
    static constexpr float FILTER_KEY_TRACK_PERCENT_DEFAULT = 0.0f;

    // Synth LFO. lfoShape choices: Sine/Triangle/Saw/Square (bound to a Cycler - registered as a
    // plain int choice). lfoSyncDivision (the raw 1-based ndsp::Timing::NoteTiming range) is bound
    // to a TimingDial instead, which - unlike Cycler - reads its own min/max/default via
    // ParameterManager::getParameterMinValue<float>/etc, so it's registered through
    // ParameterManager's float-typed template (TYPE_FLOAT) even though the underlying JUCE
    // parameter is still a stepped AudioParameterFloat for host-automation purposes - see
    // registerLfoParameters for exactly how. lfoMode is a plain bool (Trigger=false/Free=true),
    // bound to an SVGToggle, same shape as lfoSyncEnabled below.
    static constexpr char LFO_SHAPE_ID[] = "lfo-shape";
    static constexpr int LFO_SHAPE_DEFAULT = 0;
    static constexpr char LFO_RATE_HZ_ID[] = "lfo-rate-hz";
    static constexpr float LFO_RATE_HZ_DEFAULT = 2.0f;
    static constexpr float LFO_RATE_HZ_MIN = 0.01f;
    static constexpr float LFO_RATE_HZ_MAX = 20.0f;
    static constexpr char LFO_SYNC_ENABLED_ID[] = "lfo-sync-enabled";
    static constexpr bool LFO_SYNC_ENABLED_DEFAULT = false;
    static constexpr char LFO_SYNC_DIVISION_ID[] = "lfo-sync-division";
    static constexpr float LFO_SYNC_DIVISION_DEFAULT = 7.0f; // NOTE_4 (1/4 note), a raw NoteTiming value
    static constexpr float LFO_SYNC_DIVISION_MIN = 1.0f;
    static constexpr float LFO_SYNC_DIVISION_MAX = 14.0f;
    static constexpr char LFO_MODE_ID[] = "lfo-mode";
    static constexpr bool LFO_MODE_DEFAULT = false; // false = Trigger, true = Free
    static constexpr char LFO_SMOOTH_PERCENT_ID[] = "lfo-smooth-percent";
    static constexpr float LFO_SMOOTH_PERCENT_DEFAULT = 0.0f;
    static constexpr char LFO_DELAY_MS_ID[] = "lfo-delay-ms";
    static constexpr float LFO_DELAY_MS_DEFAULT = 0.0f;
    static constexpr char LFO_STEREO_PERCENT_ID[] = "lfo-stereo-percent";
    static constexpr float LFO_STEREO_PERCENT_DEFAULT = 0.0f;
    static constexpr char LFO_DEPTH_PERCENT_ID[] = "lfo-depth-percent";
    static constexpr float LFO_DEPTH_PERCENT_DEFAULT = 0.0f;

    enum Section
    {
        PLUGIN,
        ENVELOPE,
        FILTER,
        LFO,
    };

    static void registerPluginParameters(PluginAudioProcessor* audioProcessor);
    static void registerEnvelopeParameters(PluginAudioProcessor* audioProcessor);
    static void registerFilterParameters(PluginAudioProcessor* audioProcessor);
    static void registerLfoParameters(PluginAudioProcessor* audioProcessor);

    static void registerSection(Section section, PluginAudioProcessor* audioProcessor);
    static void registerAllSections(PluginAudioProcessor* audioProcessor);
};
