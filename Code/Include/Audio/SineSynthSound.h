#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace audio
{

// Single, undifferentiated sound: this synth only ever plays one sine-wave voice type, so every
// note/channel is playable - no need to distinguish sounds the way a multi-timbral synth would.
class SineSynthSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int /*midiNoteNumber*/) override { return true; }
    bool appliesToChannel(int /*midiChannel*/) override { return true; }
};

}
