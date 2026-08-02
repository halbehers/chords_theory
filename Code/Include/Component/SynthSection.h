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
//
// enabledParameterID is optional (empty = no bypass toggle, matching nui::Section's own default) -
// passed straight through to Section, whose constructor already shows/wires the power-icon toggle
// button for a non-empty ID. That toggle's own live clicks correctly call Section::setBypass() via
// Section::onToggleValueChanged(), but its *initial* state (loaded/default parameter value) does
// not - Section's own bypass wiring only reacts to actual button clicks, not the ButtonAttachment's
// silent construction-time sync. A derived class using a non-empty enabledParameterID must
// therefore call setBypass() itself once, from its own constructor body, to match the parameter's
// real current value - see SynthOscillatorSection for the pattern.
class SynthSection : public nui::Section
{
public:
    SynthSection(const std::string& identifier, ndsp::ParameterManager& parameterManager, const std::string& enabledParameterID = "");
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
