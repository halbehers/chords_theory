#include "Component/SynthOscillatorSection.h"

#include "AppLocalisation.h"
#include "Component/SynthLayoutConstants.h"
#include "Parameters.h"

namespace component
{

SynthOscillatorSection::SynthOscillatorSection(const std::string& identifier, ndsp::ParameterManager& parameterManager,
                                                 const std::string& parameterIdPrefix, const std::string& sectionTitleKey):
    SynthSection(identifier, parameterManager),
    _sectionTitleKey(sectionTitleKey),
    _shapeCycler(parameterManager, parameterIdPrefix + Parameters::OSC_SHAPE_SUFFIX),
    _curve(parameterManager, identifier + "-curve",
        parameterIdPrefix + Parameters::OSC_SHAPE_SUFFIX, parameterIdPrefix + Parameters::OSC_WARP_PERCENT_SUFFIX,
        parameterIdPrefix + Parameters::OSC_FOLD_PERCENT_SUFFIX),
    _shapeNoiseDial(parameterManager, parameterIdPrefix + Parameters::OSC_SHAPE_NOISE_PERCENT_SUFFIX),
    _octaveDial(parameterManager, parameterIdPrefix + Parameters::OSC_OCTAVE_SUFFIX, "oct"),
    _detuneDial(parameterManager, parameterIdPrefix + Parameters::OSC_DETUNE_CENTS_SUFFIX, "ct"),
    _warpDial(parameterManager, parameterIdPrefix + Parameters::OSC_WARP_PERCENT_SUFFIX, "%"),
    _foldDial(parameterManager, parameterIdPrefix + Parameters::OSC_FOLD_PERCENT_SUFFIX),
    _outputDial(parameterManager, parameterIdPrefix + Parameters::OSC_OUTPUT_DB_SUFFIX, "dB"),
    _unisonVoicesDial(parameterManager, parameterIdPrefix + Parameters::OSC_UNISON_VOICES_SUFFIX, "vox"),
    _unisonDetuneDial(parameterManager, parameterIdPrefix + Parameters::OSC_UNISON_DETUNE_CENTS_SUFFIX, "ct"),
    _unisonStereoDial(parameterManager, parameterIdPrefix + Parameters::OSC_UNISON_STEREO_PERCENT_SUFFIX),
    _subLevelDial(parameterManager, parameterIdPrefix + Parameters::OSC_SUB_LEVEL_PERCENT_SUFFIX),
    _subOctaveToggle(parameterManager, parameterIdPrefix + Parameters::OSC_SUB_OCTAVE_DOWN2_ENABLED_SUFFIX, nui::Icons::getLink()), // placeholder icon, to be replaced
    _phaseRandomizeToggle(parameterManager, parameterIdPrefix + Parameters::OSC_PHASE_RANDOMIZE_ENABLED_SUFFIX, nui::Icons::getPowerOff()), // placeholder icon, to be replaced
    _phaseDial(parameterManager, parameterIdPrefix + Parameters::OSC_PHASE_PERCENT_SUFFIX)
{
    addAndMakeVisible(_shapeCycler);
    addAndMakeVisible(_curve);
    addAndMakeVisible(_shapeNoiseDial);
    addAndMakeVisible(_octaveDial);
    addAndMakeVisible(_detuneDial);
    addAndMakeVisible(_warpDial);
    addAndMakeVisible(_foldDial);
    addAndMakeVisible(_outputDial);
    addAndMakeVisible(_unisonVoicesDial);
    addAndMakeVisible(_unisonDetuneDial);
    addAndMakeVisible(_unisonStereoDial);
    addAndMakeVisible(_subLevelDial);
    addAndMakeVisible(_subOctaveToggle);
    addAndMakeVisible(_phaseRandomizeToggle);
    addAndMakeVisible(_phaseDial);

    _subOctaveToggle.setIconSize(14.f);
    _phaseRandomizeToggle.setIconSize(14.f);

    // Oscillators use the secondary accent color (every other dial in the app defaults to the
    // primary accent) so the two Oscillator sections read as visually distinct from ADSR/LFO/Filter.
    _shapeNoiseDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _octaveDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _detuneDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _warpDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _foldDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _outputDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _unisonVoicesDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _unisonDetuneDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _unisonStereoDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _subLevelDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _phaseDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);

    setGap(8.f);
    setLayoutMargin(kSynthSectionPadding);
    getLayout().setDisplayGrid(false);
    // Row 3/4 together are one visual row of 6 slots - split in two so the Sub Octave/Phase
    // Randomize toggles can stack in one narrow column (col 4, same trick as SynthLfoSection's
    // Mode/Sync toggles) while every dial in that row spans both sub-rows (height 2) to stay a
    // full 90px tall, matching row 2's plain dial row.
    getLayout().init({ 1, 3, 1, 1, 1 }, { 1, 1, 1, 1, 1, 1 });
    getLayout().setFixedRowHeight(0, 28.f);
    getLayout().setFixedRowHeight(2, 90.f);
    getLayout().setFixedRowHeight(3, 45.f);
    getLayout().setFixedRowHeight(4, 45.f);

    getLayout().addComponent(_shapeCycler, 0, 0, 6, 1);
    getLayout().addComponent(_curve, 1, 0, 6, 1);
    getLayout().addComponent(_shapeNoiseDial, 2, 0, 1, 1);
    getLayout().addComponent(_octaveDial, 2, 1, 1, 1);
    getLayout().addComponent(_detuneDial, 2, 2, 1, 1);
    getLayout().addComponent(_warpDial, 2, 3, 1, 1);
    getLayout().addComponent(_foldDial, 2, 4, 1, 1);
    getLayout().addComponent(_outputDial, 2, 5, 1, 1);
    getLayout().addComponent(_unisonVoicesDial, 3, 0, 1, 2);
    getLayout().addComponent(_unisonDetuneDial, 3, 1, 1, 2);
    getLayout().addComponent(_unisonStereoDial, 3, 2, 1, 2);
    getLayout().addComponent(_subLevelDial, 3, 3, 1, 2);
    getLayout().addComponent(_subOctaveToggle, 3, 4, 1, 1);
    getLayout().addComponent(_phaseRandomizeToggle, 4, 4, 1, 1);
    getLayout().addComponent(_phaseDial, 3, 5, 1, 2);

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
    _shapeNoiseDial.setDisplayLabel(juce::translate("synth_osc_shape_noise_label"));
    _octaveDial.setDisplayLabel(juce::translate("synth_osc_octave_label"));
    _detuneDial.setDisplayLabel(juce::translate("synth_osc_detune_label"));
    _warpDial.setDisplayLabel(juce::translate("synth_osc_warp_label"));
    _foldDial.setDisplayLabel(juce::translate("synth_osc_fold_label"));
    _outputDial.setDisplayLabel(juce::translate("synth_osc_output_label"));
    _unisonVoicesDial.setDisplayLabel(juce::translate("synth_osc_unison_voices_label"));
    _unisonDetuneDial.setDisplayLabel(juce::translate("synth_osc_unison_detune_label"));
    _unisonStereoDial.setDisplayLabel(juce::translate("synth_osc_unison_stereo_label"));
    _subLevelDial.setDisplayLabel(juce::translate("synth_osc_sub_level_label"));
    _phaseDial.setDisplayLabel(juce::translate("synth_osc_phase_label"));
}

}
