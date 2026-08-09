#include "Component/SynthMixerSection.h"

#include "AppLocalisation.h"
#include "Component/SynthLayoutConstants.h"
#include "Parameters.h"

namespace component
{

SynthMixerSection::SynthMixerSection(const std::string& identifier, ndsp::ParameterManager& parameterManager):
    SynthSection(identifier, parameterManager),
    _algorithmCycler(parameterManager, Parameters::MIXER_ALGORITHM_ID),
    _fmAmountSetter(parameterManager, Parameters::MIXER_FM_AMOUNT_PERCENT_ID, "%"),
    _ringModMixSetter(parameterManager, Parameters::MIXER_RING_MOD_MIX_PERCENT_ID, "%"),
    _amDepthSetter(parameterManager, Parameters::MIXER_AM_DEPTH_PERCENT_ID, "%"),
    _serialFoldAmountSetter(parameterManager, Parameters::MIXER_SERIAL_FOLD_AMOUNT_PERCENT_ID, "%"),
    _subLevelSlider(parameterManager, Parameters::SUB_OUTPUT_DB_ID, "dB"),
    _osc1LevelSlider(parameterManager, std::string(Parameters::OSC1_ID_PREFIX) + Parameters::OSC_OUTPUT_DB_SUFFIX, "dB"),
    _osc2LevelSlider(parameterManager, std::string(Parameters::OSC2_ID_PREFIX) + Parameters::OSC_OUTPUT_DB_SUFFIX, "dB")
{
    addAndMakeVisible(_algorithmCycler);
    addAndMakeVisible(_fmAmountSetter);
    addAndMakeVisible(_ringModMixSetter);
    addAndMakeVisible(_amDepthSetter);
    addAndMakeVisible(_serialFoldAmountSetter);
    addAndMakeVisible(_subLevelSlider);
    addAndMakeVisible(_osc1LevelSlider);
    addAndMakeVisible(_osc2LevelSlider);

    _algorithmCycler.setBackgroundColour(nui::Theme::ThemeColor::BACKGROUND);
    _algorithmCycler.addOnValueChangedListener(this);

    _subLevelSlider.setTrackColour(nui::Theme::ThemeColor::BACKGROUND);
    _subLevelSlider.setThumbColour(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _osc1LevelSlider.setTrackColour(nui::Theme::ThemeColor::BACKGROUND);
    _osc1LevelSlider.setThumbColour(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _osc2LevelSlider.setTrackColour(nui::Theme::ThemeColor::BACKGROUND);
    _osc2LevelSlider.setThumbColour(nui::Theme::ThemeColor::SECONDARY_ACCENT);

    _subLevelSlider.setBorderRadius(4.f);
    _osc1LevelSlider.setBorderRadius(4.f);
    _osc2LevelSlider.setBorderRadius(4.f);
    _subLevelSlider.setThumbBorderRadius(4.f);
    _osc1LevelSlider.setThumbBorderRadius(4.f);
    _osc2LevelSlider.setThumbBorderRadius(4.f);

    for (auto* valueSetter : { &_fmAmountSetter, &_ringModMixSetter, &_amDepthSetter, &_serialFoldAmountSetter })
    {
        valueSetter->setValueBackgroundColour(nui::Theme::ThemeColor::BACKGROUND);
        valueSetter->setDisplayMode(nelement::ValueSetter::DisplayMode::Inline);
        valueSetter->setGap(8.f);
        valueSetter->setValueBackgroundWidth(70.f);
        valueSetter->setValueBackgroundHorizontalAlignment(nui::Component::START);
        valueSetter->setValueBackgroundHeight(24.f);
        valueSetter->setValueBackgroundVerticalAlignment(nui::Component::CENTER);
        valueSetter->setFontSize(nui::Theme::SMALL);
    }

    getLayout().setGap(8.f);
    setLayoutMargin(kSynthSectionPadding);
    getLayout().setDisplayGrid(false);
    getLayout().init({ 1, 1, 4 }, { 1, 1, 1 });
    getLayout().setFixedRowHeight(0, 28.f);
    getLayout().setFixedRowHeight(1, 28.f);

    getLayout().addComponent(_algorithmCycler, 0, 0, 3, 1);
    getLayout().addComponent(_fmAmountSetter, 1, 0, 3, 1);
    getLayout().addComponent(_ringModMixSetter, 1, 0, 3, 1);
    getLayout().addComponent(_amDepthSetter, 1, 0, 3, 1);
    getLayout().addComponent(_serialFoldAmountSetter, 1, 0, 3, 1);
    getLayout().addComponent("sub-level-slider", _subLevelSlider, 2, 0, 1, 1);
    getLayout().addComponent("osc1-level-slider", _osc1LevelSlider, 2, 1, 1, 1);
    getLayout().addComponent("osc2-level-slider", _osc2LevelSlider, 2, 2, 1, 1);

    registerComponent(_algorithmCycler);
    registerComponent(_fmAmountSetter);
    registerComponent(_ringModMixSetter);
    registerComponent(_amDepthSetter);
    registerComponent(_serialFoldAmountSetter);

    updateAlgorithmParameterVisibility(_algorithmCycler.getSelectedIndex());

    // The Sub/Osc2 level sliders track those sections' own enabled toggle, independently of
    // whatever the Mixer's own algorithm/visibility state is doing - Osc1 has no such toggle (see
    // SynthOscillatorSection), so its slider is never touched here.
    if (auto* subEnabledParameter = parameterManager.getState().getParameter(Parameters::SUB_ENABLED_ID))
    {
        _subEnabledAttachment = std::make_unique<juce::ParameterAttachment>(*subEnabledParameter,
            [this](float value) { _subLevelSlider.setEnabled(value > 0.5f); });
        _subEnabledAttachment->sendInitialUpdate();
    }

    if (auto* osc2EnabledParameter = parameterManager.getState().getParameter(Parameters::OSC2_ENABLED_ID))
    {
        _osc2EnabledAttachment = std::make_unique<juce::ParameterAttachment>(*osc2EnabledParameter,
            [this](float value) { _osc2LevelSlider.setEnabled(value > 0.5f); });
        _osc2EnabledAttachment->sendInitialUpdate();
    }

    initSection();
}

SynthMixerSection::~SynthMixerSection()
{
    _algorithmCycler.removeListener(this);
    AppLocalisation::getChangeBroadcaster().removeChangeListener(this);
}

void SynthMixerSection::onSelectionChanged(const std::string& componentID, int selectedIndex)
{
    if (componentID != _algorithmCycler.getID())
        return;

    updateAlgorithmParameterVisibility(selectedIndex);
}

void SynthMixerSection::updateAlgorithmParameterVisibility(int selectedIndex)
{
    _fmAmountSetter.setVisible(selectedIndex == 1);
    _ringModMixSetter.setVisible(selectedIndex == 2);
    _amDepthSetter.setVisible(selectedIndex == 3);
    _serialFoldAmountSetter.setVisible(selectedIndex == 4);
}

std::string SynthMixerSection::getSectionName()
{
    return juce::translate("synth_mixer_section_title").toStdString();
}

void SynthMixerSection::refreshLabels()
{
    _fmAmountSetter.setDisplayLabel(juce::translate("synth_mixer_fm_amount_label"));
    _ringModMixSetter.setDisplayLabel(juce::translate("synth_mixer_ring_mod_mix_label"));
    _amDepthSetter.setDisplayLabel(juce::translate("synth_mixer_am_depth_label"));
    _serialFoldAmountSetter.setDisplayLabel(juce::translate("synth_mixer_serial_fold_amount_label"));

    _subLevelSlider.setName(juce::translate("synth_mixer_sub_label"));
    _osc1LevelSlider.setName(juce::translate("synth_mixer_osc1_label"));
    _osc2LevelSlider.setName(juce::translate("synth_mixer_osc2_label"));
}

}
