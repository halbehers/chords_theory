#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Audio/DahdsrEnvelope.h"

using audio::DahdsrEnvelope;
using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double kSampleRate = 1000.0; // 1 sample == 1ms, keeps test math trivial
}

TEST_CASE("DahdsrEnvelope: Delay holds at 0 for exactly its duration, then Attack ramps to 1", "[DahdsrEnvelope]")
{
    DahdsrEnvelope envelope;
    envelope.setSampleRate(kSampleRate);
    envelope.setParameters({ .delayMs = 10.0f, .attackMs = 10.0f, .holdMs = 5.0f, .decayMs = 10.0f, .sustainLevel = 0.5f, .releaseMs = 20.0f });

    envelope.noteOn();
    REQUIRE(envelope.getStage() == DahdsrEnvelope::Stage::Delay);

    for (int i = 0; i < 10; ++i)
        CHECK_THAT(envelope.getNextSample(), WithinAbs(0.0f, 1.0e-6f));

    CHECK(envelope.getStage() == DahdsrEnvelope::Stage::Attack);

    for (int i = 1; i <= 9; ++i)
        CHECK_THAT(envelope.getNextSample(), WithinAbs(static_cast<float>(i) / 10.0f, 1.0e-6f));

    CHECK_THAT(envelope.getNextSample(), WithinAbs(1.0f, 1.0e-6f)); // 10th attack sample reaches exactly 1.0
    CHECK(envelope.getStage() == DahdsrEnvelope::Stage::Hold);
}

TEST_CASE("DahdsrEnvelope: Hold plateaus at 1.0 for exactly its duration before Decay begins", "[DahdsrEnvelope]")
{
    DahdsrEnvelope envelope;
    envelope.setSampleRate(kSampleRate);
    envelope.setParameters({ .delayMs = 0.0f, .attackMs = 1.0f, .holdMs = 5.0f, .decayMs = 10.0f, .sustainLevel = 0.5f, .releaseMs = 20.0f });

    envelope.noteOn();
    envelope.getNextSample(); // consume the single attack sample
    REQUIRE(envelope.getStage() == DahdsrEnvelope::Stage::Hold);

    for (int i = 0; i < 4; ++i)
    {
        CHECK_THAT(envelope.getNextSample(), WithinAbs(1.0f, 1.0e-6f));
        CHECK(envelope.getStage() == DahdsrEnvelope::Stage::Hold);
    }

    CHECK_THAT(envelope.getNextSample(), WithinAbs(1.0f, 1.0e-6f)); // 5th hold sample
    CHECK(envelope.getStage() == DahdsrEnvelope::Stage::Decay);
}

TEST_CASE("DahdsrEnvelope: Sustain holds its level indefinitely until an explicit noteOff, never auto-timing-out", "[DahdsrEnvelope]")
{
    DahdsrEnvelope envelope;
    envelope.setSampleRate(kSampleRate);
    envelope.setParameters({ .delayMs = 0.0f, .attackMs = 1.0f, .holdMs = 0.0f, .decayMs = 1.0f, .sustainLevel = 0.6f, .releaseMs = 20.0f });

    envelope.noteOn();
    envelope.getNextSample(); // attack
    envelope.getNextSample(); // decay
    REQUIRE(envelope.getStage() == DahdsrEnvelope::Stage::Sustain);

    for (int i = 0; i < 100000; ++i)
    {
        CHECK_THAT(envelope.getNextSample(), WithinAbs(0.6f, 1.0e-6f));
        REQUIRE(envelope.isActive());
        REQUIRE(envelope.getStage() == DahdsrEnvelope::Stage::Sustain);
    }
}

TEST_CASE("DahdsrEnvelope: noteOff mid-Attack releases click-free from the current value, not from the peak", "[DahdsrEnvelope]")
{
    DahdsrEnvelope envelope;
    envelope.setSampleRate(kSampleRate);
    envelope.setParameters({ .delayMs = 0.0f, .attackMs = 10.0f, .holdMs = 0.0f, .decayMs = 0.0f, .sustainLevel = 1.0f, .releaseMs = 20.0f });

    envelope.noteOn();
    for (int i = 0; i < 5; ++i)
        envelope.getNextSample();

    REQUIRE(envelope.getStage() == DahdsrEnvelope::Stage::Attack);

    envelope.noteOff();
    REQUIRE(envelope.getStage() == DahdsrEnvelope::Stage::Release);

    // Release ramps linearly from 0.5 (the value attack had reached) down to 0 over 20 samples.
    CHECK_THAT(envelope.getNextSample(), WithinAbs(0.5f - 0.5f / 20.0f, 1.0e-6f));

    for (int i = 0; i < 18; ++i)
        envelope.getNextSample();

    CHECK_THAT(envelope.getNextSample(), WithinAbs(0.0f, 1.0e-6f));
    CHECK(envelope.getStage() == DahdsrEnvelope::Stage::Idle);
    CHECK_FALSE(envelope.isActive());
}

TEST_CASE("DahdsrEnvelope: zero-duration Delay/Hold/Decay are skipped instantly, matching today's exact default shape", "[DahdsrEnvelope]")
{
    DahdsrEnvelope envelope;
    envelope.setSampleRate(kSampleRate);
    // Today's SineSynthVoice constants: delay=0, attack=5ms, hold=0, decay=0, sustain=100%, release=200ms.
    envelope.setParameters({ .delayMs = 0.0f, .attackMs = 5.0f, .holdMs = 0.0f, .decayMs = 0.0f, .sustainLevel = 1.0f, .releaseMs = 200.0f });

    envelope.noteOn();
    REQUIRE(envelope.getStage() == DahdsrEnvelope::Stage::Attack); // Delay(0) already skipped

    for (int i = 1; i <= 4; ++i)
        CHECK_THAT(envelope.getNextSample(), WithinAbs(static_cast<float>(i) / 5.0f, 1.0e-6f));

    CHECK_THAT(envelope.getNextSample(), WithinAbs(1.0f, 1.0e-6f)); // Hold(0) and Decay(0) skipped straight to Sustain
    CHECK(envelope.getStage() == DahdsrEnvelope::Stage::Sustain);
}

TEST_CASE("DahdsrEnvelope: a zero-duration Decay drops the very same sample straight to the sustain level", "[DahdsrEnvelope]")
{
    DahdsrEnvelope envelope;
    envelope.setSampleRate(kSampleRate);
    envelope.setParameters({ .delayMs = 0.0f, .attackMs = 1.0f, .holdMs = 0.0f, .decayMs = 0.0f, .sustainLevel = 0.3f, .releaseMs = 20.0f });

    envelope.noteOn();
    // The single attack sample reaches 1.0, then Hold(0)/Decay(0) chain through in the same call,
    // landing on the sustain level rather than lingering at the peak.
    CHECK_THAT(envelope.getNextSample(), WithinAbs(0.3f, 1.0e-6f));
    CHECK(envelope.getStage() == DahdsrEnvelope::Stage::Sustain);
}

TEST_CASE("DahdsrEnvelope: reset() hard-resets to Idle at value 0 regardless of prior state", "[DahdsrEnvelope]")
{
    DahdsrEnvelope envelope;
    envelope.setSampleRate(kSampleRate);
    envelope.setParameters({ .delayMs = 0.0f, .attackMs = 10.0f, .holdMs = 0.0f, .decayMs = 0.0f, .sustainLevel = 1.0f, .releaseMs = 20.0f });

    envelope.noteOn();
    for (int i = 0; i < 5; ++i)
        envelope.getNextSample();
    REQUIRE(envelope.isActive());

    envelope.reset();

    CHECK_FALSE(envelope.isActive());
    CHECK(envelope.getStage() == DahdsrEnvelope::Stage::Idle);
    CHECK_THAT(envelope.getNextSample(), WithinAbs(0.0f, 1.0e-6f));
}

TEST_CASE("DahdsrEnvelope: noteOff while Idle or already Releasing is a safe no-op", "[DahdsrEnvelope]")
{
    DahdsrEnvelope envelope;
    envelope.setSampleRate(kSampleRate);
    envelope.setParameters({ .delayMs = 0.0f, .attackMs = 1.0f, .holdMs = 0.0f, .decayMs = 0.0f, .sustainLevel = 1.0f, .releaseMs = 20.0f });

    REQUIRE_NOTHROW(envelope.noteOff()); // Idle
    CHECK(envelope.getStage() == DahdsrEnvelope::Stage::Idle);

    envelope.noteOn();
    envelope.getNextSample(); // reach Sustain
    envelope.noteOff();
    REQUIRE(envelope.getStage() == DahdsrEnvelope::Stage::Release);

    const auto valueBeforeSecondNoteOff = envelope.getNextSample(); // 1st release sample: 1.0*(1 - 1/20) = 0.95
    CHECK_THAT(valueBeforeSecondNoteOff, WithinAbs(0.95f, 1.0e-6f));

    envelope.noteOff(); // already releasing - must not restart the release ramp from this new (lower) value
    CHECK(envelope.getStage() == DahdsrEnvelope::Stage::Release);
    // If noteOff() had restarted the ramp, this would release further from 0.95; instead the ramp
    // is still linear from the original stageStartValue (1.0), so the 2nd sample is 1.0*(1-2/20) = 0.9.
    CHECK_THAT(envelope.getNextSample(), WithinAbs(0.9f, 1.0e-6f));
}
