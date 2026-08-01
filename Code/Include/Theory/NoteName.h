#pragma once

#include <string>

namespace theory
{

struct NoteName
{
    std::string rawNote;       // chords.json "note" - may contain double accidentals, e.g. "Ebb"
    std::string readableNote;  // chords.json "readableNote" - simplified display spelling
    int positionInChord = 0;   // chord-tone role: 1,3,5,6,7,9,11,13, or 2/4 for sus chords -
                                // NOT array order and NOT an octave/register hint

    // Parses rawNote (via NoteConvertor::parsePitchClass) into a 0-11 pitch class.
    [[nodiscard]] int getPitchClass() const;
};

}
