#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <nierika_dsp/nierika_dsp.h>

#include "Audio/ProgressionPlayer.h"
#include "Component/ProgressionEditor.h"
#include "Component/ProgressionTimeline.h"
#include "Theory/ChordDatabase.h"

using audio::ProgressionPlayer;
using component::ProgressionEditor;
using component::ProgressionTimeline;
using theory::Chord;
using theory::ChordDatabase;
using theory::Key;
using theory::ProgressionSlot;
using theory::Scale;

namespace
{
    // Same pattern as ProgressionEditorTests.cpp/VoicingSelectorTests.cpp's own local helper -
    // triggerClick() posts an async command message, so the dispatch loop needs a brief pump
    // afterward for the real onClick handler to actually run.
    void triggerButtonClick(juce::Component& buttonWrapper)
    {
        auto* button = dynamic_cast<juce::Button*>(buttonWrapper.getChildComponent(0));
        REQUIRE(button != nullptr);
        button->triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
    }

    // Same pattern as ProgressionDragHandleTests.cpp's own local helper - a separate mouseDownPos
    // (rather than reusing position for both, like MidiEditorTests.cpp's helper does) is what makes
    // event.getDistanceFromDragStart() return a real, nonzero value, since ProgressionTimeline's own
    // drag-threshold check (mirroring ProgressionDragHandle's) reads that rather than tracking its
    // own start-position member.
    juce::MouseEvent makeMouseEvent(juce::Component& component, juce::Point<float> position, juce::Point<float> mouseDownPos)
    {
        return juce::MouseEvent(
            juce::Desktop::getInstance().getMainMouseSource(),
            position,
            juce::ModifierKeys(),
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            &component, &component,
            juce::Time::getCurrentTime(),
            mouseDownPos,
            juce::Time::getCurrentTime(),
            1,
            position != mouseDownPos);
    }

    struct RecordingListener : public ProgressionTimeline::Listener
    {
        int dragStartedCount = 0;
        int lastChordBlockIndex = -1;

        void onChordBlockDragStarted(int chordBlockIndex) override
        {
            ++dragStartedCount;
            lastChordBlockIndex = chordBlockIndex;
        }
    };
}

TEST_CASE("ProgressionTimeline::setHeightType centers the strip vertically via margin", "[ProgressionTimeline]")
{
    ProgressionEditor editor("test-editor", [](const ProgressionSlot&) -> const Chord* { return nullptr; });
    editor.setBounds(0, 0, 800, 400);

    ProgressionTimeline timeline("test-timeline", editor);
    timeline.setBounds(0, 0, 200, 60);

    CHECK(timeline.getHeightType() == nui::Theme::HeightType::AUTO);
    CHECK(timeline.getMargin().top == Catch::Approx(0.0f));
    CHECK(timeline.getMargin().bottom == Catch::Approx(0.0f));

    timeline.setHeightType(nui::Theme::HeightType::THIN);
    CHECK(timeline.getHeightType() == nui::Theme::HeightType::THIN);

    const auto resolvedHeight = nui::Theme::resolveHeight(nui::Theme::HeightType::THIN, static_cast<float>(timeline.getHeight()));
    const auto expectedMargin = juce::jmax(0.f, (static_cast<float>(timeline.getHeight()) - resolvedHeight) / 2.f);
    CHECK(timeline.getMargin().top == Catch::Approx(expectedMargin));
    CHECK(timeline.getMargin().bottom == Catch::Approx(expectedMargin));
    CHECK(timeline.getMargin().left == Catch::Approx(0.0f));
    CHECK(timeline.getMargin().right == Catch::Approx(0.0f));
}

TEST_CASE("ProgressionTimeline: constructing and painting without a ProgressionPlayer does not crash", "[ProgressionTimeline]")
{
    ProgressionEditor editor("test-editor", [](const ProgressionSlot&) -> const Chord* { return nullptr; });
    editor.setBounds(0, 0, 800, 400);

    // progressionPlayer deliberately omitted - exercises the "never played" nullptr code path.
    ProgressionTimeline timeline("test-timeline", editor);
    timeline.setBounds(0, 0, 200, 60);

    juce::Image image(juce::Image::ARGB, 200, 60, true);
    juce::Graphics g(image);
    REQUIRE_NOTHROW(timeline.paint(g));
}

TEST_CASE("ProgressionTimeline: paints without crashing on an empty progression", "[ProgressionTimeline]")
{
    ProgressionPlayer player;
    ProgressionEditor editor("test-editor", [](const ProgressionSlot&) -> const Chord* { return nullptr; }, &player);
    editor.setBounds(0, 0, 800, 400);

    ProgressionTimeline timeline("test-timeline", editor, &player);
    timeline.setBounds(0, 0, 200, 60);

    juce::Image image(juce::Image::ARGB, 200, 60, true);
    juce::Graphics g(image);
    REQUIRE_NOTHROW(timeline.paint(g));
}

TEST_CASE("ProgressionTimeline: dragging past the threshold on a chord segment fires onChordBlockDragStarted with its index", "[ProgressionTimeline]")
{
    ProgressionEditor editor("test-editor", [](const ProgressionSlot&) -> const Chord* { return nullptr; });
    editor.setBounds(0, 0, 800, 400);

    const Chord& chord = ChordDatabase::getInstance().get(Key::C, Scale::Major).degrees.front().chords.front();
    editor.addChordAtBeat(0.0, chord); // occupies beats [0, 4) - the first bar of the first measure

    ProgressionTimeline timeline("test-timeline", editor);
    // One measure (16 beats) across 400px -> 25px/beat, so this one chord's segment spans x in [0, 100].
    timeline.setBounds(0, 0, 400, 60);

    RecordingListener listener;
    timeline.addListener(&listener);

    const juce::Point<float> startPos { 50.0f, 30.0f };   // inside the chord's segment
    const juce::Point<float> draggedPos { 80.0f, 30.0f }; // 30px away, past the 6px threshold, still inside it

    timeline.mouseDown(makeMouseEvent(timeline, startPos, startPos));
    timeline.mouseDrag(makeMouseEvent(timeline, draggedPos, startPos));

    CHECK(listener.dragStartedCount == 1);
    CHECK(listener.lastChordBlockIndex == 0);

    timeline.removeListener(&listener);
}

TEST_CASE("ProgressionTimeline: dragging over empty space does not fire onChordBlockDragStarted", "[ProgressionTimeline]")
{
    ProgressionEditor editor("test-editor", [](const ProgressionSlot&) -> const Chord* { return nullptr; });
    editor.setBounds(0, 0, 800, 400);

    const Chord& chord = ChordDatabase::getInstance().get(Key::C, Scale::Major).degrees.front().chords.front();
    editor.addChordAtBeat(0.0, chord); // occupies beats [0, 4) only - well inside the first quarter of the measure

    ProgressionTimeline timeline("test-timeline", editor);
    timeline.setBounds(0, 0, 400, 60);

    RecordingListener listener;
    timeline.addListener(&listener);

    const juce::Point<float> startPos { 300.0f, 30.0f }; // well past the chord's own segment (x <= 100)
    const juce::Point<float> draggedPos { 340.0f, 30.0f };

    timeline.mouseDown(makeMouseEvent(timeline, startPos, startPos));
    timeline.mouseDrag(makeMouseEvent(timeline, draggedPos, startPos));

    CHECK(listener.dragStartedCount == 0);

    timeline.removeListener(&listener);
}

TEST_CASE("ProgressionTimeline: survives repeated play/stop cycles driven by ProgressionEditor's own button", "[ProgressionTimeline]")
{
    ProgressionPlayer player;
    const Chord& chord = ChordDatabase::getInstance().get(Key::C, Scale::Major).degrees.front().chords.front();

    ProgressionEditor editor("test-editor", [](const ProgressionSlot&) -> const Chord* { return nullptr; }, &player);
    editor.setBounds(0, 0, 800, 400);
    editor.addChordAtBeat(0.0, chord);

    ProgressionTimeline timeline("test-timeline", editor, &player);
    timeline.setBounds(0, 0, 200, 60);

    auto* playButton = dynamic_cast<nelement::SVGButton*>(editor.findChildWithID("progression-play-button"));
    REQUIRE(playButton != nullptr);

    for (int i = 0; i < 3; ++i)
    {
        triggerButtonClick(*playButton); // play
        REQUIRE(player.isPlaying());

        juce::Image image(juce::Image::ARGB, 200, 60, true);
        juce::Graphics g(image);
        REQUIRE_NOTHROW(timeline.paint(g));

        triggerButtonClick(*playButton); // stop
        REQUIRE_FALSE(player.isPlaying());
    }
}
