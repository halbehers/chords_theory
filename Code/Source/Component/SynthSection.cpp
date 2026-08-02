#include "Component/SynthSection.h"

#include "AppLocalisation.h"

namespace component
{

SynthSection::SynthSection(const std::string& identifier, ndsp::ParameterManager& parameterManager, const std::string& enabledParameterID):
    Section(identifier, parameterManager, enabledParameterID)
{
    nui::Component::displayBackground(nui::Theme::SECONDARY_BACKGROUND, nui::Theme::getBorderRadius());
    
    AppLocalisation::getChangeBroadcaster().addChangeListener(this);
}

SynthSection::~SynthSection()
{
    AppLocalisation::getChangeBroadcaster().removeChangeListener(this);
}

void SynthSection::initSection()
{
    setSectionNameFontSize(nui::Theme::PARAGRAPH);
    setSectionName(getSectionName());
    refreshLabels();
}

void SynthSection::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    Section::changeListenerCallback(source);

    if (source != &AppLocalisation::getChangeBroadcaster())
        return;

    setSectionName(getSectionName());
    refreshLabels();
}

}
