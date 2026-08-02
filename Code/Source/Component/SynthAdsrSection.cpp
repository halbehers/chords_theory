#include "Component/SynthAdsrSection.h"

#include "AppLocalisation.h"
#include "Component/SynthLayoutConstants.h"
#include "Parameters.h"

namespace component
{

SynthAdsrSection::SynthAdsrSection(const std::string& identifier, ndsp::ParameterManager& parameterManager):
    SynthSection(identifier, parameterManager),
    _envelopeCurve(parameterManager, identifier + "-curve",
        { Parameters::ENVELOPE_DELAY_MS_ID, Parameters::ENVELOPE_ATTACK_MS_ID, Parameters::ENVELOPE_HOLD_MS_ID,
          Parameters::ENVELOPE_DECAY_MS_ID, Parameters::ENVELOPE_SUSTAIN_PERCENT_ID, Parameters::ENVELOPE_RELEASE_MS_ID }),
    _delayDial(parameterManager, Parameters::ENVELOPE_DELAY_MS_ID),
    _attackDial(parameterManager, Parameters::ENVELOPE_ATTACK_MS_ID),
    _holdDial(parameterManager, Parameters::ENVELOPE_HOLD_MS_ID),
    _decayDial(parameterManager, Parameters::ENVELOPE_DECAY_MS_ID),
    _sustainDial(parameterManager, Parameters::ENVELOPE_SUSTAIN_PERCENT_ID),
    _releaseDial(parameterManager, Parameters::ENVELOPE_RELEASE_MS_ID)
{
    addAndMakeVisible(_envelopeCurve);
    addAndMakeVisible(_delayDial);
    addAndMakeVisible(_attackDial);
    addAndMakeVisible(_holdDial);
    addAndMakeVisible(_decayDial);
    addAndMakeVisible(_sustainDial);
    addAndMakeVisible(_releaseDial);

    _delayDial.setHeightType(nui::Theme::HeightType::LARGE);
    _attackDial.setHeightType(nui::Theme::HeightType::LARGE);
    _holdDial.setHeightType(nui::Theme::HeightType::LARGE);
    _decayDial.setHeightType(nui::Theme::HeightType::LARGE);
    _sustainDial.setHeightType(nui::Theme::HeightType::LARGE);
    _releaseDial.setHeightType(nui::Theme::HeightType::LARGE);

    _envelopeCurve.displayBackground(nui::Theme::ThemeColor::BACKGROUND, nui::Theme::getBorderRadius());

    setGap(8.f);
    setLayoutMargin(kSynthSectionPadding);
    getLayout().setDisplayGrid(false);
    getLayout().init({ 3, 1 }, { 1, 1, 1, 1, 1, 1 });
    getLayout().setFixedRowHeight(1, 90.f);

    getLayout().addComponent(_envelopeCurve, 0, 0, 6, 1);
    getLayout().addComponent(_delayDial, 1, 0, 1, 1);
    getLayout().addComponent(_attackDial, 1, 1, 1, 1);
    getLayout().addComponent(_holdDial, 1, 2, 1, 1);
    getLayout().addComponent(_decayDial, 1, 3, 1, 1);
    getLayout().addComponent(_sustainDial, 1, 4, 1, 1);
    getLayout().addComponent(_releaseDial, 1, 5, 1, 1);

    initSection();
}

SynthAdsrSection::~SynthAdsrSection()
{
    AppLocalisation::getChangeBroadcaster().removeChangeListener(this);
}

std::string SynthAdsrSection::getSectionName()
{
    return juce::translate("synth_adsr_section_title").toStdString();
}

void SynthAdsrSection::refreshLabels()
{
    _delayDial.setDisplayLabel(juce::translate("synth_adsr_delay_label"));
    _attackDial.setDisplayLabel(juce::translate("synth_adsr_attack_label"));
    _holdDial.setDisplayLabel(juce::translate("synth_adsr_hold_label"));
    _decayDial.setDisplayLabel(juce::translate("synth_adsr_decay_label"));
    _sustainDial.setDisplayLabel(juce::translate("synth_adsr_sustain_label"));
    _releaseDial.setDisplayLabel(juce::translate("synth_adsr_release_label"));
}

}
