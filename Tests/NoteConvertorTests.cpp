#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

#include "Theory/NoteConvertor.h"

using theory::NoteConvertor;

TEST_CASE("NoteConvertor::parsePitchClass resolves naturals, sharps, flats, and double accidentals", "[NoteConvertor]")
{
    CHECK(NoteConvertor::parsePitchClass("C") == 0);
    CHECK(NoteConvertor::parsePitchClass("D") == 2);
    CHECK(NoteConvertor::parsePitchClass("E") == 4);
    CHECK(NoteConvertor::parsePitchClass("F") == 5);
    CHECK(NoteConvertor::parsePitchClass("G") == 7);
    CHECK(NoteConvertor::parsePitchClass("A") == 9);
    CHECK(NoteConvertor::parsePitchClass("B") == 11);

    CHECK(NoteConvertor::parsePitchClass("C#") == 1);
    CHECK(NoteConvertor::parsePitchClass("Db") == 1);
    CHECK(NoteConvertor::parsePitchClass("F#") == 6);
    CHECK(NoteConvertor::parsePitchClass("Bb") == 10);

    // Double accidentals, confirmed present in the real chords.json data.
    CHECK(NoteConvertor::parsePitchClass("C##") == 2);
    CHECK(NoteConvertor::parsePitchClass("Ebb") == 2);
    CHECK(NoteConvertor::parsePitchClass("Fb") == 4);
    CHECK(NoteConvertor::parsePitchClass("Cb") == 11);

    // Wrap-around edge cases.
    CHECK(NoteConvertor::parsePitchClass("B#") == 0);
    CHECK(NoteConvertor::parsePitchClass("Cbb") == 10);
}

TEST_CASE("NoteConvertor::voiceChordCloseToMiddleC produces a strictly ascending closed voicing anchored near middle C", "[NoteConvertor]")
{
    SECTION("single note roots exactly at or below middle C")
    {
        const auto result = NoteConvertor::voiceChordCloseToMiddleC(std::vector<int> { 0 }); // C
        REQUIRE(result.size() == 1);
        CHECK(result.front() == 60);
    }

    SECTION("C major triad closes within an octave at middle C")
    {
        const auto result = NoteConvertor::voiceChordCloseToMiddleC({ 0, 4, 7 }); // C, E, G
        REQUIRE(result.size() == 3);
        CHECK(result == std::vector<int>{ 60, 64, 67 });
    }

    SECTION("worked example from the plan: C7#9 (Minor Blues degree I)")
    {
        // C(1), E(3), G(5), Bb(7), D#(9) -> pitch classes 0,4,7,10,3
        const auto result = NoteConvertor::voiceChordCloseToMiddleC({ 0, 4, 7, 10, 3 });
        CHECK(result == std::vector<int>{ 60, 64, 67, 70, 75 });
    }

    SECTION("inversion: bass note (first array entry) anchors near middle C, not the harmonic root")
    {
        // C/E: E(3), G(5), C(1) -> pitch classes 4, 7, 0
        const auto result = NoteConvertor::voiceChordCloseToMiddleC({ 4, 7, 0 });
        REQUIRE(result.size() == 3);
        CHECK(result.front() == 52); // E3, the highest E at or below middle C
        CHECK(std::is_sorted(result.begin(), result.end()));
    }

    SECTION("output is always strictly ascending and rooted within an octave below middle C")
    {
        for (const auto& pitchClasses : std::vector<std::vector<int>>{
                 { 0, 4, 7, 11 },       // Cmaj7
                 { 2, 5, 9, 0 },        // Dm7 (assorted order)
                 { 7, 11, 2, 5, 9 },    // G9
             })
        {
            const auto result = NoteConvertor::voiceChordCloseToMiddleC(pitchClasses);
            REQUIRE(result.size() == pitchClasses.size());
            CHECK(std::is_sorted(result.begin(), result.end()));
            CHECK(result.front() > NoteConvertor::kMiddleC - 12);
            CHECK(result.front() <= NoteConvertor::kMiddleC);

            for (std::size_t i = 0; i < result.size(); ++i)
                CHECK(result[i] % 12 == ((pitchClasses[i] % 12) + 12) % 12);
        }
    }
}
