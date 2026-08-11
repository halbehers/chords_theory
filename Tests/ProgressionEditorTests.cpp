#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Audio/ProgressionPlayer.h"
#include "Component/MidiEditor.h"
#include "Component/ProgressionEditor.h"
#include "Theory/ChordDatabase.h"
#include "Theory/ProgressionPreset.h"

using audio::ProgressionPlayer;
using component::MidiEditor;
using component::ProgressionEditor;
using theory::Chord;
using theory::ChordDatabase;
using theory::Degree;
using theory::Key;
using theory::ProgressionPreset;
using theory::ProgressionSlot;
using theory::Scale;

namespace
{
    // A real per-degree chord from C Major's own catalog - chord blocks are now always detected
    // from actual placed notes (see theory::ChordIdentifier), so a bare symbol/popularityOrder stub
    // with no real Chord::notes (like this file's old makeChord() helper) would place zero notes
    // and never be detected at all. ProgressionEditor's own MidiEditor defaults to Key::C/Scale::
    // Major, so these round-trip back through detection unchanged.
    const Chord& realChord(Degree degree)
    {
        return ChordDatabase::getInstance().get(Key::C, Scale::Major).findDegree(degree)->chords.front();
    }

    struct RecordingListener : public ProgressionEditor::Listener
    {
        int contentChangedCount = 0;
        int fileDroppedCount = 0;
        double lastFileDroppedBeat = -1.0;
        int playbackStateChangedCount = 0;
        bool lastPlaybackState = false;
        int chordBlockDragStartedCount = 0;
        int lastChordBlockDragIndex = -1;

        void onChordFileDropped(double startBeat, const juce::String&) override
        {
            ++fileDroppedCount;
            lastFileDroppedBeat = startBeat;
        }
        void onProgressionDragStarted() override {}
        void onContentChanged() override { ++contentChangedCount; }
        void onPlaybackStateChanged(bool isPlaying) override
        {
            ++playbackStateChangedCount;
            lastPlaybackState = isPlaying;
        }
        void onChordBlockDragStarted(int chordBlockIndex) override
        {
            ++chordBlockDragStartedCount;
            lastChordBlockDragIndex = chordBlockIndex;
        }
    };

    // Mirrors MidiEditorTests.cpp's own local constants/helper (this codebase's convention -
    // per-file duplication rather than a shared test helper) - just enough to reach into the
    // MidiEditor child's chord lane at its known, unzoomed/unscrolled default layout.
    constexpr float kGutterWidth = 40.f;
    constexpr float kPixelsPerBeat = 80.f;
    constexpr float kScrollbarThickness = 8.f;
    constexpr float kChordLaneHeight = 28.f;

    float beatToX(double beat) { return kGutterWidth + static_cast<float>(beat) * kPixelsPerBeat; }
    float chordLaneY(float editorHeight) { return editorHeight - kScrollbarThickness - kChordLaneHeight; }

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

    // Both nelement::SVGButton and nelement::TextButton wrap a single, always-first, internal
    // juce::Button child that actually receives clicks - triggerClick() drives their real onClick
    // handler without needing real mouse events or exact on-screen geometry. Same pattern as
    // VoicingSelectorTests.cpp's own helper.
    void triggerButtonClick(juce::Component& buttonWrapper)
    {
        auto* button = dynamic_cast<juce::Button*>(buttonWrapper.getChildComponent(0));
        REQUIRE(button != nullptr);
        button->triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
    }
}

TEST_CASE("ProgressionEditor::loadPreset places one chord block per bar, in order", "[ProgressionEditor]")
{
    const Chord& chordI = realChord(Degree::I);
    const Chord& chordV = realChord(Degree::V);
    const Chord& chordVI = realChord(Degree::VI);

    ProgressionEditor sequencer("test-sequencer",
        [&](const ProgressionSlot& slot) -> const Chord*
        {
            if (slot.degree == Degree::I) return &chordI;
            if (slot.degree == Degree::V) return &chordV;
            if (slot.degree == Degree::VI) return &chordVI;
            return nullptr;
        });

    ProgressionPreset preset;
    preset.id = "test-preset";
    preset.slots = { ProgressionSlot { Degree::I, 0 }, ProgressionSlot { Degree::V, 0 }, ProgressionSlot { Degree::VI, 0 } };

    sequencer.loadPreset(preset);

    const auto populated = sequencer.getPopulatedSlots();
    REQUIRE(populated.size() == 3);
    CHECK(populated[0].degree == Degree::I);
    CHECK(populated[1].degree == Degree::V);
    CHECK(populated[2].degree == Degree::VI);
}

TEST_CASE("ProgressionEditor::loadPreset leaves an unresolvable slot's bar empty without shifting later slots", "[ProgressionEditor]")
{
    const Chord& chordI = realChord(Degree::I);
    const Chord& chordIV = realChord(Degree::IV);

    ProgressionEditor sequencer("test-sequencer",
        [&](const ProgressionSlot& slot) -> const Chord*
        {
            // Degree II never resolves - simulates a scale (e.g. Minor Blues) where that degree
            // doesn't exist.
            if (slot.degree == Degree::I) return &chordI;
            if (slot.degree == Degree::IV) return &chordIV;
            return nullptr;
        });

    ProgressionPreset preset;
    preset.id = "test-preset";
    preset.slots = { ProgressionSlot { Degree::I, 0 }, ProgressionSlot { Degree::II, 0 }, ProgressionSlot { Degree::IV, 0 } };

    sequencer.loadPreset(preset);

    // Only 2 chord blocks were actually created (degree II never resolved), but the surviving ones
    // still sit at their own bar position - bar 0 (degree I) and bar 2 (degree IV), not bar 0/1.
    auto* midiEditor = dynamic_cast<MidiEditor*>(sequencer.findChildWithID("progression-midi-editor"));
    REQUIRE(midiEditor != nullptr);
    REQUIRE(midiEditor->getChordBlockCount() == 2);

    REQUIRE(midiEditor->getChordBlockStartBeat(0).has_value());
    CHECK(*midiEditor->getChordBlockStartBeat(0) == Catch::Approx(0.0));
    REQUIRE(midiEditor->getChordBlockStartBeat(1).has_value());
    CHECK(*midiEditor->getChordBlockStartBeat(1) == Catch::Approx(2.0 * MidiEditor::kBeatsPerBar));

    const auto populated = sequencer.getPopulatedSlots();
    REQUIRE(populated.size() == 2);
    CHECK(populated[0].degree == Degree::I);
    CHECK(populated[1].degree == Degree::IV);
}

TEST_CASE("ProgressionEditor::getPopulatedSlots derives each block's degree/popularityOrder from its placed notes, not a live re-resolution of the ChordResolver", "[ProgressionEditor]")
{
    const auto& cMajorDegreeI = ChordDatabase::getInstance().get(Key::C, Scale::Major).findDegree(Degree::I)->chords;
    Chord currentForI = cMajorDegreeI[1]; // popularityOrder 2, "Cmaj7"

    ProgressionEditor sequencer("test-sequencer",
        [&currentForI](const ProgressionSlot& slot) -> const Chord*
        {
            return slot.degree == Degree::I ? &currentForI : nullptr;
        });

    sequencer.addChordAtBeat(0.0, currentForI);

    // Change what the resolver would now return for degree I - the already-placed notes are
    // unaffected, and detection re-derives identity from THEM, not from re-invoking the resolver, so
    // this has no effect on the block that's already there.
    currentForI = cMajorDegreeI.front(); // popularityOrder 1, the plain "C" triad

    const auto populated = sequencer.getPopulatedSlots();
    REQUIRE(populated.size() == 1);
    CHECK(populated.front().degree == Degree::I);
    CHECK(populated.front().popularityOrder == 2); // still Cmaj7's value, from the notes actually placed
}

TEST_CASE("ProgressionEditor::getPopulatedSlots returns chord blocks sorted by their position, not insertion order", "[ProgressionEditor]")
{
    const Chord& chordI = realChord(Degree::I);
    const Chord& chordV = realChord(Degree::V);

    ProgressionEditor sequencer("test-sequencer",
        [](const ProgressionSlot&) -> const Chord* { return nullptr; });

    // Insert the later bar's chord first.
    sequencer.addChordAtBeat(MidiEditor::kBeatsPerBar, chordV);
    sequencer.addChordAtBeat(0.0, chordI);

    const auto populated = sequencer.getPopulatedSlots();
    REQUIRE(populated.size() == 2);
    CHECK(populated[0].degree == Degree::I);
    CHECK(populated[1].degree == Degree::V);
}

TEST_CASE("ProgressionEditor: a chord file dropped on the MidiEditor bubbles up to this component's own listeners", "[ProgressionEditor]")
{
    ProgressionEditor sequencer("test-sequencer",
        [](const ProgressionSlot&) -> const Chord* { return nullptr; });
    sequencer.setBounds(0, 0, 800, 400);

    RecordingListener listener;
    sequencer.addListener(&listener);

    auto* midiEditor = sequencer.findChildWithID("progression-midi-editor");
    REQUIRE(midiEditor != nullptr);
    auto* dropTarget = dynamic_cast<juce::FileDragAndDropTarget*>(midiEditor);
    REQUIRE(dropTarget != nullptr);

    juce::StringArray files;
    files.add("/tmp/test-chord.mid");
    dropTarget->filesDropped(files, 100, 100);

    CHECK(listener.fileDroppedCount == 1);
    CHECK(listener.lastFileDroppedBeat >= 0.0);

    sequencer.removeListener(&listener);
}

TEST_CASE("ProgressionEditor: a MidiEditor chord lane drag bubbles up to this component's own listeners", "[ProgressionEditor]")
{
    const Chord& chord = realChord(Degree::I);

    ProgressionEditor sequencer("test-sequencer",
        [](const ProgressionSlot&) -> const Chord* { return nullptr; });
    sequencer.setBounds(0, 0, 800, 400);
    sequencer.addChordAtBeat(0.0, chord); // occupies beats [0, 4)

    RecordingListener listener;
    sequencer.addListener(&listener);

    auto* midiEditor = dynamic_cast<MidiEditor*>(sequencer.findChildWithID("progression-midi-editor"));
    REQUIRE(midiEditor != nullptr);

    // ProgressionEditor's own grid gives _midiEditor a fraction of the 800x400 sequencer's height
    // (it shares the grid with the preset/drag-out-button header row) - override its bounds directly so
    // this file's beatToX/chordLaneY constants (mirroring MidiEditor.cpp's own fixed layout at an
    // 800x400 size) apply exactly as they do in MidiEditorTests.cpp's own tests.
    midiEditor->setBounds(0, 0, 800, 400);

    const auto laneY = chordLaneY(400.f) + kChordLaneHeight * 0.5f;
    const juce::Point<float> start { beatToX(1.0), laneY };
    const juce::Point<float> dragged { beatToX(1.0) + 10.f, laneY }; // past the 3px click-vs-drag threshold

    midiEditor->mouseDown(makeMouseEvent(*midiEditor, start));
    midiEditor->mouseDrag(makeMouseEvent(*midiEditor, dragged));

    CHECK(listener.chordBlockDragStartedCount == 1);
    CHECK(listener.lastChordBlockDragIndex == 0);

    midiEditor->mouseUp(makeMouseEvent(*midiEditor, dragged));

    sequencer.removeListener(&listener);
}

TEST_CASE("ProgressionEditor: a MidiEditor content change bubbles up to this component's own listeners", "[ProgressionEditor]")
{
    const Chord& chord = realChord(Degree::I);
    ProgressionEditor sequencer("test-sequencer",
        [](const ProgressionSlot&) -> const Chord* { return nullptr; });

    RecordingListener listener;
    sequencer.addListener(&listener);

    sequencer.addChordAtBeat(0.0, chord);

    CHECK(listener.contentChangedCount == 1);

    sequencer.removeListener(&listener);
}

TEST_CASE("ProgressionEditor: clicking the play button toggles playback and swaps its icon", "[ProgressionEditor]")
{
    ProgressionPlayer player;
    // Needs a chord with real notes (unlike makeChord()'s bare symbol/popularityOrder stub used
    // elsewhere in this file) - playback has nothing to play otherwise, and startPlayback() no-ops.
    const Chord& chord = ChordDatabase::getInstance().get(Key::C, Scale::Major).degrees.front().chords.front();

    ProgressionEditor sequencer("test-sequencer",
        [](const ProgressionSlot&) -> const Chord* { return nullptr; },
        &player);
    sequencer.setBounds(0, 0, 800, 400);

    sequencer.addChordAtBeat(0.0, chord);

    auto* playButton = dynamic_cast<nelement::SVGButton*>(sequencer.findChildWithID("progression-play-button"));
    REQUIRE(playButton != nullptr);
    CHECK(playButton->getIconBinary() == nui::Icons::getPlay());

    auto* midiEditor = dynamic_cast<MidiEditor*>(sequencer.findChildWithID("progression-midi-editor"));
    REQUIRE(midiEditor != nullptr);

    triggerButtonClick(*playButton);

    CHECK(midiEditor->isPlaying());
    CHECK(playButton->getIconBinary() == nui::Icons::getStop());

    triggerButtonClick(*playButton);

    CHECK_FALSE(midiEditor->isPlaying());
    CHECK(playButton->getIconBinary() == nui::Icons::getPlay());
}

TEST_CASE("ProgressionEditor: a MidiEditor playback state change bubbles up to this component's own listeners", "[ProgressionEditor]")
{
    ProgressionPlayer player;
    const Chord& chord = ChordDatabase::getInstance().get(Key::C, Scale::Major).degrees.front().chords.front();

    ProgressionEditor sequencer("test-sequencer",
        [](const ProgressionSlot&) -> const Chord* { return nullptr; },
        &player);
    sequencer.setBounds(0, 0, 800, 400);
    sequencer.addChordAtBeat(0.0, chord);

    RecordingListener listener;
    sequencer.addListener(&listener);

    auto* playButton = dynamic_cast<nelement::SVGButton*>(sequencer.findChildWithID("progression-play-button"));
    REQUIRE(playButton != nullptr);

    triggerButtonClick(*playButton);
    CHECK(listener.playbackStateChangedCount == 1);
    CHECK(listener.lastPlaybackState);

    triggerButtonClick(*playButton);
    CHECK(listener.playbackStateChangedCount == 2);
    CHECK_FALSE(listener.lastPlaybackState);

    sequencer.removeListener(&listener);
}

TEST_CASE("ProgressionEditor: chord-block and playback forwarding getters match the underlying MidiEditor", "[ProgressionEditor]")
{
    ProgressionPlayer player;
    const Chord& chord = realChord(Degree::I);

    ProgressionEditor sequencer("test-sequencer",
        [](const ProgressionSlot&) -> const Chord* { return nullptr; },
        &player);
    sequencer.setBounds(0, 0, 800, 400);

    auto* midiEditor = dynamic_cast<MidiEditor*>(sequencer.findChildWithID("progression-midi-editor"));
    REQUIRE(midiEditor != nullptr);

    sequencer.addChordAtBeat(0.0, chord);

    REQUIRE(sequencer.getChordBlockCount() == midiEditor->getChordBlockCount());
    REQUIRE(sequencer.getChordBlockCount() > 0);
    CHECK(sequencer.getChordBlockStartBeat(0) == midiEditor->getChordBlockStartBeat(0));
    CHECK(sequencer.getChordBlockLengthBeats(0) == midiEditor->getChordBlockLengthBeats(0));
    CHECK(sequencer.getChordBlockLabel(0) == midiEditor->getChordBlockLabel(0));

    CHECK_FALSE(sequencer.isPlaying());
    sequencer.startPlayback();
    CHECK(sequencer.isPlaying());
    CHECK(midiEditor->isPlaying());
    sequencer.stopPlayback();
    CHECK_FALSE(sequencer.isPlaying());
    CHECK_FALSE(midiEditor->isPlaying());
}

TEST_CASE("ProgressionEditor: loadPreset then Play schedules every chord's notes, including the last one", "[ProgressionEditor]")
{
    ProgressionPlayer player;
    const Chord& chordI = realChord(Degree::I);
    const Chord& chordIV = realChord(Degree::IV);
    const Chord& chordV = realChord(Degree::V);
    const Chord& chordVI = realChord(Degree::VI);

    ProgressionEditor sequencer("test-sequencer",
        [&](const ProgressionSlot& slot) -> const Chord*
        {
            if (slot.degree == Degree::I) return &chordI;
            if (slot.degree == Degree::IV) return &chordIV;
            if (slot.degree == Degree::V) return &chordV;
            if (slot.degree == Degree::VI) return &chordVI;
            return nullptr;
        },
        &player);
    sequencer.setBounds(0, 0, 800, 400);

    ProgressionPreset preset;
    preset.id = "test-preset";
    preset.slots = { ProgressionSlot { Degree::I, 0 }, ProgressionSlot { Degree::IV, 0 },
                      ProgressionSlot { Degree::V, 0 }, ProgressionSlot { Degree::VI, 0 } };
    sequencer.loadPreset(preset);

    auto* playButton = dynamic_cast<nelement::SVGButton*>(sequencer.findChildWithID("progression-play-button"));
    REQUIRE(playButton != nullptr);
    triggerButtonClick(*playButton);
    REQUIRE(player.isPlaying());

    // Step through just over one full loop pass in small blocks, matching a real audio callback
    // (not one giant call) - real MIDI note numbers only start at their own bar, so a note-on's
    // cumulative sample offset tells us which chord (bar) it belongs to.
    constexpr double kSampleRate = 44100.0;
    constexpr double kBpm = 120.0;
    constexpr int kBlockSize = 512;
    const auto samplesPerBeat = (60.0 / kBpm) * kSampleRate;
    const auto loopLengthSamples = static_cast<int>(4.0 * MidiEditor::kBeatsPerBar * samplesPerBeat);

    std::vector<int> noteOnOffsets;
    int samplesProcessed = 0;
    while (samplesProcessed < loopLengthSamples + kBlockSize)
    {
        juce::MidiBuffer buffer;
        player.renderNextBlock(buffer, kBlockSize, kSampleRate, kBpm);
        for (const auto metadata : buffer)
            if (metadata.getMessage().isNoteOn())
                noteOnOffsets.push_back(samplesProcessed + metadata.samplePosition);
        samplesProcessed += kBlockSize;
    }

    // One onset-cluster expected per bar (0, 1, 2, 3) - assert the 4th (last) bar's cluster exists,
    // not just that *some* note-on happened somewhere in the pass.
    const auto lastChordStartSamples = 3.0 * MidiEditor::kBeatsPerBar * samplesPerBeat;
    const auto lastChordEndSamples = 4.0 * MidiEditor::kBeatsPerBar * samplesPerBeat;
    const auto lastChordOnsets = std::count_if(noteOnOffsets.begin(), noteOnOffsets.end(),
        [&](int offset) { return offset >= lastChordStartSamples && offset < lastChordEndSamples; });

    CHECK(lastChordOnsets > 0);
}
