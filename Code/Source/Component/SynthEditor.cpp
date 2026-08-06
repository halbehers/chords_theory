#include "Component/SynthEditor.h"

#include "Parameters.h"

namespace component
{

SynthEditor::SynthEditor(ndsp::ParameterManager& parameterManager,
    ndsp::SingleChannelSampleFIFO<juce::AudioBuffer<float>>* leftWaveformFifo,
    ndsp::SingleChannelSampleFIFO<juce::AudioBuffer<float>>* rightWaveformFifo):
    Component("synth-editor"),
    _subSection("synth-sub-section", parameterManager, Parameters::SUB_ENABLED_ID),
    _oscillator1Section("synth-oscillator-1-section", parameterManager, Parameters::OSC1_ID_PREFIX, "synth_oscillator_1_section_title"),
    _oscillator2Section("synth-oscillator-2-section", parameterManager, Parameters::OSC2_ID_PREFIX, "synth_oscillator_2_section_title", Parameters::OSC2_ENABLED_ID),
    _mixerSection("synth-mixer-section", parameterManager),
    _adsrSection("synth-adsr-section", parameterManager),
    _lfoSection("synth-lfo-section", parameterManager),
    _filterSection("synth-filter-section", parameterManager),
    _outputSection("synth-output-section", parameterManager, leftWaveformFifo, rightWaveformFifo)
{
    addAndMakeVisible(_subSection);
    addAndMakeVisible(_oscillator1Section);
    addAndMakeVisible(_oscillator2Section);
    addAndMakeVisible(_mixerSection);
    addAndMakeVisible(_adsrSection);
    addAndMakeVisible(_lfoSection);
    addAndMakeVisible(_filterSection);
    addAndMakeVisible(_outputSection);

    _layout.setGap(16.f);
    _layout.setMargin(0.f, 0.f, 0.f, 16.f);

    _layout.setDisplayGrid(false);
    _layout.init({ 3, 2 }, { 4, 6, 2, 2, 6, 4, 6, 6 });

    _layout.addComponent(_subSection, 0, 0, 1, 1);
    _layout.addComponent(_oscillator1Section, 0, 1, 3, 1);
    _layout.addComponent(_oscillator2Section, 0, 4, 2, 1);
    _layout.addComponent(_mixerSection, 0, 6, 1, 1);
    _layout.addComponent(_outputSection, 0, 7, 1, 1);
    _layout.addComponent(_adsrSection, 1, 0, 3, 1);
    _layout.addComponent(_lfoSection, 1, 3, 3, 1);
    _layout.addComponent(_filterSection, 1, 6, 2, 1);
}

void SynthEditor::paint(juce::Graphics& g)
{
    Component::paint(g);

    _layout.paint(g);
}

void SynthEditor::resized()
{
    Component::resized();

    _layout.resized();
}

}
