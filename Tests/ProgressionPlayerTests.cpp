#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

#include "Audio/ProgressionPlayer.h"

using audio::ProgressionPlayer;
using audio::ScheduledNote;

namespace
{
    constexpr double kSampleRate = 44100.0;
    constexpr double kBpm = 120.0; // samplesPerBeat = (60/bpm)*sampleRate = 22050 exactly

    struct MidiEvent
    {
        int offset;
        bool isNoteOn;
        int noteNumber;
    };

    std::vector<MidiEvent> collectEvents(const juce::MidiBuffer& buffer)
    {
        std::vector<MidiEvent> events;
        for (const auto metadata : buffer)
        {
            const auto message = metadata.getMessage();
            if (message.isNoteOnOrOff())
                events.push_back({ metadata.samplePosition, message.isNoteOn(), message.getNoteNumber() });
        }
        return events;
    }

    bool contains(const std::vector<MidiEvent>& events, int offset, bool isNoteOn, int noteNumber)
    {
        return std::any_of(events.begin(), events.end(), [&](const MidiEvent& e)
            { return e.offset == offset && e.isNoteOn == isNoteOn && e.noteNumber == noteNumber; });
    }
}

TEST_CASE("ProgressionPlayer: a note triggers on/off at the exact expected sample offsets", "[ProgressionPlayer]")
{
    ProgressionPlayer player;
    player.setNotes({ { 60, 0.0, 1.0 } }); // C4, one full beat long
    player.setLoopBounds(0.0, 4.0);
    player.play();

    juce::MidiBuffer buffer;
    // 2 beats' worth of samples in one call, so both on and off land in the same block.
    player.renderNextBlock(buffer, 44100, kSampleRate, kBpm);

    const auto events = collectEvents(buffer);
    REQUIRE(events.size() == 2);
    CHECK(contains(events, 0, true, 60));
    CHECK(contains(events, 22050, false, 60));
}

TEST_CASE("ProgressionPlayer: renderNextBlock while not playing adds no events", "[ProgressionPlayer]")
{
    ProgressionPlayer player;
    player.setNotes({ { 60, 0.0, 1.0 } });
    player.setLoopBounds(0.0, 4.0);
    // Deliberately never call play().

    juce::MidiBuffer buffer;
    player.renderNextBlock(buffer, 44100, kSampleRate, kBpm);

    CHECK(collectEvents(buffer).empty());
    CHECK_FALSE(player.isPlaying());
}

TEST_CASE("ProgressionPlayer: a loop wrap force-releases still-sounding notes and retriggers notes at the loop start", "[ProgressionPlayer]")
{
    ProgressionPlayer player;
    // A: starts mid-loop, would naturally run past the loop end - must be force-released at the
    // wrap instead. B: spans the whole loop exactly - retriggers at the top of the next pass.
    player.setNotes({
        { 60, 0.5, 0.6 }, // A
        { 64, 0.0, 1.0 }, // B
    });
    player.setLoopBounds(0.0, 1.0); // one beat = 22050 samples
    player.play();

    juce::MidiBuffer buffer;
    // Spans the full first loop pass (22050 samples) plus 950 samples into the second pass.
    player.renderNextBlock(buffer, 23000, kSampleRate, kBpm);

    const auto events = collectEvents(buffer);
    REQUIRE(events.size() == 5);
    CHECK(contains(events, 0, true, 64));       // B onset, pass 1
    CHECK(contains(events, 11025, true, 60));   // A onset, pass 1 (0.5 beat in)
    CHECK(contains(events, 22050, false, 64));  // wrap force-release - B
    CHECK(contains(events, 22050, false, 60));  // wrap force-release - A (never reached its own end)
    CHECK(contains(events, 22050, true, 64));   // B retriggers at the top of pass 2

    // A hasn't been reached again yet in pass 2 (only 950 of 22050 samples in) - no second onset.
    const auto secondPassAOnsets = std::count_if(events.begin(), events.end(), [](const MidiEvent& e)
        { return e.noteNumber == 60 && e.isNoteOn && e.offset > 22050; });
    CHECK(secondPassAOnsets == 0);

    CHECK(player.getPlayheadBeat() == Catch::Approx(950.0 / 22050.0));
}

TEST_CASE("ProgressionPlayer: a note at the loop start retriggers on every pass, not just the first", "[ProgressionPlayer]")
{
    // Regression test: processing in small blocks that don't evenly divide the loop length (as a
    // real audio callback does, unlike a single giant renderNextBlock call) exercises the
    // std::ceil rounding in the wrap calculation, which used to leave a tiny sub-sample overshoot
    // that got carried into the next pass's start position - silently skipping any note sitting
    // exactly at loopStart (e.g. a progression's first chord) on every pass after the first.
    ProgressionPlayer player;
    player.setNotes({ { 60, 0.0, 0.5 } }); // a short note right at the loop start
    player.setLoopBounds(0.0, 1.0);        // one beat = 22050 samples
    player.play();

    constexpr int kBlockSize = 500; // deliberately doesn't divide 22050 evenly
    constexpr int kTotalSamples = 3 * 22050 + 200; // just over 3 full loop passes

    std::vector<MidiEvent> allEvents;
    int samplesProcessed = 0;
    while (samplesProcessed < kTotalSamples)
    {
        const auto thisBlockSize = juce::jmin(kBlockSize, kTotalSamples - samplesProcessed);
        juce::MidiBuffer buffer;
        player.renderNextBlock(buffer, thisBlockSize, kSampleRate, kBpm);

        for (const auto& event : collectEvents(buffer))
            allEvents.push_back({ event.offset + samplesProcessed, event.isNoteOn, event.noteNumber });

        samplesProcessed += thisBlockSize;
    }

    const auto onsetCount = std::count_if(allEvents.begin(), allEvents.end(), [](const MidiEvent& e)
        { return e.noteNumber == 60 && e.isNoteOn; });
    CHECK(onsetCount == 4); // pass 1, 2, 3, and the start of pass 4

    CHECK(contains(allEvents, 0, true, 60));
    CHECK(contains(allEvents, 22050, true, 60));
    CHECK(contains(allEvents, 44100, true, 60));
    CHECK(contains(allEvents, 66150, true, 60));
}

TEST_CASE("ProgressionPlayer::stop mid-playback force-releases on the next renderNextBlock call", "[ProgressionPlayer]")
{
    ProgressionPlayer player;
    player.setNotes({ { 60, 0.0, 4.0 } }); // long note, won't naturally end within this test
    player.setLoopBounds(0.0, 4.0);
    player.play();

    juce::MidiBuffer onBuffer;
    player.renderNextBlock(onBuffer, 512, kSampleRate, kBpm);
    REQUIRE(contains(collectEvents(onBuffer), 0, true, 60));

    player.stop();
    CHECK_FALSE(player.isPlaying());

    juce::MidiBuffer stopBuffer;
    player.renderNextBlock(stopBuffer, 512, kSampleRate, kBpm);
    const auto stopEvents = collectEvents(stopBuffer);
    REQUIRE(stopEvents.size() == 1);
    CHECK(contains(stopEvents, 0, false, 60));

    // Nothing left to release on the next block - a clean, empty no-op.
    juce::MidiBuffer idleBuffer;
    player.renderNextBlock(idleBuffer, 512, kSampleRate, kBpm);
    CHECK(collectEvents(idleBuffer).empty());
}

TEST_CASE("ProgressionPlayer: isPlaying/getPlayheadBeat reflect state through play/render/stop", "[ProgressionPlayer]")
{
    ProgressionPlayer player;
    CHECK_FALSE(player.isPlaying());
    CHECK(player.getPlayheadBeat() == Catch::Approx(0.0));

    player.setNotes({ { 60, 0.0, 1.0 } });
    player.setLoopBounds(2.0, 6.0); // a non-zero loop start, to confirm play() seeks to it

    player.play();
    CHECK(player.isPlaying());
    CHECK(player.getPlayheadBeat() == Catch::Approx(2.0));

    juce::MidiBuffer buffer;
    player.renderNextBlock(buffer, 11025, kSampleRate, kBpm); // half a beat's worth of samples
    CHECK(player.getPlayheadBeat() == Catch::Approx(2.5));

    player.stop();
    CHECK_FALSE(player.isPlaying());
}
