#pragma once

#include <string>

#include <nierika_dsp/nierika_dsp.h>

namespace component
{

// Shared plumbing for every Synth tab section (ADSR/LFO/Filter/Oscillator): border/background
// styling, and keeping the section title and every dial's label in sync with the current locale.
// Derived classes implement getSectionName()/refreshLabels().
//
// initSection() MUST be called as the last statement of the most-derived class's own constructor
// body - not from here. A base class constructor runs before any derived class's own members
// (e.g. SynthLfoSection's _syncDivisionDial) are constructed, so a call made from SynthSection's
// constructor could never safely reach an override that touches them - only by the time the
// derived class's own constructor body is running are those members guaranteed to exist.
class SynthSection : public nui::Section
{
public:
    SynthSection(const std::string& identifier, ndsp::ParameterManager& parameterManager);
    ~SynthSection() override;

protected:
    virtual std::string getSectionName() = 0;
    virtual void refreshLabels() = 0;

    void initSection();

private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthSection)
};

}
