#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <vector>

#include "Component/ChordCard.h"
#include "Theory/ChordDatabase.h"

using component::ChordCard;
using theory::Chord;
using theory::ChordDatabase;
using theory::Degree;
using theory::Key;
using theory::Scale;

namespace
{
    juce::MouseEvent makeMouseEvent(juce::Component& component, juce::Point<float> position,
        juce::Point<float> mouseDownPos, int numberOfClicks = 1)
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
            numberOfClicks,
            position != mouseDownPos);
    }

    struct RecordingListener : public ChordCard::Listener
    {
        int changedCount = 0;
        int dragStartedCount = 0;
        int previewCount = 0;
        int voicingSelectorRequestedCount = 0;
        std::optional<Degree> lastVoicingSelectorDegree;
        std::vector<Chord> lastAvailableVoicings;
        std::string lastCurrentSymbol;

        void onChordChanged(Degree, const Chord&) override { ++changedCount; }
        void onChordDragStarted(Degree, const Chord&) override { ++dragStartedCount; }
        void onChordPreviewRequested(Degree, const Chord&) override { ++previewCount; }

        void onVoicingSelectorRequested(Degree degree, const std::vector<Chord>& availableVoicings, const std::string& currentSymbol) override
        {
            ++voicingSelectorRequestedCount;
            lastVoicingSelectorDegree = degree;
            lastAvailableVoicings = availableVoicings;
            lastCurrentSymbol = currentSymbol;
        }
    };

    const Chord& getTestChord()
    {
        return ChordDatabase::getInstance().get(Key::C, Scale::Major).degrees.front().chords.front();
    }
}

TEST_CASE("ChordCard's clickable area is the card itself, not one of its decorative labels", "[ChordCard]")
{
    // Regression test: nelement::Text's internal juce::Label intercepts mouse clicks by default
    // and previously covered the card's entire clickable area (see the
    // setInterceptsMouseClicks(false, false) fix on both labels in ChordCard's constructor) -
    // without that fix, hit-testing at any point on the card resolves to a label instead of the
    // card, and no mouse event ever reaches ChordCard's own mouseDown/mouseDrag/mouseUp/
    // mouseDoubleClick.
    ChordCard card("test-card", Degree::I);
    card.setChord(getTestChord());
    card.setBounds(0, 0, 120, 60);
    card.setVisible(true); // getComponentAt() hit-tests only visible components

    CHECK(card.getComponentAt(60, 30) == &card);
}

TEST_CASE("ChordCard: a plain click (no drag) fires onChordPreviewRequested, not onChordChanged/onChordDragStarted", "[ChordCard]")
{
    ChordCard card("test-card", Degree::I);
    card.setChord(getTestChord());
    card.setBounds(0, 0, 120, 60);

    RecordingListener listener;
    card.addListener(&listener);

    const juce::Point<float> pos { 10.0f, 10.0f };
    card.mouseDown(makeMouseEvent(card, pos, pos));
    card.mouseUp(makeMouseEvent(card, pos, pos));

    CHECK(listener.previewCount == 1);
    CHECK(listener.dragStartedCount == 0);
    CHECK(listener.changedCount == 0);

    card.removeListener(&listener);
}

TEST_CASE("ChordCard: dragging past the threshold fires onChordDragStarted, not onChordPreviewRequested", "[ChordCard]")
{
    ChordCard card("test-card", Degree::I);
    card.setChord(getTestChord());
    card.setBounds(0, 0, 120, 60);

    RecordingListener listener;
    card.addListener(&listener);

    const juce::Point<float> startPos { 10.0f, 10.0f };
    const juce::Point<float> draggedPos { 40.0f, 10.0f }; // 30px away, past the 6px threshold

    card.mouseDown(makeMouseEvent(card, startPos, startPos));
    card.mouseDrag(makeMouseEvent(card, draggedPos, startPos));
    card.mouseUp(makeMouseEvent(card, draggedPos, startPos));

    CHECK(listener.dragStartedCount == 1);
    CHECK(listener.previewCount == 0);

    card.removeListener(&listener);
}

TEST_CASE("ChordCard: mouseDoubleClick with no available voicings is a safe no-op", "[ChordCard]")
{
    ChordCard card("test-card", Degree::I);
    card.setChord(getTestChord());
    card.setBounds(0, 0, 120, 60);
    // Deliberately no setAvailableVoicings() call - exercises the early-return branch.

    RecordingListener listener;
    card.addListener(&listener);

    const juce::Point<float> pos { 10.0f, 10.0f };
    REQUIRE_NOTHROW(card.mouseDoubleClick(makeMouseEvent(card, pos, pos, 2)));

    CHECK(listener.previewCount == 0);
    CHECK(listener.voicingSelectorRequestedCount == 0);

    card.removeListener(&listener);
}

TEST_CASE("ChordCard: mouseDoubleClick with available voicings fires onVoicingSelectorRequested with the card's own data", "[ChordCard]")
{
    const auto& degreeI = ChordDatabase::getInstance().get(Key::C, Scale::Major).degrees.front();
    const auto& chord = degreeI.chords.front();

    ChordCard card("test-card", Degree::I);
    card.setChord(chord);
    card.setAvailableVoicings(degreeI.chords);
    card.setBounds(0, 0, 120, 60);

    RecordingListener listener;
    card.addListener(&listener);

    const juce::Point<float> pos { 10.0f, 10.0f };
    card.mouseDoubleClick(makeMouseEvent(card, pos, pos, 2));

    CHECK(listener.voicingSelectorRequestedCount == 1);
    REQUIRE(listener.lastVoicingSelectorDegree.has_value());
    CHECK(*listener.lastVoicingSelectorDegree == Degree::I);
    CHECK(listener.lastAvailableVoicings.size() == degreeI.chords.size());
    CHECK(listener.lastCurrentSymbol == chord.symbol);

    // Selection now happens via the voicing selector, not the card - a double-click never
    // touches onChordChanged itself.
    CHECK(listener.changedCount == 0);

    card.removeListener(&listener);
}
