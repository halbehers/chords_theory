#pragma once

#include <string>

#include "Audio/VoiceSharedState.h"
#include "PluginProcessor.h"

struct Parameters
{
    // General.
    static constexpr char PLUGIN_ENABLED_ID[] = "plugin-enabled";
    static constexpr bool PLUGIN_ENABLED_DEFAULT = true;

    // Synth oscillators (two independent instances - osc1-/osc2- prefixed IDs, both registered via
    // the one registerOscillatorParameters(processor, idPrefix, state) helper below, since the
    // alternative is two ~130-line blocks differing only by ID string and which OscillatorState
    // field to write into). oscShape reuses the same vocabulary as lfoShape (Sine/Triangle/Saw/
    // Square). Octave and Unison Voices are stepped integer-like dials (interval=1), registered
    // through the float-typed template for the same reason lfoSyncDivision is - Dial-family
    // widgets read their own min/max/default via getParameterMinValue<float>/etc.
    static constexpr char OSC_SHAPE_SUFFIX[] = "shape";
    static constexpr int OSC_SHAPE_DEFAULT = 0;
    static constexpr char OSC_SHAPE_NOISE_PERCENT_SUFFIX[] = "shape-noise-percent";
    static constexpr float OSC_SHAPE_NOISE_PERCENT_DEFAULT = 0.0f;
    static constexpr char OSC_OCTAVE_SUFFIX[] = "octave";
    static constexpr float OSC_OCTAVE_DEFAULT = 0.0f;
    static constexpr float OSC_OCTAVE_MIN = -3.0f;
    static constexpr float OSC_OCTAVE_MAX = 3.0f;
    static constexpr char OSC_TRANSPOSE_SEMITONES_SUFFIX[] = "transpose-semitones";
    static constexpr float OSC_TRANSPOSE_SEMITONES_DEFAULT = 0.0f;
    static constexpr float OSC_TRANSPOSE_SEMITONES_MIN = -12.0f;
    static constexpr float OSC_TRANSPOSE_SEMITONES_MAX = 12.0f;
    static constexpr char OSC_DETUNE_CENTS_SUFFIX[] = "detune-cents";
    static constexpr float OSC_DETUNE_CENTS_DEFAULT = 0.0f;
    static constexpr float OSC_DETUNE_CENTS_MIN = -50.0f;
    static constexpr float OSC_DETUNE_CENTS_MAX = 50.0f;
    static constexpr char OSC_WARP_PERCENT_SUFFIX[] = "warp-percent";
    static constexpr float OSC_WARP_PERCENT_DEFAULT = 0.0f;
    static constexpr float OSC_WARP_PERCENT_MIN = -100.0f;
    static constexpr float OSC_WARP_PERCENT_MAX = 100.0f;
    static constexpr char OSC_FOLD_PERCENT_SUFFIX[] = "fold-percent";
    static constexpr float OSC_FOLD_PERCENT_DEFAULT = 0.0f;
    static constexpr char OSC_OUTPUT_DB_SUFFIX[] = "output-db";
    static constexpr float OSC_OUTPUT_DB_DEFAULT = 0.0f; // oscillator 1's default - oscillator 2's
                                                            // differs (see VoiceSharedState::kOscillator2DefaultOutputDb)
    static constexpr float OSC_OUTPUT_DB_MIN = -24.0f;
    static constexpr float OSC_OUTPUT_DB_MAX = 6.0f;
    static constexpr char OSC_UNISON_VOICES_SUFFIX[] = "unison-voices";
    static constexpr float OSC_UNISON_VOICES_DEFAULT = 1.0f;
    static constexpr float OSC_UNISON_VOICES_MIN = 1.0f;
    static constexpr float OSC_UNISON_VOICES_MAX = 16.0f;
    static constexpr char OSC_UNISON_DETUNE_CENTS_SUFFIX[] = "unison-detune-cents";
    static constexpr float OSC_UNISON_DETUNE_CENTS_DEFAULT = 0.0f;
    static constexpr float OSC_UNISON_DETUNE_CENTS_MAX = 50.0f;
    static constexpr char OSC_UNISON_STEREO_PERCENT_SUFFIX[] = "unison-stereo-percent";
    static constexpr float OSC_UNISON_STEREO_PERCENT_DEFAULT = 0.0f;
    static constexpr char OSC_PHASE_PERCENT_SUFFIX[] = "phase-percent";
    static constexpr float OSC_PHASE_PERCENT_DEFAULT = 0.0f;
    static constexpr char OSC_PHASE_RANDOMIZE_ENABLED_SUFFIX[] = "phase-randomize-enabled";
    static constexpr bool OSC_PHASE_RANDOMIZE_ENABLED_DEFAULT = false;

    static constexpr char OSC1_ID_PREFIX[] = "osc1-";
    static constexpr char OSC2_ID_PREFIX[] = "osc2-";

    // Oscillator 2 only - oscillator 1 has no enable/disable toggle at all (see
    // SynthOscillatorSection), so this isn't part of the shared registerOscillatorParameters
    // helper above; it's its own full ID constant, registered directly in registerSection's
    // OSCILLATOR case.
    static constexpr char OSC2_ENABLED_ID[] = "osc2-enabled";
    static constexpr bool OSC2_ENABLED_DEFAULT = false;

    // Synth sub-oscillator (see SynthSubSection) - a single, dedicated low-end layer, distinct
    // from each main oscillator's own Octave/Transpose. Defaults to disabled (same "silent until
    // touched" convention as Oscillator 2) and one octave below the note when enabled, matching
    // the plain sub-oscillator layer this replaced inside Oscillator itself.
    static constexpr char SUB_ENABLED_ID[] = "sub-enabled";
    static constexpr bool SUB_ENABLED_DEFAULT = false;
    static constexpr char SUB_OCTAVE_ID[] = "sub-octave";
    static constexpr float SUB_OCTAVE_DEFAULT = -1.0f;
    static constexpr float SUB_OCTAVE_MIN = -3.0f;
    static constexpr float SUB_OCTAVE_MAX = 3.0f;
    static constexpr char SUB_TRANSPOSE_SEMITONES_ID[] = "sub-transpose-semitones";
    static constexpr float SUB_TRANSPOSE_SEMITONES_DEFAULT = 0.0f;
    static constexpr float SUB_TRANSPOSE_SEMITONES_MIN = -12.0f;
    static constexpr float SUB_TRANSPOSE_SEMITONES_MAX = 12.0f;
    static constexpr char SUB_TONE_PERCENT_ID[] = "sub-tone-percent";
    static constexpr float SUB_TONE_PERCENT_DEFAULT = 0.0f; // 0 = pure sine, matching the old sub layer's plain-sine default
    static constexpr char SUB_OUTPUT_DB_ID[] = "sub-output-db";
    static constexpr float SUB_OUTPUT_DB_DEFAULT = 0.0f; // unity - `enabled` already handles silence-by-default
    static constexpr float SUB_OUTPUT_DB_MIN = -24.0f;
    static constexpr float SUB_OUTPUT_DB_MAX = 6.0f;

    // Synth mixer (see SynthMixerSection) - how oscillator 1 and oscillator 2 combine, plus the
    // Sub/Osc1/Osc2 level sliders (the latter two bind directly to each oscillator's own
    // pre-existing OSC_OUTPUT_DB_SUFFIX parameter, not a new mixer-scoped one). Each parameterized
    // algorithm gets its own independent amount parameter (not one shared/reinterpreted value) -
    // correct for automation/session recall, since switching algorithms must never make a
    // previously-automated value suddenly mean something else. algorithmIndex order:
    // 0=Add,1=FM,2=Ring Mod,3=AM,4=Serial Fold - matches SynthVoice's own fixed algorithm array.
    static constexpr char MIXER_ALGORITHM_ID[] = "mixer-algorithm";
    static constexpr int MIXER_ALGORITHM_DEFAULT = 0; // Add - identical to the plugin's original behavior
    static constexpr char MIXER_FM_AMOUNT_PERCENT_ID[] = "mixer-fm-amount-percent";
    static constexpr float MIXER_FM_AMOUNT_PERCENT_DEFAULT = 0.0f;
    static constexpr char MIXER_RING_MOD_MIX_PERCENT_ID[] = "mixer-ring-mod-mix-percent";
    static constexpr float MIXER_RING_MOD_MIX_PERCENT_DEFAULT = 0.0f;
    static constexpr char MIXER_AM_DEPTH_PERCENT_ID[] = "mixer-am-depth-percent";
    static constexpr float MIXER_AM_DEPTH_PERCENT_DEFAULT = 0.0f;
    static constexpr char MIXER_SERIAL_FOLD_AMOUNT_PERCENT_ID[] = "mixer-serial-fold-amount-percent";
    static constexpr float MIXER_SERIAL_FOLD_AMOUNT_PERCENT_DEFAULT = 0.0f;

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

    // Synth output/master bus (see SynthOutputSection, MasterBus/MasterCompressor). Tuning writes
    // into VoiceSharedState (read by every oscillator, per-voice), the other three into
    // MasterBusState (read once per block by ChordSynthEngine's MasterBus) - both reachable
    // through the existing getSynthEngine() accessor. All four defaults are no-ops (440Hz standard
    // reference, compressor bypassed, centred pan, unity output), so a fresh instance is sonically
    // unchanged from before this section existed.
    static constexpr char TUNING_REFERENCE_HZ_ID[] = "tuning-reference-hz";
    static constexpr float TUNING_REFERENCE_HZ_DEFAULT = 440.0f;
    static constexpr float TUNING_REFERENCE_HZ_MIN = 415.0f;
    static constexpr float TUNING_REFERENCE_HZ_MAX = 466.0f;
    static constexpr char COMPRESSOR_AMOUNT_PERCENT_ID[] = "compressor-amount-percent";
    static constexpr float COMPRESSOR_AMOUNT_PERCENT_DEFAULT = 0.0f;
    static constexpr char PAN_PERCENT_ID[] = "pan-percent";
    static constexpr float PAN_PERCENT_DEFAULT = 0.0f;
    static constexpr float PAN_PERCENT_MIN = -100.0f;
    static constexpr float PAN_PERCENT_MAX = 100.0f;
    static constexpr char OUTPUT_DB_ID[] = "output-db";
    static constexpr float OUTPUT_DB_DEFAULT = 0.0f;
    static constexpr float OUTPUT_DB_MIN = -24.0f;
    static constexpr float OUTPUT_DB_MAX = 6.0f;

    enum Section
    {
        PLUGIN,
        OSCILLATOR,
        SUB,
        MIXER,
        ENVELOPE,
        FILTER,
        LFO,
        OUTPUT,
    };

    static void registerPluginParameters(PluginAudioProcessor* audioProcessor);
    // Registers all 13 osc-* parameters for one oscillator instance, with idPrefix ("osc1-" or
    // "osc2-") prepended to every ID and onChange callbacks writing into state. outputDbDefault is
    // the one field that legitimately differs per instance (see
    // VoiceSharedState::kOscillator2DefaultOutputDb) - every other default is shared, via the
    // OSC_*_DEFAULT constants above.
    static void registerOscillatorParameters(PluginAudioProcessor* audioProcessor, const std::string& idPrefix, audio::OscillatorState& state, float outputDbDefault);
    static void registerSubParameters(PluginAudioProcessor* audioProcessor);
    static void registerMixerParameters(PluginAudioProcessor* audioProcessor);
    static void registerEnvelopeParameters(PluginAudioProcessor* audioProcessor);
    static void registerFilterParameters(PluginAudioProcessor* audioProcessor);
    static void registerLfoParameters(PluginAudioProcessor* audioProcessor);
    static void registerOutputParameters(PluginAudioProcessor* audioProcessor);

    static void registerSection(Section section, PluginAudioProcessor* audioProcessor);
    static void registerAllSections(PluginAudioProcessor* audioProcessor);
};
