#include "Theory/SessionStateSerializer.h"

#include <stdexcept>

namespace theory
{

namespace
{
    const juce::Identifier kKeyProp { "key" };
    const juce::Identifier kScaleProp { "scale" };
    const juce::Identifier kDegreeVoicingsTag { "DegreeVoicings" };
    const juce::Identifier kDegreeTag { "Degree" };
    const juce::Identifier kDegreeProp { "degree" };
    const juce::Identifier kChordSymbolProp { "chordSymbol" };

    const juce::Identifier kMidiEditorStateTag { "MidiEditorState" };
    const juce::Identifier kNotesTag { "Notes" };
    const juce::Identifier kNoteTag { "Note" };
    const juce::Identifier kMidiNoteProp { "midiNote" };
    const juce::Identifier kStartBeatProp { "startBeat" };
    const juce::Identifier kLengthBeatsProp { "lengthBeats" };

    template <typename T, typename ParseFn>
    bool tryParse(const juce::String& text, ParseFn parse, T& out)
    {
        try
        {
            out = parse(text.toStdString());
            return true;
        }
        catch (const std::invalid_argument&)
        {
            return false;
        }
    }
}

const juce::Identifier SessionStateSerializer::kStateTag { "ChordsTheoryState" };

juce::ValueTree SessionStateSerializer::toValueTree(const SessionState& state)
{
    juce::ValueTree stateTree(kStateTag);
    stateTree.setProperty(kKeyProp, juce::String(getKeyJsonKey(state.key)), nullptr);
    stateTree.setProperty(kScaleProp, juce::String(getScaleJsonKey(state.scale)), nullptr);

    juce::ValueTree degreeVoicingsTree(kDegreeVoicingsTag);
    for (const auto& [degree, chordSymbol] : state.degreeVoicings)
    {
        juce::ValueTree degreeTree(kDegreeTag);
        degreeTree.setProperty(kDegreeProp, juce::String(getDegreeLabel(degree)), nullptr);
        degreeTree.setProperty(kChordSymbolProp, juce::String(chordSymbol), nullptr);
        degreeVoicingsTree.appendChild(degreeTree, nullptr);
    }
    stateTree.appendChild(degreeVoicingsTree, nullptr);

    juce::ValueTree midiEditorStateTree(kMidiEditorStateTag);

    juce::ValueTree notesTree(kNotesTag);
    for (const auto& note : state.progressionEditorState.notes)
    {
        juce::ValueTree noteTree(kNoteTag);
        noteTree.setProperty(kMidiNoteProp, note.midiNote, nullptr);
        noteTree.setProperty(kStartBeatProp, note.startBeat, nullptr);
        noteTree.setProperty(kLengthBeatsProp, note.lengthBeats, nullptr);
        notesTree.appendChild(noteTree, nullptr);
    }
    midiEditorStateTree.appendChild(notesTree, nullptr);

    stateTree.appendChild(midiEditorStateTree, nullptr);

    return stateTree;
}

SessionState SessionStateSerializer::fromValueTree(const juce::ValueTree& tree)
{
    SessionState state;

    if (!tree.isValid() || tree.getType() != kStateTag)
        return state;

    tryParse(tree.getProperty(kKeyProp).toString(), parseKey, state.key);
    tryParse(tree.getProperty(kScaleProp).toString(), parseScale, state.scale);

    for (const auto& degreeTree : tree.getChildWithName(kDegreeVoicingsTag))
    {
        Degree degree;
        if (!tryParse(degreeTree.getProperty(kDegreeProp).toString(), parseDegree, degree))
            continue;

        state.degreeVoicings.emplace_back(degree, degreeTree.getProperty(kChordSymbolProp).toString().toStdString());
    }

    const auto midiEditorStateTree = tree.getChildWithName(kMidiEditorStateTag);

    for (const auto& noteTree : midiEditorStateTree.getChildWithName(kNotesTag))
    {
        MidiEditorNoteState note;
        note.midiNote = static_cast<int>(noteTree.getProperty(kMidiNoteProp));
        note.startBeat = static_cast<double>(noteTree.getProperty(kStartBeatProp));
        note.lengthBeats = static_cast<double>(noteTree.getProperty(kLengthBeatsProp));
        state.progressionEditorState.notes.push_back(note);
    }

    return state;
}

}
