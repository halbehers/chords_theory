#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"
#include "AppSettings.h"
#include "AppLocalisation.h"

PluginAudioProcessor::PluginAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if JucePlugin_IsSynth
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #elif ! JucePlugin_IsMidiEffect
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ), ndsp::ParameterManager(dynamic_cast<juce::AudioProcessor&>(*this), [this]() { return getParameterLayout(); })
#endif
{
    nui::Theme::configure({
        .mode = AppSettings::getInstance().getThemeMode(),
        .colorOverrides = {
            {
                nui::Theme::Mode::DARK,
                {
                    { nui::Theme::TEXT, juce::Colour(0xFFFFFFFF) },
                    { nui::Theme::INVERTED_TEXT, juce::Colour(0xFFFFFFFF) },
                    { nui::Theme::DISABLED, juce::Colour(0xFFA9A9A9) },
                    { nui::Theme::PRIMARY, juce::Colour(0xFF272727) },
                    { nui::Theme::ACCENT, juce::Colour(0xFF53F4FB) },
                    { nui::Theme::SECONDARY_ACCENT, juce::Colour(0xFFE1415B) },
                    { nui::Theme::BACKGROUND, juce::Colour(0xFF0F0F0F) },
                    { nui::Theme::SECONDARY_BACKGROUND, juce::Colour(0xFF191819) },
                    { nui::Theme::BORDER, juce::Colour(0xFF303030) },
                }
            },
            {
                nui::Theme::Mode::LIGHT,
                {
                    { nui::Theme::TEXT, juce::Colour(0xFF0E0E0E) },
                    { nui::Theme::INVERTED_TEXT, juce::Colour(0xFFFFFFFF) },
                    { nui::Theme::DISABLED, juce::Colour(0xFF616161) },
                    { nui::Theme::PRIMARY, juce::Colour(0xFFCAC8C7) },
                    { nui::Theme::ACCENT, juce::Colour(0xFF39B0C1) },
                    { nui::Theme::SECONDARY_ACCENT, juce::Colour(0xFFE17C8C) },
                    { nui::Theme::BACKGROUND, juce::Colour(0xFFFFFFFF) },
                    { nui::Theme::SECONDARY_BACKGROUND, juce::Colour(0xFFF0EDEC) },
                    { nui::Theme::BORDER, juce::Colour(0xFFA5A5A5) },
                }
            }
        },
        .borderRadius = 8.f
    });

    AppLocalisation::setLanguage(AppSettings::getInstance().getLanguage());
}

PluginAudioProcessor::~PluginAudioProcessor()
{
    clearParameters();
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginAudioProcessor::getParameterLayout()
{
    Parameters::registerAllSections(this);

    return buildParameterLayout();
}

const juce::String PluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PluginAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PluginAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PluginAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int PluginAudioProcessor::getCurrentProgram()
{
    return 0;
}

void PluginAudioProcessor::setCurrentProgram(int index)
{
    (void) index;
}

const juce::String PluginAudioProcessor::getProgramName(int index)
{
    (void) index;

    return {};
}

void PluginAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    (void) index;
    (void) newName;
}

//==============================================================================
void PluginAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    _synthEngine.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
}

void PluginAudioProcessor::releaseResources()
{
    _synthEngine.reset();

    nutils::Logger::markShuttingDown();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool PluginAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void PluginAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    buffer.clear();
    _synthEngine.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples(), getPlayHead());
}

//==============================================================================
bool PluginAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PluginAudioProcessor::createEditor()
{
    return new PluginAudioProcessorEditor(*this);
}

//==============================================================================
void PluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    ndsp::ParameterManager::getStateInformation(destData);
}

void PluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    ndsp::ParameterManager::setStateInformation(data, sizeInBytes);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginAudioProcessor();
}
