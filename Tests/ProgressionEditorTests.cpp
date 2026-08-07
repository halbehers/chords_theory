#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>

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
    Chord makeChord(const std::string& symbol, int popularityOrder)
    {
        Chord chord;
        chord.symbol = symbol;
        chord.readableName = symbol;
        chord.popularityOrder = popularityOrder;
        return chord;
    }

    struct RecordingListener : public ProgressionEditor::Listener
    {
        int contentChangedCount = 0;
        int fileDroppedCount = 0;
        double lastFileDroppedBeat = -1.0;

        void onChordFileDropped(double startBeat, const juce::String&) override
        {
            ++fileDroppedCount;
            lastFileDroppedBeat = startBeat;
        }
        void onProgressionDragStarted() override {}
        void onContentChanged() override { ++contentChangedCount; }
    };

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
    const Chord chordI = makeChord("C", 1);
    const Chord chordV = makeChord("G", 1);
    const Chord chordVI = makeChord("Am", 1);

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
    const Chord chordI = makeChord("C", 1);
    const Chord chordIV = makeChord("F", 1);

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

TEST_CASE("ProgressionEditor::getPopulatedSlots reports each block's frozen degree/popularityOrder, not a live-re-resolved one", "[ProgressionEditor]")
{
    Chord currentForI = makeChord("Cmaj7", 2);

    ProgressionEditor sequencer("test-sequencer",
        [&currentForI](const ProgressionSlot& slot) -> const Chord*
        {
            return slot.degree == Degree::I ? &currentForI : nullptr;
        });

    sequencer.addChordAtBeat(0.0, currentForI, ProgressionSlot { Degree::I, currentForI.popularityOrder });

    // Change what the resolver would now return for degree I - a live-tracking model would pick
    // this up on the next getPopulatedSlots() call; the frozen-at-drop-time model must not.
    currentForI = makeChord("C", 1);

    const auto populated = sequencer.getPopulatedSlots();
    REQUIRE(populated.size() == 1);
    CHECK(populated.front().degree == Degree::I);
    CHECK(populated.front().popularityOrder == 2); // the value at drop time, not the now-current 1
}

TEST_CASE("ProgressionEditor::getPopulatedSlots returns chord blocks sorted by their position, not insertion order", "[ProgressionEditor]")
{
    const Chord chordI = makeChord("C", 1);
    const Chord chordV = makeChord("G", 1);

    ProgressionEditor sequencer("test-sequencer",
        [](const ProgressionSlot&) -> const Chord* { return nullptr; });

    // Insert the later bar's chord first.
    sequencer.addChordAtBeat(MidiEditor::kBeatsPerBar, chordV, ProgressionSlot { Degree::V, chordV.popularityOrder });
    sequencer.addChordAtBeat(0.0, chordI, ProgressionSlot { Degree::I, chordI.popularityOrder });

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

TEST_CASE("ProgressionEditor: a MidiEditor content change bubbles up to this component's own listeners", "[ProgressionEditor]")
{
    const Chord chord = makeChord("C", 1);
    ProgressionEditor sequencer("test-sequencer",
        [](const ProgressionSlot&) -> const Chord* { return nullptr; });

    RecordingListener listener;
    sequencer.addListener(&listener);

    sequencer.addChordAtBeat(0.0, chord, ProgressionSlot { Degree::I, chord.popularityOrder });

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

    sequencer.addChordAtBeat(0.0, chord, ProgressionSlot { Degree::I, chord.popularityOrder });

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
