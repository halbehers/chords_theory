#include "Component/SynthOutputSection.h"

#include "AppLocalisation.h"
#include "Component/SynthLayoutConstants.h"
#include "Parameters.h"

namespace component
{

SynthOutputSection::SynthOutputSection(const std::string& identifier, ndsp::ParameterManager& parameterManager,
    ndsp::SingleChannelSampleFIFO<juce::AudioBuffer<float>>* leftWaveformFifo,
    ndsp::SingleChannelSampleFIFO<juce::AudioBuffer<float>>* rightWaveformFifo):
    SynthSection(identifier, parameterManager),
    _waveform(identifier + "-waveform", leftWaveformFifo, rightWaveformFifo),
    _tuningDial(parameterManager, Parameters::TUNING_REFERENCE_HZ_ID, "Hz"),
    _compressorAmountDial(parameterManager, Parameters::COMPRESSOR_AMOUNT_PERCENT_ID),
    _panDial(parameterManager, Parameters::PAN_PERCENT_ID, "%"),
    _outputDial(parameterManager, Parameters::OUTPUT_DB_ID, "dB")
{
    addAndMakeVisible(_waveform);
    addAndMakeVisible(_tuningDial);
    addAndMakeVisible(_compressorAmountDial);
    addAndMakeVisible(_panDial);
    addAndMakeVisible(_outputDial);

    // No outer rectangular displayBackground here deliberately - CircularStereoWaveform paints its
    // own circular disc (FULL_SHADE fill + BORDER outline), which floats directly on this
    // section's own panel rather than sitting inside a second, redundant rectangular card.
    _waveform.setTraceColour(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _waveform.setBackgroundColour(nui::Theme::ThemeColor::BACKGROUND);
    _waveform.setBorderColour(juce::Colours::transparentBlack);

    _tuningDial.setHeightType(nui::Theme::DialHeightType::MEDIUM);
    _compressorAmountDial.setHeightType(nui::Theme::DialHeightType::MEDIUM);
    _panDial.setHeightType(nui::Theme::DialHeightType::MEDIUM);
    _outputDial.setHeightType(nui::Theme::DialHeightType::MEDIUM);

    _tuningDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _compressorAmountDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _panDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);
    _outputDial.setAccentColor(nui::Theme::ThemeColor::SECONDARY_ACCENT);

    _tuningDial.setLabelGap(8.f);
    _compressorAmountDial.setLabelGap(8.f);
    _panDial.setLabelGap(8.f);
    _outputDial.setLabelGap(8.f);

    setGap(8.f);
    setLayoutMargin(kSynthSectionPadding);
    getLayout().setDisplayGrid(false);
    getLayout().init({ 3, 1, 1 }, { 1, 1 });
    getLayout().setFixedRowHeight(1, 79.f);
    getLayout().setFixedRowHeight(2, 79.f);

    getLayout().addComponent(_waveform, 0, 0, 2, 1);
    getLayout().addComponent(_tuningDial, 1, 0, 1, 1);
    getLayout().addComponent(_compressorAmountDial, 1, 1, 1, 1);
    getLayout().addComponent(_panDial, 2, 0, 1, 1);
    getLayout().addComponent(_outputDial, 2, 1, 1, 1);

    initSection();
}

SynthOutputSection::~SynthOutputSection()
{
    AppLocalisation::getChangeBroadcaster().removeChangeListener(this);
}

std::string SynthOutputSection::getSectionName()
{
    return juce::translate("synth_output_section_title").toStdString();
}

void SynthOutputSection::refreshLabels()
{
    _tuningDial.setDisplayLabel(juce::translate("synth_output_tuning_label"));
    _compressorAmountDial.setDisplayLabel(juce::translate("synth_output_compressor_label"));
    _panDial.setDisplayLabel(juce::translate("synth_output_pan_label"));
    _outputDial.setDisplayLabel(juce::translate("synth_output_output_label"));
}

}
