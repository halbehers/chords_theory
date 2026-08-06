#include "Component/SynthSubSection.h"

#include "AppLocalisation.h"
#include "Component/SynthLayoutConstants.h"
#include "Parameters.h"

namespace component
{

SynthSubSection::SynthSubSection(const std::string& identifier, ndsp::ParameterManager& parameterManager, const std::string& enabledParameterID):
    SynthSection(identifier, parameterManager, enabledParameterID),
    _octaveDial(parameterManager, Parameters::SUB_OCTAVE_ID, "oct"),
    _transposeDial(parameterManager, Parameters::SUB_TRANSPOSE_SEMITONES_ID, "st"),
    _toneDial(parameterManager, Parameters::SUB_TONE_PERCENT_ID)
{
    addAndMakeVisible(_octaveDial);
    addAndMakeVisible(_transposeDial);
    addAndMakeVisible(_toneDial);

    _octaveDial.setHeightType(nui::Theme::DialHeightType::MEDIUM);
    _transposeDial.setHeightType(nui::Theme::DialHeightType::MEDIUM);
    _toneDial.setHeightType(nui::Theme::DialHeightType::MEDIUM);

    _octaveDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _transposeDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _toneDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);

    _octaveDial.setLabelGap(8.f);
    _transposeDial.setLabelGap(8.f);
    _toneDial.setLabelGap(8.f);

    setGap(8.f);
    setLayoutMargin(kSynthSectionPadding);
    getLayout().setDisplayGrid(false);
    getLayout().init({ 1, 1, 1 }, { 1 });

    getLayout().addComponent(_octaveDial, 0, 0, 1, 1);
    getLayout().addComponent(_transposeDial, 1, 0, 1, 1);
    getLayout().addComponent(_toneDial, 2, 0, 1, 1);

    registerComponent(_octaveDial);
    registerComponent(_transposeDial);
    registerComponent(_toneDial);

    // Section's own bypass wiring only reacts to the enabled toggle's live clicks - its *initial*
    // state needs syncing here from the parameter's actual current (saved or default) value, same
    // reasoning as SynthOscillatorSection's own enabledParameterID handling.
    const auto isEnabled = parameterManager.getState().getRawParameterValue(enabledParameterID)->load() > 0.5f;
    setBypass(!isEnabled);

    initSection();
}

SynthSubSection::~SynthSubSection()
{
    AppLocalisation::getChangeBroadcaster().removeChangeListener(this);
}

std::string SynthSubSection::getSectionName()
{
    return juce::translate("synth_sub_section_title").toStdString();
}

void SynthSubSection::refreshLabels()
{
    _octaveDial.setDisplayLabel(juce::translate("synth_sub_octave_label"));
    _transposeDial.setDisplayLabel(juce::translate("synth_sub_transpose_label"));
    _toneDial.setDisplayLabel(juce::translate("synth_sub_tone_label"));
}

}
