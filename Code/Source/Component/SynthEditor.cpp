#include "Component/SynthEditor.h"

namespace component
{

SynthEditor::SynthEditor(ndsp::ParameterManager& parameterManager):
    Component("synth-editor"),
    _adsrSection("synth-adsr-section", parameterManager),
    _lfoSection("synth-lfo-section", parameterManager),
    _filterSection("synth-filter-section", parameterManager)
{
    addAndMakeVisible(_adsrSection);
    addAndMakeVisible(_lfoSection);
    addAndMakeVisible(_filterSection);

    _layout.setGap(16.f);
    _layout.setDisplayGrid(false);
    _layout.init({ 1, 1 }, { 1, 1, 1 });

    _layout.setFixedRowHeight(0, 250.f);
    _layout.setFixedRowHeight(1, 250.f);

    _layout.addComponent(_adsrSection, 1, 0, 1, 1);
    _layout.addComponent(_lfoSection, 1, 1, 1, 1);
    _layout.addComponent(_filterSection, 1, 2, 1, 1);
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
