/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <nierika_dsp/nierika_dsp.h>

#include <string>

#include "Audio/ChordSynthEngine.h"

using BlockType = juce::AudioBuffer<float>;

//==============================================================================
/**
*/
class PluginAudioProcessor : public juce::AudioProcessor, public ndsp::ParameterManager
{
public:
    //==============================================================================
    PluginAudioProcessor();
    ~PluginAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (BlockType&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    audio::ChordSynthEngine& getSynthEngine() { return _synthEngine; }

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Runtime checks, not preprocessor ones: JUCE compiles this file once as shared code across
    // every format in FORMATS (CMakeLists.txt), so e.g. JucePlugin_Build_VST3 is true for all of
    // them whenever VST3 is among the built formats, regardless of which one actually loads it.
    [[nodiscard]] bool isVst3() const { return wrapperType == juce::AudioProcessor::wrapperType_VST3; }
    [[nodiscard]] bool isAudioUnit() const { return wrapperType == juce::AudioProcessor::wrapperType_AudioUnit; }
    [[nodiscard]] bool isStandalone() const { return wrapperType == juce::AudioProcessor::wrapperType_Standalone; }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout getParameterLayout();

    audio::ChordSynthEngine _synthEngine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginAudioProcessor)
};
