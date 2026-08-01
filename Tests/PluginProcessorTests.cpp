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
    const std::vector<std::string> ids {
        Parameters::PLUGIN_ENABLED_ID,
    };

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
}
