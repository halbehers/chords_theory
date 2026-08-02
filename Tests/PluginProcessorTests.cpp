#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Parameters.h"
#include "PluginProcessor.h"

#include <set>
#include <string>
#include <vector>

TEST_CASE("PluginAudioProcessor constructs successfully, registering all parameter sections", "[PluginProcessor]")
{
    // PluginAudioProcessor::getParameterLayout() calls Parameters::registerAllSections as part
    // of construction (see PluginProcessor.cpp) - this exercises that path without risking a
    // double-registration by calling registerAllSections a second time on an already-built processor.
    REQUIRE_NOTHROW(PluginAudioProcessor());
}

TEST_CASE("Parameters ID constants are all non-empty and mutually distinct", "[PluginProcessor]")
{
    // A copy-paste ID collision here would silently corrupt APVTS parameter registration - this
    // is a correctness net that stays useful as more parameters get added on top of the template.
    // Oscillator IDs are prefix+suffix (osc1-/osc2- x 14 suffixes, see registerOscillatorParameters)
    // rather than standalone ID constants like every other section - built here the same way
    // registration itself builds them. Output/Sub Level/Sub Octave stay in this list even though
    // no dial exposes them right now (see SynthOscillatorSection) - the underlying parameters are
    // still registered and still need uniqueness checking.
    const std::vector<std::string> oscillatorSuffixes {
        Parameters::OSC_SHAPE_SUFFIX,
        Parameters::OSC_SHAPE_NOISE_PERCENT_SUFFIX,
        Parameters::OSC_OCTAVE_SUFFIX,
        Parameters::OSC_TRANSPOSE_SEMITONES_SUFFIX,
        Parameters::OSC_DETUNE_CENTS_SUFFIX,
        Parameters::OSC_WARP_PERCENT_SUFFIX,
        Parameters::OSC_FOLD_PERCENT_SUFFIX,
        Parameters::OSC_OUTPUT_DB_SUFFIX,
        Parameters::OSC_UNISON_VOICES_SUFFIX,
        Parameters::OSC_UNISON_DETUNE_CENTS_SUFFIX,
        Parameters::OSC_UNISON_STEREO_PERCENT_SUFFIX,
        Parameters::OSC_SUB_LEVEL_PERCENT_SUFFIX,
        Parameters::OSC_SUB_OCTAVE_DOWN2_ENABLED_SUFFIX,
        Parameters::OSC_PHASE_PERCENT_SUFFIX,
        Parameters::OSC_PHASE_RANDOMIZE_ENABLED_SUFFIX,
    };

    std::vector<std::string> ids {
        Parameters::PLUGIN_ENABLED_ID,
        Parameters::OSC2_ENABLED_ID,
        Parameters::ENVELOPE_DELAY_MS_ID,
        Parameters::ENVELOPE_ATTACK_MS_ID,
        Parameters::ENVELOPE_HOLD_MS_ID,
        Parameters::ENVELOPE_DECAY_MS_ID,
        Parameters::ENVELOPE_SUSTAIN_PERCENT_ID,
        Parameters::ENVELOPE_RELEASE_MS_ID,
        Parameters::FILTER_TYPE_ID,
        Parameters::FILTER_SLOPE_ID,
        Parameters::FILTER_CUTOFF_HZ_ID,
        Parameters::FILTER_RESONANCE_ID,
        Parameters::FILTER_DRIVE_DB_ID,
        Parameters::FILTER_MIX_PERCENT_ID,
        Parameters::FILTER_KEY_TRACK_PERCENT_ID,
        Parameters::LFO_SHAPE_ID,
        Parameters::LFO_RATE_HZ_ID,
        Parameters::LFO_SYNC_ENABLED_ID,
        Parameters::LFO_SYNC_DIVISION_ID,
        Parameters::LFO_MODE_ID,
        Parameters::LFO_SMOOTH_PERCENT_ID,
        Parameters::LFO_DELAY_MS_ID,
        Parameters::LFO_STEREO_PERCENT_ID,
        Parameters::LFO_DEPTH_PERCENT_ID,
    };

    for (const auto& suffix : oscillatorSuffixes)
    {
        ids.push_back(Parameters::OSC1_ID_PREFIX + suffix);
        ids.push_back(Parameters::OSC2_ID_PREFIX + suffix);
    }

    for (const auto& id : ids)
        CHECK(! id.empty());

    const std::set<std::string> uniqueIds(ids.begin(), ids.end());
    CHECK(uniqueIds.size() == ids.size());
}

TEST_CASE("PluginAudioProcessor round-trips parameter state via getStateInformation/setStateInformation", "[PluginProcessor]")
{
    PluginAudioProcessor source;

    auto* enabledParameter = source.getState().getParameter(Parameters::PLUGIN_ENABLED_ID);
    REQUIRE(enabledParameter != nullptr);
    enabledParameter->setValueNotifyingHost(0.0f); // flips PLUGIN_ENABLED_ID away from its default (true)

    auto* releaseParameter = dynamic_cast<juce::RangedAudioParameter*>(source.getState().getParameter(Parameters::ENVELOPE_RELEASE_MS_ID));
    REQUIRE(releaseParameter != nullptr);
    releaseParameter->setValueNotifyingHost(releaseParameter->convertTo0to1(999.0f)); // flips away from its default (200)

    // AudioProcessorValueTreeState only flushes parameter values into its internal state
    // ValueTree periodically (an internal 10Hz timer), not synchronously on
    // setValueNotifyingHost() - pump the message loop briefly so getStateInformation() below
    // serializes the up-to-date value rather than a stale, not-yet-flushed one.
    juce::MessageManager::getInstance()->runDispatchLoopUntil(150);

    juce::MemoryBlock state;
    source.getStateInformation(state);
    REQUIRE(state.getSize() > 0);

    // A fresh instance, exactly matching what happens when the editor is recreated / the host
    // reloads a project.
    PluginAudioProcessor restored;
    restored.setStateInformation(state.getData(), (int) state.getSize());

    const auto* restoredValue = restored.getState().getRawParameterValue(Parameters::PLUGIN_ENABLED_ID);
    REQUIRE(restoredValue != nullptr);
    CHECK(restoredValue->load() == Catch::Approx(0.0f));

    const auto* restoredReleaseValue = restored.getState().getRawParameterValue(Parameters::ENVELOPE_RELEASE_MS_ID);
    REQUIRE(restoredReleaseValue != nullptr);
    CHECK(restoredReleaseValue->load() == Catch::Approx(999.0f));
}

TEST_CASE("PluginAudioProcessor: setting the envelope-attack-ms parameter immediately updates the synth engine's shared state", "[PluginProcessor]")
{
    // Unlike the ValueTree-serialization round-trip above (debounced by an internal 10Hz timer),
    // the onChange callback wired in Parameters::registerEnvelopeParameters fires synchronously
    // on setValueNotifyingHost() - see VoiceSharedState's doc comment for why - so no message-loop
    // pump is needed here.
    PluginAudioProcessor processor;

    auto* attackParameter = dynamic_cast<juce::RangedAudioParameter*>(processor.getState().getParameter(Parameters::ENVELOPE_ATTACK_MS_ID));
    REQUIRE(attackParameter != nullptr);

    attackParameter->setValueNotifyingHost(attackParameter->convertTo0to1(1234.0f));

    CHECK(processor.getSynthEngine().getSharedState().envelopeAttackMs.load() == Catch::Approx(1234.0f));
}
