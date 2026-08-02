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
        parameterIdPrefix + Parameters::OSC_FOLD_PERCENT_SUFFIX, parameterIdPrefix + Parameters::OSC_SHAPE_NOISE_PERCENT_SUFFIX,
        parameterIdPrefix + Parameters::OSC_PHASE_PERCENT_SUFFIX, parameterIdPrefix + Parameters::OSC_PHASE_RANDOMIZE_ENABLED_SUFFIX),
    _shapeNoiseDial(parameterManager, parameterIdPrefix + Parameters::OSC_SHAPE_NOISE_PERCENT_SUFFIX),
    _octaveDial(parameterManager, parameterIdPrefix + Parameters::OSC_OCTAVE_SUFFIX, "oct"),
    _transposeDial(parameterManager, parameterIdPrefix + Parameters::OSC_TRANSPOSE_SEMITONES_SUFFIX, "st"),
    _detuneDial(parameterManager, parameterIdPrefix + Parameters::OSC_DETUNE_CENTS_SUFFIX, "ct"),
    _warpDial(parameterManager, parameterIdPrefix + Parameters::OSC_WARP_PERCENT_SUFFIX, "%"),
    _foldDial(parameterManager, parameterIdPrefix + Parameters::OSC_FOLD_PERCENT_SUFFIX),
    _unisonVoicesDial(parameterManager, parameterIdPrefix + Parameters::OSC_UNISON_VOICES_SUFFIX, "vox"),
    _unisonDetuneDial(parameterManager, parameterIdPrefix + Parameters::OSC_UNISON_DETUNE_CENTS_SUFFIX, "ct"),
    _unisonStereoDial(parameterManager, parameterIdPrefix + Parameters::OSC_UNISON_STEREO_PERCENT_SUFFIX),
    _phaseRandomizeToggle(parameterManager, parameterIdPrefix + Parameters::OSC_PHASE_RANDOMIZE_ENABLED_SUFFIX, nui::Icons::getPowerOff()), // placeholder icon, to be replaced
    _phaseDial(parameterManager, parameterIdPrefix + Parameters::OSC_PHASE_PERCENT_SUFFIX)
{
    addAndMakeVisible(_shapeCycler);
    addAndMakeVisible(_curve);
    addAndMakeVisible(_shapeNoiseDial);
    addAndMakeVisible(_octaveDial);
    addAndMakeVisible(_transposeDial);
    addAndMakeVisible(_detuneDial);
    addAndMakeVisible(_warpDial);
    addAndMakeVisible(_foldDial);
    addAndMakeVisible(_unisonVoicesDial);
    addAndMakeVisible(_unisonDetuneDial);
    addAndMakeVisible(_unisonStereoDial);
    addAndMakeVisible(_phaseRandomizeToggle);
    addAndMakeVisible(_phaseDial);

    _phaseRandomizeToggle.setIconSize(14.f);

    // Oscillators use the secondary accent color (every other dial in the app defaults to the
    // primary accent) so the two Oscillator sections read as visually distinct from ADSR/LFO/Filter.
    _shapeNoiseDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _octaveDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _transposeDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _detuneDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _warpDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _foldDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _unisonVoicesDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _unisonDetuneDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _unisonStereoDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _phaseDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);

    setGap(8.f);
    setLayoutMargin(kSynthSectionPadding);
    getLayout().setDisplayGrid(false);
    // Only one toggle left in this row (Phase Randomize) now that Sub Octave moved out, so this
    // is back to a single plain 90px row - no need for the split-sub-row/stacked-toggle trick
    // SynthLfoSection's Mode/Sync toggles still use.
    getLayout().init({ 1, 3, 1, 1 }, { 1, 1, 1, 1, 1, 1 });
    getLayout().setFixedRowHeight(0, 28.f);
    getLayout().setFixedRowHeight(2, 90.f);
    getLayout().setFixedRowHeight(3, 90.f);

    getLayout().addComponent(_shapeCycler, 0, 0, 6, 1);
    getLayout().addComponent(_curve, 1, 0, 6, 1);
    getLayout().addComponent(_shapeNoiseDial, 2, 0, 1, 1);
    getLayout().addComponent(_octaveDial, 2, 1, 1, 1);
    getLayout().addComponent(_transposeDial, 2, 2, 1, 1);
    getLayout().addComponent(_detuneDial, 2, 3, 1, 1);
    getLayout().addComponent(_warpDial, 2, 4, 1, 1);
    getLayout().addComponent(_foldDial, 2, 5, 1, 1);
    getLayout().addComponent(_unisonVoicesDial, 3, 0, 1, 1);
    getLayout().addComponent(_unisonDetuneDial, 3, 1, 1, 1);
    getLayout().addComponent(_unisonStereoDial, 3, 2, 1, 1);
    getLayout().addComponent(_phaseRandomizeToggle, 3, 3, 1, 1);
    getLayout().addComponent(_phaseDial, 3, 4, 1, 1);

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
    _transposeDial.setDisplayLabel(juce::translate("synth_osc_transpose_label"));
    _detuneDial.setDisplayLabel(juce::translate("synth_osc_detune_label"));
    _warpDial.setDisplayLabel(juce::translate("synth_osc_warp_label"));
    _foldDial.setDisplayLabel(juce::translate("synth_osc_fold_label"));
    _unisonVoicesDial.setDisplayLabel(juce::translate("synth_osc_unison_voices_label"));
    _unisonDetuneDial.setDisplayLabel(juce::translate("synth_osc_unison_detune_label"));
    _unisonStereoDial.setDisplayLabel(juce::translate("synth_osc_unison_stereo_label"));
    _phaseDial.setDisplayLabel(juce::translate("synth_osc_phase_label"));
}

}
