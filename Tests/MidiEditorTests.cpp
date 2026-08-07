#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Component/MidiEditor.h"
#include "Theory/ChordDatabase.h"
#include "Theory/NoteConvertor.h"

using component::MidiEditor;
using theory::Chord;
using theory::ChordDatabase;
using theory::Key;
using theory::NoteConvertor;
using theory::Scale;

namespace
{
    // Mirrors the fixed default-state constants declared in MidiEditor.cpp's anonymous namespace
    // (kGutterWidth, kRulerHeight, kDefaultPixelsPerBeat, kDefaultRowHeight, kInitialTopMidiNote,
    // kMaxMidiNote) - a freshly constructed, unzoomed/unscrolled editor's coordinate math is fully
    // deterministic from these, same "hardcode the widget's known fixed layout" precedent
    // ProgressionSlotView's own tests once used (its {40, 28} for an 80x56 slot's centre). Kept in
    // sync manually; if MidiEditor.cpp's own defaults ever change, update these too.
    constexpr float kGutterWidth = 40.f;
    constexpr float kRulerHeight = 24.f;
    constexpr float kPixelsPerBeat = 80.f;
    constexpr float kRowHeight = 16.f;
    constexpr int kInitialTopMidiNote = 67;
    constexpr float kScrollbarThickness = 8.f;
    constexpr float kChordLaneHeight = 28.f;
    constexpr double kBeatsPerBar = 4.0; // a dropped chord's default length/snap cell is a full bar

    float beatToX(double beat) { return kGutterWidth + static_cast<float>(beat) * kPixelsPerBeat; }
    float pitchToY(int midiNote) { return kRulerHeight + static_cast<float>(kInitialTopMidiNote - midiNote) * kRowHeight; }

    juce::MouseEvent makeMouseEvent(juce::Component& component, juce::Point<float> position)
    {
        return juce::MouseEvent(
            juce::Desktop::getInstance().getMainMouseSource(),
            position,
            juce::ModifierKeys(),
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            &component, &component,
            juce::Time::getCurrentTime(),
            position,
            juce::Time::getCurrentTime(),
            1,
            false);
    }

    struct RecordingListener : public MidiEditor::Listener
    {
        int droppedCount = 0;
        double lastDroppedBeat = -1.0;

        void onChordFileDropped(double startBeat, const juce::String&) override
        {
            ++droppedCount;
            lastDroppedBeat = startBeat;
        }
    };

    const Chord& getTestChord()
    {
        return ChordDatabase::getInstance().get(Key::C, Scale::Major).degrees.front().chords.front();
    }
}

TEST_CASE("MidiEditor::addChordAtBeat splits the chord into one note block per chord tone", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    const auto& chord = getTestChord();
    const auto expectedNoteCount = static_cast<int>(NoteConvertor::voiceChordCloseToMiddleC(chord).size());

    editor.addChordAtBeat(0.0, chord);

    CHECK(editor.getNoteCount() == expectedNoteCount);
    CHECK(editor.getChordBlockCount() == 1);
    REQUIRE(editor.getChordBlockStartBeat(0).has_value());
    CHECK(*editor.getChordBlockStartBeat(0) == Catch::Approx(0.0));
    CHECK(*editor.getChordBlockLengthBeats(0) == Catch::Approx(kBeatsPerBar));
}

TEST_CASE("MidiEditor: double-click on empty space adds a note, double-click on a note removes it", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    const juce::Point<float> pos { 200.f, 200.f };
    editor.mouseDoubleClick(makeMouseEvent(editor, pos));
    REQUIRE(editor.getNoteCount() == 1);

    // Same point again - now lands on the note just created, so this removes it instead of adding
    // a second one (the resize-handle zone is only 7px wide, comfortably narrower than a 1-beat
    // note at the default zoom, so a click at its own creation point always lands in its body).
    editor.mouseDoubleClick(makeMouseEvent(editor, pos));
    CHECK(editor.getNoteCount() == 0);
}

TEST_CASE("MidiEditor: dragging a note moves it in both time and pitch", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    editor.addChordAtBeat(0.0, getTestChord());
    REQUIRE(editor.getNoteCount() > 0);

    const auto originalPitch = *editor.getNoteMidiPitch(0);
    // Well clear of both 7px-wide resize handle zones at the note's own default 1-beat length, so
    // this always lands in the note's body (a plain move), not either edge's resize zone.
    const juce::Point<float> start { beatToX(0.5f), pitchToY(originalPitch) + 1.f };
    const juce::Point<float> dragged { start.x + 2.f * kPixelsPerBeat, start.y - 3.f * kRowHeight }; // +2 beats, +3 semitones

    editor.mouseDown(makeMouseEvent(editor, start));
    editor.mouseDrag(makeMouseEvent(editor, dragged));
    editor.mouseUp(makeMouseEvent(editor, dragged));

    CHECK(*editor.getNoteStartBeat(0) == Catch::Approx(2.0));
    CHECK(*editor.getNoteMidiPitch(0) == originalPitch + 3);
}

TEST_CASE("MidiEditor: dragging a note's right edge resizes it without moving its start", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    editor.addChordAtBeat(0.0, getTestChord());
    REQUIRE(editor.getNoteCount() > 0);

    const auto originalPitch = *editor.getNoteMidiPitch(0);
    const auto originalStart = *editor.getNoteStartBeat(0);
    const auto originalLength = *editor.getNoteLengthBeats(0);
    const auto rightEdgeX = beatToX(originalStart + originalLength);
    const auto y = pitchToY(originalPitch) + 1.f;

    const juce::Point<float> start { rightEdgeX - 3.f, y }; // inside the resize handle's 7px zone
    const juce::Point<float> dragged { start.x + kPixelsPerBeat, y }; // +1 beat of length

    editor.mouseDown(makeMouseEvent(editor, start));
    editor.mouseDrag(makeMouseEvent(editor, dragged));
    editor.mouseUp(makeMouseEvent(editor, dragged));

    CHECK(*editor.getNoteStartBeat(0) == Catch::Approx(originalStart));
    CHECK(*editor.getNoteLengthBeats(0) == Catch::Approx(originalLength + 1.0));
}

TEST_CASE("MidiEditor: dragging a note's left edge resizes it without moving its end", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    editor.addChordAtBeat(kBeatsPerBar, getTestChord()); // starts at bar 2, leaving room to drag the start earlier
    REQUIRE(editor.getNoteCount() > 0);

    const auto originalPitch = *editor.getNoteMidiPitch(0);
    const auto originalStart = *editor.getNoteStartBeat(0);
    const auto originalEnd = originalStart + *editor.getNoteLengthBeats(0);
    const auto leftEdgeX = beatToX(originalStart);
    const auto y = pitchToY(originalPitch) + 1.f;

    const juce::Point<float> start { leftEdgeX + 3.f, y }; // inside the resize handle's 7px zone
    const juce::Point<float> dragged { start.x - 0.5f * kPixelsPerBeat, y }; // start half a beat earlier

    editor.mouseDown(makeMouseEvent(editor, start));
    editor.mouseDrag(makeMouseEvent(editor, dragged));
    editor.mouseUp(makeMouseEvent(editor, dragged));

    CHECK(*editor.getNoteStartBeat(0) == Catch::Approx(originalStart - 0.5));
    CHECK(*editor.getNoteStartBeat(0) + *editor.getNoteLengthBeats(0) == Catch::Approx(originalEnd));
}

TEST_CASE("MidiEditor: a chord block's length tracks the longest of its remaining notes", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    editor.addChordAtBeat(0.0, getTestChord());
    REQUIRE(editor.getNoteCount() > 0);
    REQUIRE(editor.getChordBlockCount() == 1);
    CHECK(*editor.getChordBlockLengthBeats(0) == Catch::Approx(kBeatsPerBar));

    // Extend note 0's right edge by one extra beat - the chord block should grow to match, even
    // though ChordBlockData::lengthBeats itself is never mutated after creation (see
    // MidiEditor::effectiveChordBlockLength).
    const auto pitch = *editor.getNoteMidiPitch(0);
    const auto start = *editor.getNoteStartBeat(0);
    const auto rightEdgeX = beatToX(start + *editor.getNoteLengthBeats(0));
    const auto y = pitchToY(pitch) + 1.f;

    const juce::Point<float> dragStart { rightEdgeX - 3.f, y };
    const juce::Point<float> dragged { dragStart.x + kPixelsPerBeat, y };

    editor.mouseDown(makeMouseEvent(editor, dragStart));
    editor.mouseDrag(makeMouseEvent(editor, dragged));
    editor.mouseUp(makeMouseEvent(editor, dragged));

    REQUIRE(*editor.getNoteLengthBeats(0) == Catch::Approx(kBeatsPerBar + 1.0));
    CHECK(*editor.getChordBlockLengthBeats(0) == Catch::Approx(kBeatsPerBar + 1.0));
}

TEST_CASE("MidiEditor: filesDropped fires onChordFileDropped with the drop's beat position", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    RecordingListener listener;
    editor.addListener(&listener);

    REQUIRE(editor.isInterestedInFileDrag({ "chord.mid" }));
    editor.filesDropped({ "chord.mid" }, static_cast<int>(beatToX(4.0)), 100);

    CHECK(listener.droppedCount == 1);
    CHECK(listener.lastDroppedBeat >= 0.0);

    editor.removeListener(&listener);
}

TEST_CASE("MidiEditor: a chord dropped on a fully-occupied beat replaces the existing block and its notes", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    const auto& chordA = getTestChord();
    const auto& chordB = ChordDatabase::getInstance().get(Key::C, Scale::Major).degrees[1].chords.front();
    const auto expectedCountB = static_cast<int>(NoteConvertor::voiceChordCloseToMiddleC(chordB).size());

    editor.addChordAtBeat(0.0, chordA);
    editor.addChordAtBeat(0.0, chordB); // same whole-bar cell, chordA fully occupies it

    CHECK(editor.getChordBlockCount() == 1);
    CHECK(editor.getNoteCount() == expectedCountB);
}

TEST_CASE("MidiEditor: a chord dropped on a partially-occupied beat takes only the free remainder", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    editor.addChordAtBeat(0.0, getTestChord());
    REQUIRE(editor.getChordBlockCount() == 1);
    REQUIRE(*editor.getChordBlockStartBeat(0) == Catch::Approx(0.0));

    // There's no chord-block resize gesture (only move) - so the way to reach a genuine partial
    // overlap is to drag the existing block half a bar (2 beats) to the right, so it now straddles
    // bar cells [0,4) and [4,8), leaving the first half of [0,4) free for the next drop to claim.
    const auto chordLaneY = 400.f - kScrollbarThickness - kChordLaneHeight + 4.f;
    const juce::Point<float> start { beatToX(0.0) + 1.f, chordLaneY };
    const juce::Point<float> dragged { start.x + kPixelsPerBeat * 2.f, chordLaneY };
    editor.mouseDown(makeMouseEvent(editor, start));
    editor.mouseDrag(makeMouseEvent(editor, dragged));
    editor.mouseUp(makeMouseEvent(editor, dragged));

    REQUIRE(*editor.getChordBlockStartBeat(0) == Catch::Approx(2.0));

    editor.addChordAtBeat(0.0, getTestChord()); // targets cell [0,4) - [2,4) of it is now occupied

    REQUIRE(editor.getChordBlockCount() == 2);
    CHECK(*editor.getChordBlockStartBeat(1) == Catch::Approx(0.0));
    CHECK(*editor.getChordBlockLengthBeats(1) == Catch::Approx(2.0));
}
