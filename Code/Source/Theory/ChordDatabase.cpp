#include "Theory/ChordDatabase.h"

#include <algorithm>

#include <juce_core/juce_core.h>

#include "BinaryData.h"
#include "Theory/Degree.h"

namespace theory
{

namespace
{
    Chord parseChord(const std::string& symbol, const juce::var& chordVar)
    {
        Chord chord;
        chord.symbol = symbol;
        chord.readableName = chordVar["readableName"].toString().toStdString();
        chord.type = parseChordType(chordVar["type"].toString().toStdString());
        chord.popularityOrder = static_cast<int>(chordVar["popularityOrder"]);

        const auto notesVar = chordVar["notes"];
        for (int i = 0; i < notesVar.size(); ++i)
        {
            const auto noteVar = notesVar[i];

            NoteName note;
            note.rawNote = noteVar["note"].toString().toStdString();
            note.readableNote = noteVar["readableNote"].toString().toStdString();
            note.positionInChord = static_cast<int>(noteVar["positionInChord"]);
            chord.notes.push_back(std::move(note));
        }

        return chord;
    }

    ScaleDegreeData parseDegree(Degree degree, const juce::var& degreeVar)
    {
        ScaleDegreeData data;
        data.degree = degree;

        if (auto* obj = degreeVar.getDynamicObject())
        {
            for (const auto& property : obj->getProperties())
                data.chords.push_back(parseChord(property.name.toString().toStdString(), property.value));
        }

        std::sort(data.chords.begin(), data.chords.end(),
                   [](const Chord& a, const Chord& b) { return a.popularityOrder < b.popularityOrder; });

        return data;
    }

    std::vector<NoteName> parseScaleNotes(const juce::var& notesVar, const juce::var& readableNotesVar)
    {
        std::vector<NoteName> notes;

        for (int i = 0; i < notesVar.size(); ++i)
        {
            NoteName note;
            note.rawNote = notesVar[i].toString().toStdString();
            note.readableNote = i < readableNotesVar.size() ? readableNotesVar[i].toString().toStdString() : note.rawNote;
            note.positionInChord = i + 1; // 1-based position within the scale, not a chord-tone role
            notes.push_back(std::move(note));
        }

        return notes;
    }

    KeyScaleData parseKeyScaleData(Key key, Scale scale, const juce::var& scaleVar)
    {
        KeyScaleData data;
        data.key = key;
        data.scale = scale;

        const auto moodsVar = scaleVar["moods"];
        for (int i = 0; i < 5 && i < moodsVar.size(); ++i)
            data.moods[static_cast<std::size_t>(i)] = moodsVar[i].toString().toStdString();

        data.scaleNotes = parseScaleNotes(scaleVar["notes"], scaleVar["readableNotes"]);

        for (int degreeIndex = 0; degreeIndex < kNumDegrees; ++degreeIndex)
        {
            const auto degree = static_cast<Degree>(degreeIndex);
            const auto degreeVar = scaleVar[juce::Identifier(getDegreeLabel(degree))];

            // Minor Blues only has I/IV/V in the source data - every other scale has all 7.
            if (degreeVar.isVoid())
                continue;

            data.degrees.push_back(parseDegree(degree, degreeVar));
        }

        return data;
    }
}

ChordDatabase::ChordDatabase()
{
    const auto jsonText = juce::String::createStringFromData(BinaryData::chords_json, BinaryData::chords_jsonSize);
    const auto root = juce::JSON::parse(jsonText);

    for (int keyIndex = 0; keyIndex < kNumKeys; ++keyIndex)
    {
        const auto key = static_cast<Key>(keyIndex);
        const auto keyVar = root[juce::Identifier(getKeyJsonKey(key))];

        for (int scaleIndex = 0; scaleIndex < kNumScales; ++scaleIndex)
        {
            const auto scale = static_cast<Scale>(scaleIndex);
            const auto scaleVar = keyVar[juce::Identifier(getScaleJsonKey(scale))];

            _index[static_cast<std::size_t>(keyIndex) * static_cast<std::size_t>(kNumScales) + static_cast<std::size_t>(scaleIndex)]
                = parseKeyScaleData(key, scale, scaleVar);
        }
    }
}

const ChordDatabase& ChordDatabase::getInstance()
{
    static const ChordDatabase instance;
    return instance;
}

const KeyScaleData& ChordDatabase::get(Key key, Scale scale) const
{
    const auto keyIndex = static_cast<std::size_t>(key);
    const auto scaleIndex = static_cast<std::size_t>(scale);
    return _index[keyIndex * static_cast<std::size_t>(kNumScales) + scaleIndex];
}

}
