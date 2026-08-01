#include "Theory/NoteConvertor.h"

#include <cctype>
#include <stdexcept>

namespace theory
{

namespace
{
    // Wraps x into [0, 12) - std::% keeps the sign of its left operand in C++, so a plain x % 12
    // returns a negative result for negative x, which every mod-12 computation below relies on
    // NOT happening.
    int mod12(int x)
    {
        const int m = x % 12;
        return m < 0 ? m + 12 : m;
    }

    int getBasePitchClass(char letter)
    {
        switch (letter)
        {
            case 'C': return 0;
            case 'D': return 2;
            case 'E': return 4;
            case 'F': return 5;
            case 'G': return 7;
            case 'A': return 9;
            case 'B': return 11;
            default:  throw std::invalid_argument(std::string("Invalid note letter: ") + letter);
        }
    }
}

int NoteConvertor::parsePitchClass(const std::string& noteName)
{
    if (noteName.empty())
        throw std::invalid_argument("Empty note name");

    int pitchClass = getBasePitchClass(static_cast<char>(std::toupper(static_cast<unsigned char>(noteName[0]))));

    for (std::size_t i = 1; i < noteName.size(); ++i)
    {
        if (noteName[i] == '#')
            ++pitchClass;
        else if (noteName[i] == 'b')
            --pitchClass;
        else
            throw std::invalid_argument("Invalid accidental in note name: " + noteName);
    }

    return mod12(pitchClass);
}

std::vector<int> NoteConvertor::voiceChordCloseToMiddleC(const std::vector<int>& chordTonePitchClasses)
{
    std::vector<int> result;
    result.reserve(chordTonePitchClasses.size());

    for (const int pitchClass : chordTonePitchClasses)
    {
        if (result.empty())
        {
            result.push_back(kMiddleC - mod12(kMiddleC - pitchClass));
            continue;
        }

        const int previousMidiNote = result.back();
        const int candidateBase = previousMidiNote + 1;
        const int distanceToNextMatch = mod12(pitchClass - candidateBase);
        result.push_back(candidateBase + distanceToNextMatch);
    }

    return result;
}

std::vector<int> NoteConvertor::voiceChordCloseToMiddleC(const Chord& chord)
{
    std::vector<int> pitchClasses;
    pitchClasses.reserve(chord.notes.size());

    for (const auto& note : chord.notes)
        pitchClasses.push_back(note.getPitchClass());

    return voiceChordCloseToMiddleC(pitchClasses);
}

}
