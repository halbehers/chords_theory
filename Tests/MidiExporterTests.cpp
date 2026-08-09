#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <set>

#include "Theory/ChordDatabase.h"
#include "Theory/MidiEditorState.h"
#include "Theory/MidiExporter.h"
#include "Theory/ResolvedProgressionSlot.h"

using theory::ChordDatabase;
using theory::Degree;
using theory::Key;
using theory::MidiEditorNoteState;
using theory::MidiExporter;
using theory::ResolvedProgressionSlot;
using theory::Scale;

namespace
{
    juce::MidiMessageSequence readBackSoleTrack(const juce::File& midiFile)
    {
        juce::MidiFile file;
        auto stream = midiFile.createInputStream();
        REQUIRE(stream != nullptr);
        REQUIRE(file.readFrom(*stream));
        REQUIRE(file.getNumTracks() == 1);

        return *file.getTrack(0);
    }

    int countNoteOns(const juce::MidiMessageSequence& sequence)
    {
        int count = 0;
        for (int i = 0; i < sequence.getNumEvents(); ++i)
        {
            if (sequence.getEventPointer(i)->message.isNoteOn())
                ++count;
        }
        return count;
    }
}

TEST_CASE("writeSingleChordMidiFile produces a readable file with one note-on per chord tone at tick 0", "[MidiExporter]")
{
    const auto& degreeI = ChordDatabase::getInstance().get(Key::C, Scale::Major).degrees.front();
    const auto& chord = degreeI.chords.front(); // C major triad - 3 notes

    const auto file = MidiExporter::writeSingleChordMidiFile(chord);
    REQUIRE(file.existsAsFile());

    const auto sequence = readBackSoleTrack(file);
    CHECK(countNoteOns(sequence) == static_cast<int>(chord.notes.size()));

    for (int i = 0; i < sequence.getNumEvents(); ++i)
    {
        const auto& message = sequence.getEventPointer(i)->message;
        if (message.isNoteOn())
            CHECK(message.getTimeStamp() == Catch::Approx(0.0));
    }

    file.deleteFile();
}

TEST_CASE("writeSingleChordMidiFile places note-off at exactly one measure (4 beats) later", "[MidiExporter]")
{
    const auto& degreeI = ChordDatabase::getInstance().get(Key::C, Scale::Major).degrees.front();
    const auto& chord = degreeI.chords.front();

    const auto file = MidiExporter::writeSingleChordMidiFile(chord);
    const auto sequence = readBackSoleTrack(file);

    const double expectedMeasureTicks = static_cast<double>(MidiExporter::kBeatsPerMeasure * MidiExporter::kTicksPerQuarterNote);

    int noteOffCount = 0;
    for (int i = 0; i < sequence.getNumEvents(); ++i)
    {
        const auto& message = sequence.getEventPointer(i)->message;
        if (message.isNoteOff())
        {
            CHECK(message.getTimeStamp() == Catch::Approx(expectedMeasureTicks));
            ++noteOffCount;
        }
    }
    CHECK(noteOffCount == static_cast<int>(chord.notes.size()));

    file.deleteFile();
}

TEST_CASE("writeProgressionMidiFile places each slot's chord at its cumulative one-measure offset", "[MidiExporter]")
{
    const auto& data = ChordDatabase::getInstance().get(Key::C, Scale::Major);
    const auto& degreeI = *data.findDegree(Degree::I);
    const auto& degreeV = *data.findDegree(Degree::V);
    const auto& degreeVI = *data.findDegree(Degree::VI);

    const std::vector<ResolvedProgressionSlot> slots = {
        { { Degree::I, 0 }, degreeI.chords.front() },
        { { Degree::V, 0 }, degreeV.chords.front() },
        { { Degree::VI, 0 }, degreeVI.chords.front() },
    };

    const auto file = MidiExporter::writeProgressionMidiFile(slots);
    const auto sequence = readBackSoleTrack(file);

    const int expectedNoteOns = static_cast<int>(degreeI.chords.front().notes.size()
        + degreeV.chords.front().notes.size() + degreeVI.chords.front().notes.size());
    CHECK(countNoteOns(sequence) == expectedNoteOns);

    const double measureTicks = static_cast<double>(MidiExporter::kBeatsPerMeasure * MidiExporter::kTicksPerQuarterNote);

    std::set<double> noteOnTimestamps;
    for (int i = 0; i < sequence.getNumEvents(); ++i)
    {
        const auto& message = sequence.getEventPointer(i)->message;
        if (message.isNoteOn())
            noteOnTimestamps.insert(message.getTimeStamp());
    }

    // Three distinct start times: 0, one measure, two measures.
    CHECK(noteOnTimestamps == std::set<double> { 0.0, measureTicks, measureTicks * 2.0 });

    file.deleteFile();
}

TEST_CASE("writeProgressionMidiFile with an empty slot list still produces a readable (empty) file", "[MidiExporter]")
{
    const auto file = MidiExporter::writeProgressionMidiFile({});
    REQUIRE(file.existsAsFile());

    const auto sequence = readBackSoleTrack(file);
    CHECK(countNoteOns(sequence) == 0);

    file.deleteFile();
}

TEST_CASE("writeMidiEditorContentFile places each note at its exact startBeat/lengthBeats tick position, not measure-quantized", "[MidiExporter]")
{
    const std::vector<MidiEditorNoteState> notes = {
        { 60, 0.0, 1.0 },  // C4, beat 0-1
        { 64, 0.5, 2.5 },  // E4, beat 0.5-3 - deliberately off the bar grid
        { 67, 3.0, 0.25 }, // G4, beat 3-3.25
    };

    const auto file = MidiExporter::writeMidiEditorContentFile(notes);
    REQUIRE(file.existsAsFile());

    const auto sequence = readBackSoleTrack(file);
    CHECK(countNoteOns(sequence) == 3);

    std::set<double> noteOnTimestamps;
    std::set<double> noteOffTimestamps;
    for (int i = 0; i < sequence.getNumEvents(); ++i)
    {
        const auto& message = sequence.getEventPointer(i)->message;
        if (message.isNoteOn())
            noteOnTimestamps.insert(message.getTimeStamp());
        else if (message.isNoteOff())
            noteOffTimestamps.insert(message.getTimeStamp());
    }

    const auto ticksPerBeat = static_cast<double>(MidiExporter::kTicksPerQuarterNote);
    CHECK(noteOnTimestamps == std::set<double> { 0.0, 0.5 * ticksPerBeat, 3.0 * ticksPerBeat });
    CHECK(noteOffTimestamps == std::set<double> { 1.0 * ticksPerBeat, 3.0 * ticksPerBeat, 3.25 * ticksPerBeat });

    file.deleteFile();
}

TEST_CASE("writeMidiEditorContentFile with an empty note list still produces a readable (empty) file", "[MidiExporter]")
{
    const auto file = MidiExporter::writeMidiEditorContentFile({});
    REQUIRE(file.existsAsFile());

    const auto sequence = readBackSoleTrack(file);
    CHECK(countNoteOns(sequence) == 0);

    file.deleteFile();
}
