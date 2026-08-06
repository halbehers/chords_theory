#pragma once

#include <atomic>

#include <nierika_dsp/nierika_dsp.h>

namespace audio
{

// One oscillator's worth of atomics, matching ndsp::Oscillator::Parameters field-for-field - see
// SynthVoice::readOscillatorParameters() for the int/bool-to-Oscillator::Parameters mapping.
// shape: 0=Sine,1=Triangle,2=Saw,3=Square, same vocabulary as VoiceSharedState::lfoShape below.
// Every field here follows VoiceSharedState's own cross-thread/read-cadence contract (see its doc
// comment) - re-snapshotted once per renderNextBlock, same as the Filter/LFO blocks.
struct OscillatorState
{
    // Only oscillator2 ever has this driven by a real UI toggle (see
    // SynthOscillatorSection/Parameters::registerSection's OSCILLATOR case) - oscillator1 has no
    // enable/disable control at all, so this simply stays at its default (always on) forever.
    std::atomic<bool> enabled { true };
    std::atomic<int> shape { 0 };
    std::atomic<float> shapeNoisePercent { 0.0f };
    std::atomic<int> octave { 0 };
    std::atomic<int> transposeSemitones { 0 };
    std::atomic<float> detuneCents { 0.0f };
    std::atomic<float> warpPercent { 0.0f };
    std::atomic<float> foldPercent { 0.0f };
    std::atomic<float> outputDb { 0.0f };
    std::atomic<int> unisonVoices { 1 };
    std::atomic<float> unisonDetuneCents { 0.0f };
    std::atomic<float> unisonStereoPercent { 0.0f };
    std::atomic<float> phasePercent { 0.0f };
    std::atomic<bool> phaseRandomizeEnabled { false };
};

// The dedicated sub-oscillator layer (see SynthSubSection) - a single, voice-wide instance rather
// than one per main oscillator (unlike OscillatorState above). Matches ndsp::SubOscillator::
// Parameters field-for-field - see SynthVoice::readSubOscillatorParameters().
struct SubOscillatorState
{
    std::atomic<bool> enabled { false };
    std::atomic<int> octave { -1 };
    std::atomic<int> transposeSemitones { 0 };
    std::atomic<float> tonePercent { 0.0f };
    std::atomic<float> outputDb { 0.0f }; // unity - `enabled` already handles silence-by-default
};

// How oscillator1 and oscillator2 combine (see SynthMixerSection/Parameters::MIXER_ALGORITHM_ID).
// algorithmIndex: 0=Add,1=FM,2=RingMod,3=AM,4=SerialFold - matches the mixer-algorithm
// AudioParameterChoice's order exactly, and indexes SynthVoice's own fixed algorithm array. Each
// parameterized algorithm gets its own independent amount, not one shared/reinterpreted value, so
// switching algorithms never makes a previously-automated value suddenly mean something else.
struct MixerState
{
    std::atomic<int> algorithmIndex { 0 };
    std::atomic<float> fmAmountPercent { 0.0f };
    std::atomic<float> ringModMixPercent { 0.0f };
    std::atomic<float> amDepthPercent { 0.0f };
    std::atomic<float> serialFoldAmountPercent { 0.0f };
};

// Owned by ChordSynthEngine, referenced by every SynthVoice. Every field is std::atomic
// because a write can legitimately arrive from either thread: a UI knob drag fires an APVTS
// onChange callback on the message thread, but host automation writing the same parameter during
// processBlock fires the identical callback on the audio thread (confirmed against JUCE's actual
// AudioProcessorValueTreeState::ParameterAdapter::parameterValueChanged - it notifies listeners
// synchronously on whichever thread touched the parameter, no thread hop). A plain float would be
// a real data race against automation writes; relaxed ordering is sufficient since every field is
// read/written independently, with no cross-field consistency required.
//
// Read cadence is deliberately not uniform - see SynthVoice for where each field is actually
// read: envelope shape is snapshotted once at startNote() (matches juce::ADSR's own "don't change
// shape mid-flight" contract), not re-read continuously during a held note.
struct VoiceSharedState
{
    // oscillator2 defaults its output far enough down (-60dB, effectively silent) that a fresh,
    // untouched instance sums to the same audible result as oscillator1 alone - matching this
    // plugin's "sonically identical to today until touched" convention for every other section.
    static constexpr float kOscillator2DefaultOutputDb = -60.0f;

    OscillatorState oscillator1;
    OscillatorState oscillator2 { .outputDb = kOscillator2DefaultOutputDb };
    SubOscillatorState subOscillator;
    MixerState mixer;

    // Global reference pitch (concert A4), read identically by every oscillator across every voice
    // (see SynthOutputSection) - not nested in Oscillator/Sub/Mixer state since it isn't specific
    // to any one of them.
    std::atomic<float> tuningReferenceHz { 440.0f };

    std::atomic<float> envelopeDelayMs { 0.0f };
    std::atomic<float> envelopeAttackMs { 5.0f };
    std::atomic<float> envelopeHoldMs { 0.0f };
    std::atomic<float> envelopeDecayMs { 0.0f };
    std::atomic<float> envelopeSustainPercent { 100.0f };
    std::atomic<float> envelopeReleaseMs { 200.0f };

    // filterType: 0 = low-pass, 1 = high-pass, 2 = band-pass. filterSlope: 0 = 12dB/octave,
    // 1 = 24dB/octave. See SynthVoice::readFilterParameters() for the int-to-enum mapping.
    std::atomic<int> filterType { 0 };
    std::atomic<int> filterSlope { 0 };
    std::atomic<float> filterCutoffHz { 20000.0f };
    std::atomic<float> filterResonance { 0.70710678f };
    std::atomic<float> filterDriveDb { 0.0f };
    std::atomic<float> filterMixPercent { 100.0f };
    std::atomic<float> filterKeyTrackPercent { 0.0f };

    // lfoShape: 0=Sine,1=Triangle,2=Saw,3=Square. lfoMode: 0=Trigger,1=Free. lfoSyncDivision is a
    // raw ndsp::Timing::NoteTiming value. See SynthVoice::readLfoParameters() for the mapping.
    std::atomic<int> lfoShape { 0 };
    std::atomic<float> lfoRateHz { 2.0f };
    std::atomic<bool> lfoSyncEnabled { false };
    std::atomic<int> lfoSyncDivision { static_cast<int>(ndsp::Timing::NOTE_4) };
    std::atomic<int> lfoMode { 0 };
    std::atomic<float> lfoSmoothPercent { 0.0f };
    std::atomic<float> lfoDelayMs { 0.0f };
    std::atomic<float> lfoStereoPercent { 0.0f };
    std::atomic<float> lfoDepthPercent { 0.0f };

    // Audio-thread-only transport/modulation state: written once per block by
    // ChordSynthEngine::renderNextBlock itself, read only by voices called synchronously within
    // that same call stack - never touched cross-thread, so plain (non-atomic) doubles are
    // correct here, unlike every field above.
    double hostBpm = 120.0;
    double freeLfoPhase01 = 0.0;
};

}
