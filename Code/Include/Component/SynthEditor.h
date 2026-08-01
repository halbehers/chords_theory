#pragma once

#include <string>

#include <nierika_dsp/nierika_dsp.h>

#include "Component/SynthAdsrSection.h"
#include "Component/SynthFilterSection.h"
#include "Component/SynthLfoSection.h"

namespace component
{

// The Synth tab's content: a grid of sound-design modules (currently ADSR | LFO | Filter, side by
// side). Plain nui::Component with its own manual GridLayout (same convention as
// ProgressionSequencer/AppLayout), not Section::initLayout's auto-grid, for full control over
// per-module sizing. Adding a future module is a one-line addComponent() call plus bumping the
// column (or row) count in init() below - the same pattern every other grid in this codebase uses.
class SynthEditor : public nui::Component
{
public:
    explicit SynthEditor(ndsp::ParameterManager& parameterManager);
    ~SynthEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    SynthAdsrSection _adsrSection;
    SynthLfoSection _lfoSection;
    SynthFilterSection _filterSection;

    nlayout::GridLayout<nui::Component> _layout { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthEditor)
};

}
