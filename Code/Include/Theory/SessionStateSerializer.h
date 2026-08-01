#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include "Theory/SessionState.h"

namespace theory
{

// Converts SessionState to/from the "ChordsTheoryState" juce::ValueTree child that AppLayout
// splices into the plugin's own APVTS state tree (see ndsp::ParameterManager::getState()) - since
// that whole tree, including this child, is what PluginAudioProcessor::getStateInformation/
// setStateInformation serialize, this is what makes session state survive a DAW project
// close/reopen.
class SessionStateSerializer
{
public:
    static const juce::Identifier kStateTag;

    static juce::ValueTree toValueTree(const SessionState& state);

    // Returns a default-constructed SessionState (Key::C/Scale::Major, no voicings/slots) if
    // `tree` is invalid or isn't a kStateTag node. Malformed child entries (unrecognized key/
    // scale/degree names) are skipped individually rather than aborting the whole parse.
    static SessionState fromValueTree(const juce::ValueTree& tree);
};

}
