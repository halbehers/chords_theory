#pragma once

#include <string>

#include <nierika_dsp/nierika_dsp.h>

namespace component
{

// One "module" of the Synth tab's grid (see SynthEditor) - the LFO: shape cycler, a draggable
// LFOCurve preview, a mode toggle and tempo-sync toggle stacked in one narrow column (sync swaps
// between a free-rate dial and a sync-division dial), and Smooth/Delay/Stereo/Depth knobs - all
// APVTS-attached directly to Parameters::LFO_*.
// nui::Section already implements element::SVGToggle::OnValueChangedListener itself (for its own
// internal bypass/FX-sequencer toggles), so this just overrides that inherited virtual rather
// than re-declaring the base (which would be an ambiguous duplicate base class).
class SynthLfoSection : public nui::Section
{
public:
    SynthLfoSection(const std::string& identifier, ndsp::ParameterManager& parameterManager);
    ~SynthLfoSection() override;

private:
    void onToggleValueChanged(const std::string& componentID, bool isOn) override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void refreshDialLabels();

    nelement::Cycler _shapeCycler;
    nelement::LFOCurve _lfoCurve;
    nelement::SVGToggle _modeToggle;
    nelement::SVGToggle _syncToggle;
    nelement::TimingDial _syncDivisionDial;
    nelement::Dial _rateDial;
    nelement::PercentageDial _smoothDial;
    nelement::TimeInMsDial _delayDial;
    nelement::PercentageDial _stereoDial;
    nelement::PercentageDial _depthDial;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthLfoSection)
};

}
