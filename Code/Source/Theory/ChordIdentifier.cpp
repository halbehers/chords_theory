#include "Theory/ChordIdentifier.h"

#include <algorithm>
#include <set>

#include "Theory/ChordDatabase.h"

namespace theory
{

std::optional<DetectedChord> ChordIdentifier::identify(const std::vector<int>& midiNotes, Key key, Scale scale)
{
    if (midiNotes.empty())
        return std::nullopt;

    // midiNotes always come from MidiEditor's own note storage, which never goes negative (its
    // lowest allowed pitch is well above 0) - a plain % 12 is safe here, unlike NoteConvertor's
    // private mod12 which has to defend against negative input.
    const auto bassMidiNote = *std::min_element(midiNotes.begin(), midiNotes.end());
    const auto bassPitchClass = bassMidiNote % 12;

    std::set<int> pitchClassSet;
    for (const auto midiNote : midiNotes)
        pitchClassSet.insert(midiNote % 12);

    for (const auto& degreeData : ChordDatabase::getInstance().get(key, scale).degrees)
    {
        for (const auto& chord : degreeData.chords)
        {
            if (chord.notes.empty() || chord.notes.front().getPitchClass() != bassPitchClass)
                continue;

            std::set<int> chordPitchClasses;
            for (const auto& note : chord.notes)
                chordPitchClasses.insert(note.getPitchClass());

            if (chordPitchClasses == pitchClassSet)
                return DetectedChord { chord.readableName, ProgressionSlot { degreeData.degree, chord.popularityOrder } };
        }
    }

    return std::nullopt;
}

}
