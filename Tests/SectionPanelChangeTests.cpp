#include <catch2/catch_test_macros.hpp>

#include "PluginProcessor.h"

// nui::Section lives in nierika_dsp, which has no test harness of its own - these tests cover it
// from this project's suite instead, specifically the regression this project's real bug exposed:
// AppLayout::onPanelChanged (see AppLayout.cpp) depends on switchPanel() notifying listeners
// exactly once, with the correct panel ID, only when the active panel actually changes - to
// re-assert VoicingSelector's own independent open/closed state, which
// GridLayout::setVisible(true) would otherwise silently blow away every time the Chords tab
// becomes active again (it unconditionally shows every component registered in that panel).

namespace
{
    struct RecordingListener : public nui::Section::OnPanelChangedListener
    {
        int callCount = 0;
        std::string lastPanelID;

        void onPanelChanged(const std::string& newPanelID) override
        {
            ++callCount;
            lastPanelID = newPanelID;
        }
    };
}

TEST_CASE("nui::Section::switchPanel notifies OnPanelChangedListener with the new panel ID", "[Section]")
{
    PluginAudioProcessor processor;
    nui::Section section("test-section", processor);
    section.addPanel("panel-b", "Panel B");

    RecordingListener listener;
    section.addOnPanelChangedListener(&listener);

    section.switchPanel("panel-b");

    CHECK(listener.callCount == 1);
    CHECK(listener.lastPanelID == "panel-b");

    section.removeListener(&listener);
}

TEST_CASE("nui::Section::switchPanel back to a previously-active panel notifies again", "[Section]")
{
    PluginAudioProcessor processor;
    nui::Section section("test-section", processor);
    section.addPanel("panel-b", "Panel B");

    RecordingListener listener;
    section.addOnPanelChangedListener(&listener);

    section.switchPanel("panel-b");
    section.switchPanel(MAIN_PANEL_ID);

    CHECK(listener.callCount == 2);
    CHECK(listener.lastPanelID == MAIN_PANEL_ID);

    section.removeListener(&listener);
}

TEST_CASE("nui::Section::switchPanel to the already-active panel is a no-op that does not notify", "[Section]")
{
    PluginAudioProcessor processor;
    nui::Section section("test-section", processor);
    section.addPanel("panel-b", "Panel B");

    RecordingListener listener;
    section.addOnPanelChangedListener(&listener);

    section.switchPanel(MAIN_PANEL_ID); // already the active panel

    CHECK(listener.callCount == 0);

    section.removeListener(&listener);
}
