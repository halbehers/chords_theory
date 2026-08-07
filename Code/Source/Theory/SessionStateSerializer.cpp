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
    const juce::Identifier kNextChordBlockIdProp { "nextChordBlockId" };
    const juce::Identifier kNotesTag { "Notes" };
    const juce::Identifier kNoteTag { "Note" };
    const juce::Identifier kMidiNoteProp { "midiNote" };
    const juce::Identifier kStartBeatProp { "startBeat" };
    const juce::Identifier kLengthBeatsProp { "lengthBeats" };
    const juce::Identifier kSourceChordIdProp { "sourceChordId" };
    const juce::Identifier kChordBlocksTag { "ChordBlocks" };
    const juce::Identifier kChordBlockTag { "ChordBlock" };
    const juce::Identifier kIdProp { "id" };
    const juce::Identifier kLabelProp { "label" };
    const juce::Identifier kPopularityOrderProp { "popularityOrder" };

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
    midiEditorStateTree.setProperty(kNextChordBlockIdProp, state.progressionEditorState.nextChordBlockId, nullptr);

    juce::ValueTree notesTree(kNotesTag);
    for (const auto& note : state.progressionEditorState.notes)
    {
        juce::ValueTree noteTree(kNoteTag);
        noteTree.setProperty(kMidiNoteProp, note.midiNote, nullptr);
        noteTree.setProperty(kStartBeatProp, note.startBeat, nullptr);
        noteTree.setProperty(kLengthBeatsProp, note.lengthBeats, nullptr);
        noteTree.setProperty(kSourceChordIdProp, note.sourceChordId, nullptr);
        notesTree.appendChild(noteTree, nullptr);
    }
    midiEditorStateTree.appendChild(notesTree, nullptr);

    juce::ValueTree chordBlocksTree(kChordBlocksTag);
    for (const auto& block : state.progressionEditorState.chordBlocks)
    {
        juce::ValueTree blockTree(kChordBlockTag);
        blockTree.setProperty(kIdProp, block.id, nullptr);
        blockTree.setProperty(kLabelProp, juce::String(block.label), nullptr);
        blockTree.setProperty(kStartBeatProp, block.startBeat, nullptr);
        blockTree.setProperty(kLengthBeatsProp, block.lengthBeats, nullptr);
        blockTree.setProperty(kDegreeProp, juce::String(getDegreeLabel(block.sourceSlot.degree)), nullptr);
        blockTree.setProperty(kPopularityOrderProp, block.sourceSlot.popularityOrder, nullptr);
        chordBlocksTree.appendChild(blockTree, nullptr);
    }
    midiEditorStateTree.appendChild(chordBlocksTree, nullptr);

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
    state.progressionEditorState.nextChordBlockId = static_cast<int>(midiEditorStateTree.getProperty(kNextChordBlockIdProp));

    for (const auto& noteTree : midiEditorStateTree.getChildWithName(kNotesTag))
    {
        MidiEditorNoteState note;
        note.midiNote = static_cast<int>(noteTree.getProperty(kMidiNoteProp));
        note.startBeat = static_cast<double>(noteTree.getProperty(kStartBeatProp));
        note.lengthBeats = static_cast<double>(noteTree.getProperty(kLengthBeatsProp));
        note.sourceChordId = static_cast<int>(noteTree.getProperty(kSourceChordIdProp));
        state.progressionEditorState.notes.push_back(note);
    }

    for (const auto& blockTree : midiEditorStateTree.getChildWithName(kChordBlocksTag))
    {
        Degree degree;
        if (!tryParse(blockTree.getProperty(kDegreeProp).toString(), parseDegree, degree))
            continue;

        MidiEditorChordBlockState block;
        block.id = static_cast<int>(blockTree.getProperty(kIdProp));
        block.label = blockTree.getProperty(kLabelProp).toString().toStdString();
        block.startBeat = static_cast<double>(blockTree.getProperty(kStartBeatProp));
        block.lengthBeats = static_cast<double>(blockTree.getProperty(kLengthBeatsProp));
        block.sourceSlot = ProgressionSlot { degree, static_cast<int>(blockTree.getProperty(kPopularityOrderProp)) };
        state.progressionEditorState.chordBlocks.push_back(block);
    }

    return state;
}

}
