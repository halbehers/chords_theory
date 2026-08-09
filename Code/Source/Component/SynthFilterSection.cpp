#include "Component/SynthFilterSection.h"

#include "AppLocalisation.h"
#include "Component/SynthLayoutConstants.h"
#include "Parameters.h"

namespace component
{

SynthFilterSection::SynthFilterSection(const std::string& identifier, ndsp::ParameterManager& parameterManager):
    SynthSection(identifier, parameterManager),
    _typeCycler(parameterManager, Parameters::FILTER_TYPE_ID),
    _slopeCycler(parameterManager, Parameters::FILTER_SLOPE_ID),
    _responseCurve(parameterManager, identifier + "-curve", Parameters::FILTER_TYPE_ID, Parameters::FILTER_SLOPE_ID,
        Parameters::FILTER_CUTOFF_HZ_ID, Parameters::FILTER_RESONANCE_ID),
    _cutoffDial(parameterManager, Parameters::FILTER_CUTOFF_HZ_ID),
    _resonanceDial(parameterManager, Parameters::FILTER_RESONANCE_ID),
    _driveDial(parameterManager, Parameters::FILTER_DRIVE_DB_ID, "dB"),
    _mixDial(parameterManager, Parameters::FILTER_MIX_PERCENT_ID),
    _keyTrackDial(parameterManager, Parameters::FILTER_KEY_TRACK_PERCENT_ID)
{
    addAndMakeVisible(_typeCycler);
    addAndMakeVisible(_slopeCycler);
    addAndMakeVisible(_responseCurve);
    addAndMakeVisible(_cutoffDial);
    addAndMakeVisible(_resonanceDial);
    addAndMakeVisible(_driveDial);
    addAndMakeVisible(_mixDial);
    addAndMakeVisible(_keyTrackDial);

    _responseCurve.displayBackground(nui::Theme::ThemeColor::BACKGROUND, nui::Theme::getBorderRadius());
    _typeCycler.setBackgroundColour(nui::Theme::ThemeColor::BACKGROUND);
    _slopeCycler.setBackgroundColour(nui::Theme::ThemeColor::BACKGROUND);

    _cutoffDial.setHeightType(nui::Theme::DialHeightType::MEDIUM);
    _resonanceDial.setHeightType(nui::Theme::DialHeightType::MEDIUM);
    _driveDial.setHeightType(nui::Theme::DialHeightType::MEDIUM);
    _mixDial.setHeightType(nui::Theme::DialHeightType::MEDIUM);
    _keyTrackDial.setHeightType(nui::Theme::DialHeightType::MEDIUM);

    _cutoffDial.setLabelGap(8.f);
    _resonanceDial.setLabelGap(8.f);
    _driveDial.setLabelGap(8.f);
    _mixDial.setLabelGap(8.f);
    _keyTrackDial.setLabelGap(8.f);

    setGap(8.f);
    setLayoutMargin(kSynthSectionPadding);
    getLayout().setDisplayGrid(false);
    getLayout().init({ 1, 3, 1 }, { 1, 1, 1, 1, 1 });
    getLayout().setFixedRowHeight(0, 28.f);
    getLayout().setFixedRowHeight(2, 75.f);

    getLayout().addComponent(_typeCycler, 0, 0, 2, 1);
    getLayout().addComponent(_slopeCycler, 0, 2, 3, 1);
    getLayout().addComponent(_responseCurve, 1, 0, 5, 1);
    getLayout().addComponent(_cutoffDial, 2, 0, 1, 1);
    getLayout().addComponent(_resonanceDial, 2, 1, 1, 1);
    getLayout().addComponent(_driveDial, 2, 2, 1, 1);
    getLayout().addComponent(_mixDial, 2, 3, 1, 1);
    getLayout().addComponent(_keyTrackDial, 2, 4, 1, 1);

    initSection();
}

SynthFilterSection::~SynthFilterSection()
{
    AppLocalisation::getChangeBroadcaster().removeChangeListener(this);
}

std::string SynthFilterSection::getSectionName()
{
    return juce::translate("synth_filter_section_title").toStdString();
}

void SynthFilterSection::refreshLabels()
{
    _cutoffDial.setDisplayLabel(juce::translate("synth_filter_cutoff_label"));
    _resonanceDial.setDisplayLabel(juce::translate("synth_filter_resonance_label"));
    _driveDial.setDisplayLabel(juce::translate("synth_filter_drive_label"));
    _mixDial.setDisplayLabel(juce::translate("synth_filter_mix_label"));
    _keyTrackDial.setDisplayLabel(juce::translate("synth_filter_key_track_label"));
}

}
