#include "PluginProcessor.h"
#include "PluginEditor.h"

PluginAudioProcessorEditor::PluginAudioProcessorEditor(PluginAudioProcessor& p):
    AudioProcessorEditor(&p),
    _layout(p, p)
{
    addAndMakeVisible(_layout, 10);

    setResizable(true, false); // false: no custom corner-grip overlay, the OS window frame already provides edge/corner resizing
    setResizeLimits(1150, 750, 1920, 1200);
    setSize(1150, 750);
}

void PluginAudioProcessorEditor::setBypass(bool isBypassed)
{
    _layout.setBypass(isBypassed);
}

void PluginAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.setColour(nui::Theme::newColor(nui::Theme::BACKGROUND).asJuce());
    g.fillAll();
}

void PluginAudioProcessorEditor::resized()
{
    _layout.setBounds(getLocalBounds());
}
