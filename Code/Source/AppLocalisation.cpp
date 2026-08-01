#include "AppLocalisation.h"

#include <juce_core/juce_core.h>

#include <memory>
#include <unordered_map>

#include "BinaryData.h"

namespace
{
    struct LanguageResource { const char* data; int size; };

    const std::unordered_map<std::string, LanguageResource>& getLanguageResources()
    {
        static const std::unordered_map<std::string, LanguageResource> resources = {
            { "fr", { BinaryData::fr_lang, BinaryData::fr_langSize } },
            { "es", { BinaryData::es_lang, BinaryData::es_langSize } },
            { "de", { BinaryData::de_lang, BinaryData::de_langSize } },
            { "it", { BinaryData::it_lang, BinaryData::it_langSize } },
            { "pt", { BinaryData::pt_lang, BinaryData::pt_langSize } },
        };
        return resources;
    }
}

juce::ChangeBroadcaster AppLocalisation::_changeBroadcaster;

void AppLocalisation::setLanguage(const std::string& languageCode)
{
    auto english = std::make_unique<juce::LocalisedStrings>(
        juce::String::createStringFromData(BinaryData::en_lang, BinaryData::en_langSize), true);

    const auto& resources = getLanguageResources();
    const auto it = resources.find(languageCode);

    if (it != resources.end())
    {
        auto localised = std::make_unique<juce::LocalisedStrings>(
            juce::String::createStringFromData(it->second.data, it->second.size), true);
        localised->setFallback(english.release());
        juce::LocalisedStrings::setCurrentMappings(localised.release());
    }
    else
    {
        juce::LocalisedStrings::setCurrentMappings(english.release());
    }

    _changeBroadcaster.sendChangeMessage();
}

juce::ChangeBroadcaster& AppLocalisation::getChangeBroadcaster()
{
    return _changeBroadcaster;
}
