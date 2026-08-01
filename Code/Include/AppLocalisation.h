#pragma once

#include <juce_events/juce_events.h>
#include <string>

class AppLocalisation
{
public:
    static void setLanguage(const std::string& languageCode);

    [[nodiscard]] static juce::ChangeBroadcaster& getChangeBroadcaster();

private:
    static juce::ChangeBroadcaster _changeBroadcaster;
};
