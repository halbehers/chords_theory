#pragma once

#include <string>
#include <utility>
#include <vector>

#include "Theory/Degree.h"
#include "Theory/Key.h"
#include "Theory/Scale.h"

namespace theory
{

// Everything the user has set in a session that needs to survive a DAW project close/reopen:
// Key, Scale, the chosen chord (symbol) for every scale degree, and the full contents of the
// progression sequencer. Pure data - no UI/JUCE-component dependency, so it's directly testable;
// AppLayout is responsible for gathering one of these from the live UI and applying one back.
struct SessionState
{
    Key key = Key::C;
    Scale scale = Scale::Major;
    std::vector<std::pair<Degree, std::string>> degreeVoicings; // degree -> chord symbol
    std::vector<std::pair<int, Degree>> progressionSlots;       // slot index -> degree (only occupied slots)

    bool operator==(const SessionState& other) const
    {
        return key == other.key && scale == other.scale
            && degreeVoicings == other.degreeVoicings && progressionSlots == other.progressionSlots;
    }
};

}
