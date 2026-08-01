#pragma once

#include "PluginProcessor.h"
#include "AppLayout.h"

class PluginAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    public juce::DragAndDropContainer
{
public:
    explicit PluginAudioProcessorEditor(PluginAudioProcessor&);
    ~PluginAudioProcessorEditor() override = default;

    //==============================================================================
    void paint(juce::Graphics&) override;

    void resized() override;

    void setBypass(bool isBypassed);

private:
    AppLayout _layout;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginAudioProcessorEditor)
};
