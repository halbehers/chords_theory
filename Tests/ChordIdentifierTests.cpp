#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "Theory/ChordIdentifier.h"

using theory::ChordIdentifier;
using theory::Degree;
using theory::Key;
using theory::ProgressionSlot;
using theory::Scale;

TEST_CASE("ChordIdentifier::identify matches a root-position triad", "[ChordIdentifier]")
{
    const auto detected = ChordIdentifier::identify({ 60, 64, 67 }, Key::C, Scale::Major); // C4,E4,G4

    REQUIRE(detected.has_value());
    CHECK(detected->label == "C");
    CHECK(detected->slot == ProgressionSlot { Degree::I, 1 });
}

TEST_CASE("ChordIdentifier::identify matches an inversion via its bass-first content", "[ChordIdentifier]")
{
    const auto detected = ChordIdentifier::identify({ 64, 67, 72 }, Key::C, Scale::Major); // E4,G4,C5 - same
                                                                                             // pitch-class set
                                                                                             // as a root-
                                                                                             // position C
                                                                                             // triad, but a
                                                                                             // different bass

    REQUIRE(detected.has_value());
    CHECK(detected->label == "C/E");
    CHECK(detected->slot == ProgressionSlot { Degree::I, 11 });
}

TEST_CASE("ChordIdentifier::identify matches a 2-note power chord", "[ChordIdentifier]")
{
    const auto detected = ChordIdentifier::identify({ 60, 67 }, Key::C, Scale::Major); // C4,G4

    REQUIRE(detected.has_value());
    CHECK(detected->label == "C5");
    CHECK(detected->slot == ProgressionSlot { Degree::I, 8 });
}

TEST_CASE("ChordIdentifier::identify returns nullopt for a non-diatonic cluster", "[ChordIdentifier]")
{
    // C4,C#4,D4 - a tight semitone cluster, cataloged nowhere in C Major.
    CHECK_FALSE(ChordIdentifier::identify({ 60, 61, 62 }, Key::C, Scale::Major).has_value());
}

TEST_CASE("ChordIdentifier::identify returns nullopt for an empty input", "[ChordIdentifier]")
{
    CHECK_FALSE(ChordIdentifier::identify({}, Key::C, Scale::Major).has_value());
}

TEST_CASE("ChordIdentifier::identify does not fall back to a different scale", "[ChordIdentifier]")
{
    // D,F,Ab - a D diminished triad. Cataloged as C Harmonic Minor's degree II, but not diatonic to
    // C Major at all (confirmed absent from C Major's full chord list) - proves there's no
    // cross-scale fallback search.
    const std::vector<int> dDiminished { 62, 65, 68 };

    const auto inHarmonicMinor = ChordIdentifier::identify(dDiminished, Key::C, Scale::HarmonicMinor);
    REQUIRE(inHarmonicMinor.has_value());
    CHECK(inHarmonicMinor->label == "Ddim");
    CHECK(inHarmonicMinor->slot == ProgressionSlot { Degree::II, 1 });

    CHECK_FALSE(ChordIdentifier::identify(dDiminished, Key::C, Scale::Major).has_value());
}
