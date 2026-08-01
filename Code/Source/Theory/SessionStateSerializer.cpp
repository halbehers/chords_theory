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
    const juce::Identifier kProgressionSlotsTag { "ProgressionSlots" };
    const juce::Identifier kSlotTag { "Slot" };
    const juce::Identifier kIndexProp { "index" };

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

    juce::ValueTree slotsTree(kProgressionSlotsTag);
    for (const auto& [index, degree] : state.progressionSlots)
    {
        juce::ValueTree slotTree(kSlotTag);
        slotTree.setProperty(kIndexProp, index, nullptr);
        slotTree.setProperty(kDegreeProp, juce::String(getDegreeLabel(degree)), nullptr);
        slotsTree.appendChild(slotTree, nullptr);
    }
    stateTree.appendChild(slotsTree, nullptr);

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

    for (const auto& slotTree : tree.getChildWithName(kProgressionSlotsTag))
    {
        Degree degree;
        if (!tryParse(slotTree.getProperty(kDegreeProp).toString(), parseDegree, degree))
            continue;

        state.progressionSlots.emplace_back(static_cast<int>(slotTree.getProperty(kIndexProp)), degree);
    }

    return state;
}

}
