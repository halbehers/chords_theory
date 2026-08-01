#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Audio/ChordSynthEngine.h"

using audio::ChordSynthEngine;

namespace
{
    constexpr double kSampleRate = 44100.0;
    constexpr int kBlockSize = 512;
}

TEST_CASE("ChordSynthEngine::previewChord produces audible output while sounding", "[ChordSynthEngine]")
{
    ChordSynthEngine engine;
    engine.prepare(kSampleRate, kBlockSize);

    juce::AudioBuffer<float> buffer(2, kBlockSize);
    buffer.clear();

    juce::MidiBuffer midi;
    engine.previewChord({ 60, 64, 67 }); // C major triad, closed voicing near middle C

    engine.renderNextBlock(buffer, midi, 0, kBlockSize);

    CHECK(buffer.getMagnitude(0, kBlockSize) > 0.0f);
}

TEST_CASE("ChordSynthEngine::previewChord with no notes is a safe, silent no-op", "[ChordSynthEngine]")
{
    ChordSynthEngine engine;
    engine.prepare(kSampleRate, kBlockSize);

    juce::AudioBuffer<float> buffer(2, kBlockSize);
    buffer.clear();

    juce::MidiBuffer midi;
    REQUIRE_NOTHROW(engine.previewChord({}));

    engine.renderNextBlock(buffer, midi, 0, kBlockSize);

    CHECK(buffer.getMagnitude(0, kBlockSize) == Catch::Approx(0.0f));
}

TEST_CASE("ChordSynthEngine::previewChord auto-releases and falls silent after its fixed duration", "[ChordSynthEngine]")
{
    ChordSynthEngine engine;
    engine.prepare(kSampleRate, kBlockSize);

    juce::AudioBuffer<float> buffer(2, kBlockSize);
    juce::MidiBuffer midi;

    engine.previewChord({ 60, 64, 67 });

    buffer.clear();
    midi.clear();
    engine.renderNextBlock(buffer, midi, 0, kBlockSize);
    CHECK(buffer.getMagnitude(0, kBlockSize) > 0.0f);

    // Let the ~1s preview timer fire and queue the auto-noteOff - it's a message-thread
    // juce::Timer, so it needs the dispatch loop actually pumped, same pattern as
    // PluginProcessorTests.cpp's APVTS-async-flush test.
    juce::MessageManager::getInstance()->runDispatchLoopUntil(1100);

    // Render enough further blocks to carry the release tail (~0.2s) all the way to silence.
    // midi must be cleared before every call - MidiKeyboardState::processNextMidiBuffer doesn't
    // consume/clear the caller's buffer, so a reused, never-cleared MidiBuffer would keep
    // re-delivering the very first noteOn on every call, perpetually retriggering the voices (in
    // real host use this never happens, since the host hands processBlock a fresh buffer every
    // callback).
    float lastMagnitude = 0.0f;
    for (int i = 0; i < 20; ++i)
    {
        buffer.clear();
        midi.clear();
        engine.renderNextBlock(buffer, midi, 0, kBlockSize);
        lastMagnitude = buffer.getMagnitude(0, kBlockSize);
    }

    CHECK(lastMagnitude == Catch::Approx(0.0f));
}
