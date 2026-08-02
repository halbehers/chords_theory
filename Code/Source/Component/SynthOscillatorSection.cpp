#include "Component/SynthOscillatorSection.h"

#include "AppLocalisation.h"
#include "Component/SynthLayoutConstants.h"
#include "Parameters.h"

namespace component
{

SynthOscillatorSection::SynthOscillatorSection(const std::string& identifier, ndsp::ParameterManager& parameterManager,
                                                 const std::string& parameterIdPrefix, const std::string& sectionTitleKey,
                                                 const std::string& enabledParameterID):
    SynthSection(identifier, parameterManager, enabledParameterID),
    _sectionTitleKey(sectionTitleKey),
    _shapeCycler(parameterManager, parameterIdPrefix + Parameters::OSC_SHAPE_SUFFIX),
    _curve(parameterManager, identifier + "-curve",
        parameterIdPrefix + Parameters::OSC_SHAPE_SUFFIX, parameterIdPrefix + Parameters::OSC_WARP_PERCENT_SUFFIX,
        parameterIdPrefix + Parameters::OSC_FOLD_PERCENT_SUFFIX, parameterIdPrefix + Parameters::OSC_SHAPE_NOISE_PERCENT_SUFFIX,
        parameterIdPrefix + Parameters::OSC_PHASE_PERCENT_SUFFIX, parameterIdPrefix + Parameters::OSC_PHASE_RANDOMIZE_ENABLED_SUFFIX),
    _octaveDial(parameterManager, parameterIdPrefix + Parameters::OSC_OCTAVE_SUFFIX, "oct"),
    _detuneDial(parameterManager, parameterIdPrefix + Parameters::OSC_DETUNE_CENTS_SUFFIX, "ct"),
    _transposeDial(parameterManager, parameterIdPrefix + Parameters::OSC_TRANSPOSE_SEMITONES_SUFFIX, "st"),
    _unisonVoicesDial(parameterManager, parameterIdPrefix + Parameters::OSC_UNISON_VOICES_SUFFIX, "vox"),
    _unisonDetuneDial(parameterManager, parameterIdPrefix + Parameters::OSC_UNISON_DETUNE_CENTS_SUFFIX, "ct"),
    _unisonStereoDial(parameterManager, parameterIdPrefix + Parameters::OSC_UNISON_STEREO_PERCENT_SUFFIX)
{
    addAndMakeVisible(_shapeCycler);
    addAndMakeVisible(_curve);
    addAndMakeVisible(_octaveDial);
    addAndMakeVisible(_detuneDial);
    addAndMakeVisible(_transposeDial);
    addAndMakeVisible(_unisonVoicesDial);
    addAndMakeVisible(_unisonDetuneDial);
    addAndMakeVisible(_unisonStereoDial);

    _octaveDial.setHeightType(nui::Theme::HeightType::LARGE);
    _detuneDial.setHeightType(nui::Theme::HeightType::LARGE);
    _transposeDial.setHeightType(nui::Theme::HeightType::LARGE);
    _unisonVoicesDial.setHeightType(nui::Theme::HeightType::LARGE);
    _unisonDetuneDial.setHeightType(nui::Theme::HeightType::LARGE);
    _unisonStereoDial.setHeightType(nui::Theme::HeightType::LARGE);

    _curve.displayBackground(nui::Theme::ThemeColor::BACKGROUND, nui::Theme::getBorderRadius());
    _shapeCycler.setBackgroundColour(nui::Theme::newColor(nui::Theme::ThemeColor::BACKGROUND).asJuce());
    _octaveDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _detuneDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _transposeDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _unisonVoicesDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _unisonDetuneDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _unisonStereoDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);

    _curve.setValueSettersFontSize(nui::Theme::SMALL);

    setGap(8.f);
    setLayoutMargin(kSynthSectionPadding);
    getLayout().setDisplayGrid(false);
    getLayout().init({ 1, 2, 1, 1 }, { 1, 1, 1, 1, 1, 1 });
    getLayout().setFixedRowHeight(0, 28.f);

    getLayout().addComponent(_shapeCycler, 0, 0, 6, 1);
    getLayout().addComponent(_curve, 1, 0, 6, 1);
    getLayout().addComponent(_octaveDial, 2, 0, 2, 1);
    getLayout().addComponent(_detuneDial, 2, 2, 2, 1);
    getLayout().addComponent(_transposeDial, 2, 4, 2, 1);
    getLayout().addComponent(_unisonVoicesDial, 3, 0, 2, 1);
    getLayout().addComponent(_unisonDetuneDial, 3, 2, 2, 1);
    getLayout().addComponent(_unisonStereoDial, 3, 4, 2, 1);

    registerComponent(_shapeCycler);
    registerComponent(_curve);
    registerComponent(_octaveDial);
    registerComponent(_detuneDial);
    registerComponent(_transposeDial);
    registerComponent(_unisonVoicesDial);
    registerComponent(_unisonDetuneDial);
    registerComponent(_unisonStereoDial);

    if (!enabledParameterID.empty())
    {
        // Section's own bypass wiring only reacts to the enabled toggle's live clicks - its
        // *initial* state needs syncing here from the parameter's actual current (saved or
        // default) value, same reasoning as every other "must match actual state, not just
        // default" sync in this codebase (e.g. SynthLfoSection's sync-division/rate visibility).
        const auto isEnabled = parameterManager.getState().getRawParameterValue(enabledParameterID)->load() > 0.5f;
        setBypass(!isEnabled);
    }

    initSection();
}

SynthOscillatorSection::~SynthOscillatorSection()
{
    AppLocalisation::getChangeBroadcaster().removeChangeListener(this);
}

std::string SynthOscillatorSection::getSectionName()
{
    return juce::translate(_sectionTitleKey).toStdString();
}

void SynthOscillatorSection::refreshLabels()
{
    _octaveDial.setDisplayLabel(juce::translate("synth_osc_octave_label"));
    _detuneDial.setDisplayLabel(juce::translate("synth_osc_detune_label"));
    _transposeDial.setDisplayLabel(juce::translate("synth_osc_transpose_label"));
    _unisonVoicesDial.setDisplayLabel(juce::translate("synth_osc_unison_voices_label"));
    _unisonDetuneDial.setDisplayLabel(juce::translate("synth_osc_unison_detune_label"));
    _unisonStereoDial.setDisplayLabel(juce::translate("synth_osc_unison_stereo_label"));

    _curve.setPhaseDisplayLabel(juce::translate("synth_osc_phase_label"));
    _curve.setNoiseDisplayLabel(juce::translate("synth_osc_shape_noise_label"));
    _curve.setWarpDisplayLabel(juce::translate("synth_osc_warp_label"));
    _curve.setFoldDisplayLabel(juce::translate("synth_osc_fold_label"));
}

}
