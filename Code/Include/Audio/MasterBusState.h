#pragma once

#include <atomic>

namespace audio
{

// Master-bus atomics (see SynthOutputSection) - owned by ChordSynthEngine alongside
// VoiceSharedState, but deliberately separate from it: these are read once per block by MasterBus
// on the already-summed stereo mix, never by any individual SynthVoice (unlike everything in
// VoiceSharedState, which is per-voice-consumed). Same cross-thread/relaxed-ordering contract as
// VoiceSharedState - see its own doc comment.
struct MasterBusState
{
    std::atomic<float> compressorAmountPercent { 0.0f }; // 0 = bypass, matches this codebase's "silent/no-op until touched" convention
    std::atomic<float> panPercent { 0.0f };               // -100..100, 0 = center
    std::atomic<float> outputDb { 0.0f };                 // -24..+6, matches OSC_OUTPUT_DB's existing range
};

}
