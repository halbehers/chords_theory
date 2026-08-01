#pragma once

#include <string>
#include <vector>

#include "Theory/Chord.h"

namespace theory
{

// Stateless utility class for converting between chords.json's bare pitch-class name strings and
// actual MIDI note numbers. No instance state - every member is static, same shape as this
// project's Parameters struct.
class NoteConvertor
{
public:
    static constexpr int kMiddleC = 60; // MIDI note number for C4

    // Parses a bare pitch-class name string ("C", "Bb", "F#", "Ebb", "C##", ...) into 0-11.
    // Letter -> base pitch class (C=0, D=2, E=4, F=5, G=7, A=9, B=11), then applies the
    // accidental suffix (a run of '#' or 'b' characters - chords.json never mixes the two within
    // one note name), summed and wrapped mod 12.
    static int parsePitchClass(const std::string& noteName);

    // Closed-voicing-near-middle-C placement: given N chord tones as pitch classes in chords.json
    // array order (bass-note-first for inversions), returns N MIDI note numbers such that:
    //   - the first tone becomes the lowest sounding note, placed at the highest MIDI value <=
    //     kMiddleC that matches its pitch class (i.e. at or just below C4);
    //   - each subsequent tone is placed at the lowest MIDI value strictly above the previous
    //     tone's MIDI value that matches its own pitch class - guaranteeing monotonically
    //     ascending output (no crossed voices) while keeping each tone within an octave of its
    //     neighbor, which is what "closed voicing" means in practice.
    // Contract: output.size() == chordTonePitchClasses.size(), output is strictly ascending,
    // output.front() is in [kMiddleC - 11, kMiddleC].
    static std::vector<int> voiceChordCloseToMiddleC(const std::vector<int>& chordTonePitchClasses);

    // Convenience overload: extracts chord.notes' pitch classes (in array order) and voices them,
    // same contract as above. Shared by MidiExporter and the chord audio-preview path so both
    // "turn a Chord into playable MIDI notes" call sites go through one implementation.
    static std::vector<int> voiceChordCloseToMiddleC(const Chord& chord);
};

}
