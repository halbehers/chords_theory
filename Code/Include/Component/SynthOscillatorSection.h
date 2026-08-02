#pragma once

#include <string>

#include <nierika_dsp/nierika_dsp.h>

#include "SynthSection.h"

namespace component
{

// One "module" of the Synth tab's grid (see SynthEditor) - an oscillator: shape cycler, a
// OscillatorCurve preview (shows Shape/Warp/Noise/Fold and a start-phase marker live), Shape
// Noise/Octave/Transpose/Detune/Warp/Fold knobs, and Unison Voices/Unison Detune/Unison Stereo/
// Phase Randomize toggle/Phase. Output, Sub Level, and Sub Octave are deliberately not exposed
// here - their underlying parameters/DSP still exist (see Parameters::registerOscillatorParameters,
// audio::Oscillator), they're just not wired to a dial yet; Output is planned for a future Mixer
// section, Sub Level/Octave for a future Sub section. A single reusable class - SynthEditor owns
// two instances side by side, each bound to its own osc1-/osc2- prefixed parameter IDs via
// parameterIdPrefix, with its own section-title translation key (not string concatenation across
// locales - see SynthEditor for the two literal keys it passes in).
class SynthOscillatorSection : public SynthSection
{
public:
    SynthOscillatorSection(const std::string& identifier, ndsp::ParameterManager& parameterManager,
                            const std::string& parameterIdPrefix, const std::string& sectionTitleKey);
    ~SynthOscillatorSection() override;

protected:
    std::string getSectionName() override;
    void refreshLabels() override;

private:
    std::string _sectionTitleKey;

    nelement::Cycler _shapeCycler;
    nelement::OscillatorCurve _curve;
    nelement::PercentageDial _shapeNoiseDial;
    nelement::Dial _octaveDial;
    nelement::Dial _transposeDial;
    nelement::Dial _detuneDial;
    nelement::Dial _warpDial;
    nelement::PercentageDial _foldDial;
    nelement::Dial _unisonVoicesDial;
    nelement::Dial _unisonDetuneDial;
    nelement::PercentageDial _unisonStereoDial;
    nelement::SVGToggle _phaseRandomizeToggle;
    nelement::PercentageDial _phaseDial;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthOscillatorSection)
};

}
