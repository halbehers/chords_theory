#include "Component/SynthEditor.h"

#include "Parameters.h"

namespace component
{

SynthEditor::SynthEditor(ndsp::ParameterManager& parameterManager):
    Component("synth-editor"),
    _oscillator1Section("synth-oscillator-1-section", parameterManager, Parameters::OSC1_ID_PREFIX, "synth_oscillator_1_section_title"),
    _oscillator2Section("synth-oscillator-2-section", parameterManager, Parameters::OSC2_ID_PREFIX, "synth_oscillator_2_section_title", Parameters::OSC2_ENABLED_ID),
    _adsrSection("synth-adsr-section", parameterManager),
    _lfoSection("synth-lfo-section", parameterManager),
    _filterSection("synth-filter-section", parameterManager)
{
    addAndMakeVisible(_oscillator1Section);
    addAndMakeVisible(_oscillator2Section);
    addAndMakeVisible(_adsrSection);
    addAndMakeVisible(_lfoSection);
    addAndMakeVisible(_filterSection);

    _layout.setGap(16.f);
    _layout.setMargin(0.f, 0.f, 0.f, 16.f);

    _layout.setDisplayGrid(false);
    _layout.init({ 3, 2 }, { 3, 5, 3, 5, 3, 5 });

    _layout.addComponent(_oscillator1Section, 0, 1, 2, 1);
    _layout.addComponent(_oscillator2Section, 0, 3, 2, 1);
    _layout.addComponent(_adsrSection, 1, 0, 2, 1);
    _layout.addComponent(_lfoSection, 1, 2, 2, 1);
    _layout.addComponent(_filterSection, 1, 4, 2, 1);
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
