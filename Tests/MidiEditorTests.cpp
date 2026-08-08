#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Audio/ProgressionPlayer.h"
#include "Component/MidiEditor.h"
#include "Theory/ChordDatabase.h"
#include "Theory/MidiEditorState.h"
#include "Theory/NoteConvertor.h"
#include "Theory/ProgressionSlot.h"

using audio::ProgressionPlayer;
using component::MidiEditor;
using theory::Chord;
using theory::ChordDatabase;
using theory::Degree;
using theory::Key;
using theory::MidiEditorState;
using theory::NoteConvertor;
using theory::ProgressionSlot;
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
        int contentChangedCount = 0;
        int playbackStateChangedCount = 0;
        bool lastPlaybackState = false;

        void onChordFileDropped(double startBeat, const juce::String&) override
        {
            ++droppedCount;
            lastDroppedBeat = startBeat;
        }

        void onContentChanged() override { ++contentChangedCount; }

        void onPlaybackStateChanged(bool isPlaying) override
        {
            ++playbackStateChangedCount;
            lastPlaybackState = isPlaying;
        }
    };

    const Chord& getTestChord()
    {
        return ChordDatabase::getInstance().get(Key::C, Scale::Major).degrees.front().chords.front();
    }

    // getTestChord() always resolves to degree I - this just packages that fact for addChordAtBeat
    // calls that don't care about provenance beyond "a real, consistent slot".
    ProgressionSlot testSlot(const Chord& chord) { return { Degree::I, chord.popularityOrder }; }

    // Double-clicks empty space to add a single note at (beat, pitch) - kDefaultNoteLengthBeats
    // (1.0) long, same mechanism as the "double-click adds a note" test above. Callers pick
    // beat/pitch that stay within the default 800x400 test bounds' content area and don't collide
    // with an already-placed note.
    void addNoteAt(MidiEditor& editor, double beat, int pitch)
    {
        editor.mouseDoubleClick(makeMouseEvent(editor, { beatToX(beat), pitchToY(pitch) + 1.f }));
    }

    // Drags a rubber-band from an empty starting point to `end` - mouseUp resolves it into
    // _selectedNoteIndices for every note whose bounds intersect the resulting rect.
    void marqueeSelect(MidiEditor& editor, juce::Point<float> start, juce::Point<float> end)
    {
        editor.mouseDown(makeMouseEvent(editor, start));
        editor.mouseDrag(makeMouseEvent(editor, end));
        editor.mouseUp(makeMouseEvent(editor, end));
    }

    // A plain click (no movement) at pos - mouseDown immediately followed by mouseUp at the same point.
    void click(MidiEditor& editor, juce::Point<float> pos)
    {
        editor.mouseDown(makeMouseEvent(editor, pos));
        editor.mouseUp(makeMouseEvent(editor, pos));
    }
}

TEST_CASE("MidiEditor::addChordAtBeat splits the chord into one note block per chord tone", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    const auto& chord = getTestChord();
    const auto expectedNoteCount = static_cast<int>(NoteConvertor::voiceChordCloseToMiddleC(chord).size());

    editor.addChordAtBeat(0.0, chord, testSlot(chord));

    CHECK(editor.getNoteCount() == expectedNoteCount);
    CHECK(editor.getChordBlockCount() == 1);
    REQUIRE(editor.getChordBlockStartBeat(0).has_value());
    CHECK(*editor.getChordBlockStartBeat(0) == Catch::Approx(0.0));
    CHECK(*editor.getChordBlockLengthBeats(0) == Catch::Approx(kBeatsPerBar));

    REQUIRE(editor.getChordBlockSlot(0).has_value());
    CHECK(*editor.getChordBlockSlot(0) == testSlot(chord));
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

    editor.addChordAtBeat(0.0, getTestChord(), testSlot(getTestChord()));
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

    editor.addChordAtBeat(0.0, getTestChord(), testSlot(getTestChord()));
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

    editor.addChordAtBeat(kBeatsPerBar, getTestChord(), testSlot(getTestChord())); // starts at bar 2, leaving room to drag the start earlier
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

    editor.addChordAtBeat(0.0, getTestChord(), testSlot(getTestChord()));
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

    editor.addChordAtBeat(0.0, chordA, { Degree::I, chordA.popularityOrder });
    editor.addChordAtBeat(0.0, chordB, { Degree::II, chordB.popularityOrder }); // same whole-bar cell, chordA fully occupies it

    CHECK(editor.getChordBlockCount() == 1);
    CHECK(editor.getNoteCount() == expectedCountB);
}

TEST_CASE("MidiEditor: a chord dropped on a partially-occupied beat takes only the free remainder", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    editor.addChordAtBeat(0.0, getTestChord(), testSlot(getTestChord()));
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

    editor.addChordAtBeat(0.0, getTestChord(), testSlot(getTestChord())); // targets cell [0,4) - [2,4) of it is now occupied

    REQUIRE(editor.getChordBlockCount() == 2);
    CHECK(*editor.getChordBlockStartBeat(1) == Catch::Approx(0.0));
    CHECK(*editor.getChordBlockLengthBeats(1) == Catch::Approx(2.0));
}

TEST_CASE("MidiEditor::clear empties both notes and chord blocks", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    editor.addChordAtBeat(0.0, getTestChord(), testSlot(getTestChord()));
    REQUIRE(editor.getNoteCount() > 0);
    REQUIRE(editor.getChordBlockCount() > 0);

    editor.clear();

    CHECK(editor.getNoteCount() == 0);
    CHECK(editor.getChordBlockCount() == 0);
}

TEST_CASE("MidiEditor::getState/restoreState round-trips notes, chord blocks, and their sourceSlot exactly", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    const auto& chord = getTestChord();
    editor.addChordAtBeat(0.0, chord, testSlot(chord));
    editor.addChordAtBeat(kBeatsPerBar, chord, { Degree::V, chord.popularityOrder });

    const auto state = editor.getState();
    REQUIRE(state.notes.size() == static_cast<std::size_t>(editor.getNoteCount()));
    REQUIRE(state.chordBlocks.size() == 2);

    MidiEditor restored("test-midi-editor-restored");
    restored.setBounds(0, 0, 800, 400);
    restored.restoreState(state);

    CHECK(restored.getNoteCount() == editor.getNoteCount());
    CHECK(restored.getChordBlockCount() == editor.getChordBlockCount());
    CHECK(restored.getState() == state);

    REQUIRE(restored.getChordBlockSlot(0).has_value());
    CHECK(*restored.getChordBlockSlot(0) == testSlot(chord));
    REQUIRE(restored.getChordBlockSlot(1).has_value());
    CHECK(*restored.getChordBlockSlot(1) == ProgressionSlot { Degree::V, chord.popularityOrder });
}

TEST_CASE("MidiEditor::onContentChanged fires on add/move/resize/delete but not on a plain click", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    RecordingListener listener;
    editor.addListener(&listener);

    editor.addChordAtBeat(0.0, getTestChord(), testSlot(getTestChord()));
    CHECK(listener.contentChangedCount == 1);

    // A completed drag (move the note) fires once more.
    const auto pitch = *editor.getNoteMidiPitch(0);
    const juce::Point<float> start { beatToX(0.5), pitchToY(pitch) + 1.f };
    const juce::Point<float> dragged { start.x + kPixelsPerBeat, start.y };
    editor.mouseDown(makeMouseEvent(editor, start));
    editor.mouseDrag(makeMouseEvent(editor, dragged));
    editor.mouseUp(makeMouseEvent(editor, dragged));
    CHECK(listener.contentChangedCount == 2);

    // A plain click (mouseDown immediately followed by mouseUp at the same position, no drag) on
    // empty space does not count as a mutation.
    const juce::Point<float> emptySpace { 700.f, 300.f };
    editor.mouseDown(makeMouseEvent(editor, emptySpace));
    editor.mouseUp(makeMouseEvent(editor, emptySpace));
    CHECK(listener.contentChangedCount == 2);

    // Double-click delete fires again.
    editor.mouseDoubleClick(makeMouseEvent(editor, dragged));
    CHECK(listener.contentChangedCount == 3);

    editor.removeListener(&listener);
}

TEST_CASE("MidiEditor: loop bounds auto-compute to the first/last note's bar boundaries", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    editor.addChordAtBeat(0.0, getTestChord(), testSlot(getTestChord())); // bar 0: [0,4)

    // A manually-added note past that, at beat 6 (bar 1's territory: [4,8)), 1 beat long by
    // default - the loop must stretch to cover it too. (Kept within the 800px-wide test bounds'
    // visible content area - mouseDoubleClick silently no-ops outside _contentArea.)
    const juce::Point<float> pos { beatToX(6.0), pitchToY(60) + 1.f };
    editor.mouseDoubleClick(makeMouseEvent(editor, pos));

    CHECK(editor.getLoopStartBeat() == Catch::Approx(0.0));
    CHECK(editor.getLoopEndBeat() == Catch::Approx(8.0)); // ceil(7/4)*4
}

TEST_CASE("MidiEditor: a manually-resized loop no longer moves when content changes", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    editor.addChordAtBeat(0.0, getTestChord(), testSlot(getTestChord())); // loop auto-tracks to [0,4)
    REQUIRE(editor.getLoopEndBeat() == Catch::Approx(4.0));

    // Drag the loop's end handle (in the ruler band, y within [0,kRulerHeight]) one beat later.
    const auto handleX = beatToX(4.0);
    const juce::Point<float> start { handleX, 10.f };
    const juce::Point<float> dragged { handleX + kPixelsPerBeat, 10.f };
    editor.mouseDown(makeMouseEvent(editor, start));
    editor.mouseDrag(makeMouseEvent(editor, dragged));
    editor.mouseUp(makeMouseEvent(editor, dragged));

    REQUIRE(editor.getLoopEndBeat() == Catch::Approx(5.0));

    // Adding a chord far enough out that auto-tracking (if it were still active) would expand the
    // loop to bar 4 ([12,16)) - the manual resize must have opted out of that.
    editor.addChordAtBeat(3.0 * kBeatsPerBar, getTestChord(), testSlot(getTestChord()));

    CHECK(editor.getLoopStartBeat() == Catch::Approx(0.0));
    CHECK(editor.getLoopEndBeat() == Catch::Approx(5.0));
}

TEST_CASE("MidiEditor::startPlayback on an empty editor is a no-op", "[MidiEditor]")
{
    ProgressionPlayer player;
    MidiEditor editor("test-midi-editor", &player);
    editor.setBounds(0, 0, 800, 400);

    REQUIRE(editor.getNoteCount() == 0);

    editor.startPlayback();

    CHECK_FALSE(editor.isPlaying());
    CHECK_FALSE(player.isPlaying());
}

TEST_CASE("MidiEditor::onPlaybackStateChanged fires on start and stop", "[MidiEditor]")
{
    ProgressionPlayer player;
    MidiEditor editor("test-midi-editor", &player);
    editor.setBounds(0, 0, 800, 400);

    editor.addChordAtBeat(0.0, getTestChord(), testSlot(getTestChord()));

    RecordingListener listener;
    editor.addListener(&listener);

    editor.startPlayback();
    CHECK(editor.isPlaying());
    CHECK(listener.playbackStateChangedCount == 1);
    CHECK(listener.lastPlaybackState == true);

    editor.stopPlayback();
    CHECK_FALSE(editor.isPlaying());
    CHECK(listener.playbackStateChangedCount == 2);
    CHECK(listener.lastPlaybackState == false);

    editor.removeListener(&listener);
}

TEST_CASE("MidiEditor: double-clicking the ruler zooms/scrolls so the loop exactly fills the visible width, without creating a note", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    editor.addChordAtBeat(0.0, getTestChord(), testSlot(getTestChord())); // loop auto-tracks to [0,4)
    REQUIRE(editor.getLoopStartBeat() == Catch::Approx(0.0));
    REQUIRE(editor.getLoopEndBeat() == Catch::Approx(4.0));

    const auto noteCountBefore = editor.getNoteCount();

    // Double-clicking the ruler (y within [0,kRulerHeight]) must not fall through to the
    // empty-space "add a note" behavior.
    const juce::Point<float> rulerPos { 300.f, 10.f };
    editor.mouseDoubleClick(makeMouseEvent(editor, rulerPos));
    CHECK(editor.getNoteCount() == noteCountBefore);

    // The content area is 800 - kGutterWidth - kScrollbarThickness = 752px wide; zoomed to exactly
    // fit the 4-beat loop, that's 188px/beat. Double-clicking the middle of the chord block's own
    // bar (beat 2, now at x = kGutterWidth + 2*188 = 416) in the chord lane must hit and delete it
    // - at the old default zoom (80px/beat) that same screen x would land around beat 4.7, well
    // outside the block, so this only passes if the zoom genuinely took effect.
    constexpr float kExpectedPixelsPerBeat = (800.f - kGutterWidth - kScrollbarThickness) / 4.f;
    const auto chordLaneY = 400.f - kScrollbarThickness - kChordLaneHeight + 4.f;
    const juce::Point<float> midBarPos { kGutterWidth + 2.f * kExpectedPixelsPerBeat, chordLaneY };
    editor.mouseDoubleClick(makeMouseEvent(editor, midBarPos));

    CHECK(editor.getChordBlockCount() == 0);
}

TEST_CASE("MidiEditor: clicking an unselected note selects only it; clicking a different note replaces the selection", "[MidiEditor][MultiSelect]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    addNoteAt(editor, 1.0, 64); // index 0
    addNoteAt(editor, 5.0, 55); // index 1
    REQUIRE(editor.getNoteCount() == 2);

    click(editor, { beatToX(1.0) + 40.f, pitchToY(64) + 1.f }); // click note 0's body
    click(editor, { beatToX(5.0) + 40.f, pitchToY(55) + 1.f }); // click note 1's body - replaces the selection

    // Indirect proof only note 1 ended up selected: Delete removes it and leaves note 0 untouched
    // (there's no public accessor for the selection itself - see MidiEditor.h).
    editor.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey));

    REQUIRE(editor.getNoteCount() == 1);
    CHECK(*editor.getNoteStartBeat(0) == Catch::Approx(1.0));
    CHECK(*editor.getNoteMidiPitch(0) == 64);
}

TEST_CASE("MidiEditor: a marquee drag that intersects two notes without fully containing them selects both", "[MidiEditor][MultiSelect]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    addNoteAt(editor, 1.0, 64); // bounds x:[120,200) y:[72,88)
    addNoteAt(editor, 5.0, 55); // bounds x:[440,520) y:[216,232)
    REQUIRE(editor.getNoteCount() == 2);

    // Fully contains note 0 but only clips note 1's corner - proves selection uses
    // juce::Rectangle::intersects (partial overlap counts), not full containment.
    marqueeSelect(editor, { 100.f, 60.f }, { 480.f, 220.f });

    editor.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey));
    CHECK(editor.getNoteCount() == 0);
}

TEST_CASE("MidiEditor: a trivial click on empty space clears an existing selection", "[MidiEditor][MultiSelect]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    addNoteAt(editor, 1.0, 64);
    REQUIRE(editor.getNoteCount() == 1);

    click(editor, { beatToX(1.0) + 40.f, pitchToY(64) + 1.f }); // select it
    click(editor, { 700.f, 300.f }); // empty space, no movement

    // Indirect proof the selection is now empty: Delete is a no-op.
    CHECK_FALSE(editor.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey)));
    CHECK(editor.getNoteCount() == 1);
}

TEST_CASE("MidiEditor: dragging one of several selected notes moves all of them by the same delta, preserving their relative offsets", "[MidiEditor][MultiSelect]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    addNoteAt(editor, 1.0, 64); // index 0 - will be the drag anchor
    addNoteAt(editor, 5.0, 55); // index 1
    marqueeSelect(editor, { 100.f, 60.f }, { 480.f, 220.f });

    const juce::Point<float> start { beatToX(1.0) + 40.f, pitchToY(64) + 1.f }; // note 0's body
    const juce::Point<float> dragged { start.x + 2.f * kPixelsPerBeat, start.y - 3.f * kRowHeight }; // +2 beats, +3 semitones

    editor.mouseDown(makeMouseEvent(editor, start));
    editor.mouseDrag(makeMouseEvent(editor, dragged));
    editor.mouseUp(makeMouseEvent(editor, dragged));

    CHECK(*editor.getNoteStartBeat(0) == Catch::Approx(3.0));
    CHECK(*editor.getNoteMidiPitch(0) == 67);
    CHECK(*editor.getNoteStartBeat(1) == Catch::Approx(7.0));
    CHECK(*editor.getNoteMidiPitch(1) == 58);

    // Relative offsets between the two notes are preserved.
    CHECK(*editor.getNoteStartBeat(1) - *editor.getNoteStartBeat(0) == Catch::Approx(4.0));
    CHECK(*editor.getNoteMidiPitch(1) - *editor.getNoteMidiPitch(0) == -9);
}

TEST_CASE("MidiEditor: resizing one selected note's right edge extends every selected note's length by the same delta", "[MidiEditor][MultiSelect]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    addNoteAt(editor, 1.0, 64); // index 0 - anchor, bounds x:[120,200)
    addNoteAt(editor, 1.0, 50); // index 1, same column so one marquee column covers both rows
    marqueeSelect(editor, { 110.f, 60.f }, { 210.f, 320.f });

    const auto rightEdgeX = beatToX(1.0 + 1.0); // = 200
    const juce::Point<float> start { rightEdgeX - 3.f, pitchToY(64) + 1.f }; // inside the resize handle's 7px zone
    const juce::Point<float> dragged { start.x + kPixelsPerBeat, start.y }; // +1 beat of length

    editor.mouseDown(makeMouseEvent(editor, start));
    editor.mouseDrag(makeMouseEvent(editor, dragged));
    editor.mouseUp(makeMouseEvent(editor, dragged));

    CHECK(*editor.getNoteStartBeat(0) == Catch::Approx(1.0));
    CHECK(*editor.getNoteLengthBeats(0) == Catch::Approx(2.0));
    CHECK(*editor.getNoteStartBeat(1) == Catch::Approx(1.0));
    CHECK(*editor.getNoteLengthBeats(1) == Catch::Approx(2.0));
}

TEST_CASE("MidiEditor: resizing one selected note's left edge keeps every selected note's own end fixed while extending it by the same delta", "[MidiEditor][MultiSelect]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    addNoteAt(editor, 5.0, 64); // index 0 - anchor, bounds x:[440,520), own end at beat 6.0
    addNoteAt(editor, 1.0, 50); // index 1, bounds x:[120,200), own end at beat 2.0 - deliberately
                                // different from the anchor's, to prove each note keeps its OWN end
                                // fixed rather than everyone snapping to a shared end.
    marqueeSelect(editor, { 700.f, 50.f }, { 60.f, 320.f });

    const auto leftEdgeX = beatToX(5.0); // = 440
    const juce::Point<float> start { leftEdgeX + 3.f, pitchToY(64) + 1.f }; // inside the resize handle's 7px zone
    const juce::Point<float> dragged { start.x - 0.5f * kPixelsPerBeat, start.y }; // start half a beat earlier

    editor.mouseDown(makeMouseEvent(editor, start));
    editor.mouseDrag(makeMouseEvent(editor, dragged));
    editor.mouseUp(makeMouseEvent(editor, dragged));

    CHECK(*editor.getNoteStartBeat(0) == Catch::Approx(4.5));
    CHECK(*editor.getNoteStartBeat(0) + *editor.getNoteLengthBeats(0) == Catch::Approx(6.0));
    CHECK(*editor.getNoteStartBeat(1) == Catch::Approx(0.5));
    CHECK(*editor.getNoteStartBeat(1) + *editor.getNoteLengthBeats(1) == Catch::Approx(2.0));
}

TEST_CASE("MidiEditor: clicking (no drag) one note out of an existing multi-selection collapses the selection to just that one", "[MidiEditor][MultiSelect]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    addNoteAt(editor, 1.0, 64); // index 0
    addNoteAt(editor, 5.0, 55); // index 1
    marqueeSelect(editor, { 100.f, 60.f }, { 480.f, 220.f });

    // A plain click (no movement) on note 0, already part of the selection, narrows it to just that one.
    click(editor, { beatToX(1.0) + 40.f, pitchToY(64) + 1.f });

    editor.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey));

    // Only note 0 was removed - note 1 survives (now at index 0 after the erase).
    REQUIRE(editor.getNoteCount() == 1);
    CHECK(*editor.getNoteStartBeat(0) == Catch::Approx(5.0));
    CHECK(*editor.getNoteMidiPitch(0) == 55);
}

TEST_CASE("MidiEditor::keyPressed(deleteKey) removes every selected note and clears the selection; a no-op when nothing is selected", "[MidiEditor][MultiSelect]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    CHECK_FALSE(editor.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey)));

    addNoteAt(editor, 1.0, 64);
    addNoteAt(editor, 5.0, 55);
    REQUIRE(editor.getNoteCount() == 2);

    // Adding notes doesn't select them - still a no-op.
    CHECK_FALSE(editor.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey)));
    CHECK(editor.getNoteCount() == 2);

    marqueeSelect(editor, { 100.f, 60.f }, { 480.f, 220.f });

    CHECK(editor.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey)));
    CHECK(editor.getNoteCount() == 0);

    // Selection is now empty again - another Delete is a no-op.
    CHECK_FALSE(editor.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey)));
}

TEST_CASE("MidiEditor: arrow keys nudge every selected note by kSnapBeats/one semitone; a no-op when nothing is selected", "[MidiEditor][MultiSelect]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    addNoteAt(editor, 1.0, 64); // index 0
    addNoteAt(editor, 5.0, 55); // index 1
    REQUIRE(editor.getNoteCount() == 2);

    CHECK_FALSE(editor.keyPressed(juce::KeyPress(juce::KeyPress::rightKey)));
    CHECK(*editor.getNoteStartBeat(0) == Catch::Approx(1.0));

    marqueeSelect(editor, { 100.f, 60.f }, { 480.f, 220.f });

    CHECK(editor.keyPressed(juce::KeyPress(juce::KeyPress::rightKey)));
    CHECK(*editor.getNoteStartBeat(0) == Catch::Approx(1.25));
    CHECK(*editor.getNoteStartBeat(1) == Catch::Approx(5.25));

    CHECK(editor.keyPressed(juce::KeyPress(juce::KeyPress::leftKey)));
    CHECK(*editor.getNoteStartBeat(0) == Catch::Approx(1.0));
    CHECK(*editor.getNoteStartBeat(1) == Catch::Approx(5.0));

    CHECK(editor.keyPressed(juce::KeyPress(juce::KeyPress::upKey)));
    CHECK(*editor.getNoteMidiPitch(0) == 65);
    CHECK(*editor.getNoteMidiPitch(1) == 56);

    CHECK(editor.keyPressed(juce::KeyPress(juce::KeyPress::downKey)));
    CHECK(*editor.getNoteMidiPitch(0) == 64);
    CHECK(*editor.getNoteMidiPitch(1) == 55);
}
