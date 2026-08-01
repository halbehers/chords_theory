#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <set>

#include "Theory/ChordDatabase.h"

using theory::ChordDatabase;
using theory::Degree;
using theory::Key;
using theory::kNumDegrees;
using theory::kNumKeys;
using theory::kNumScales;
using theory::Scale;

TEST_CASE("ChordDatabase parses without throwing and is reachable via the singleton", "[ChordDatabase]")
{
    REQUIRE_NOTHROW(ChordDatabase::getInstance());
}

TEST_CASE("ChordDatabase: C Major degree I matches the verified popularity-ordered chord list", "[ChordDatabase]")
{
    const auto& data = ChordDatabase::getInstance().get(Key::C, Scale::Major);
    const auto* degreeI = data.findDegree(Degree::I);

    REQUIRE(degreeI != nullptr);

    const std::vector<std::string> expectedSymbolsInOrder = {
        "C", "Cmaj7", "Cmaj9", "Csus4", "Csus2", "Cadd9", "C6", "C5", "Cmaj11", "Cmaj13", "C/E", "C/G"
    };

    REQUIRE(degreeI->chords.size() == expectedSymbolsInOrder.size());

    for (std::size_t i = 0; i < expectedSymbolsInOrder.size(); ++i)
    {
        CHECK(degreeI->chords[i].symbol == expectedSymbolsInOrder[i]);
        CHECK(degreeI->chords[i].popularityOrder == static_cast<int>(i) + 1);
    }

    // Default voicing (most popular) is always index 0.
    CHECK(degreeI->chords.front().symbol == "C");
}

TEST_CASE("ChordDatabase: every Key/Scale combination is populated with the expected degree shape", "[ChordDatabase]")
{
    const auto& database = ChordDatabase::getInstance();

    for (int keyIndex = 0; keyIndex < kNumKeys; ++keyIndex)
    {
        for (int scaleIndex = 0; scaleIndex < kNumScales; ++scaleIndex)
        {
            const auto key = static_cast<Key>(keyIndex);
            const auto scale = static_cast<Scale>(scaleIndex);
            const auto& data = database.get(key, scale);

            CAPTURE(keyIndex, scaleIndex);

            if (scale == Scale::MinorBlues)
            {
                REQUIRE(data.degrees.size() == 3);
                CHECK(data.findDegree(Degree::I) != nullptr);
                CHECK(data.findDegree(Degree::IV) != nullptr);
                CHECK(data.findDegree(Degree::V) != nullptr);
                CHECK(data.findDegree(Degree::II) == nullptr);
                CHECK(data.findDegree(Degree::III) == nullptr);
                CHECK(data.findDegree(Degree::VI) == nullptr);
                CHECK(data.findDegree(Degree::VII) == nullptr);
                CHECK(data.scaleNotes.size() == 6);
            }
            else
            {
                REQUIRE(data.degrees.size() == static_cast<std::size_t>(kNumDegrees));
                CHECK(data.scaleNotes.size() == 7);
            }

            // Every degree's chords must be sorted ascending by popularityOrder starting at 1,
            // with no gaps.
            for (const auto& degreeData : data.degrees)
            {
                REQUIRE_FALSE(degreeData.chords.empty());

                for (std::size_t i = 0; i < degreeData.chords.size(); ++i)
                    CHECK(degreeData.chords[i].popularityOrder == static_cast<int>(i) + 1);

                CHECK(std::is_sorted(degreeData.chords.begin(), degreeData.chords.end(),
                    [](const theory::Chord& a, const theory::Chord& b) { return a.popularityOrder < b.popularityOrder; }));
            }
        }
    }
}

TEST_CASE("ChordDatabase: chord tone note names parse to valid pitch classes", "[ChordDatabase]")
{
    const auto& data = ChordDatabase::getInstance().get(Key::Db, Scale::MinorBlues);
    const auto* degreeI = data.findDegree(Degree::I);

    REQUIRE(degreeI != nullptr);
    REQUIRE_FALSE(degreeI->chords.empty());

    for (const auto& chord : degreeI->chords)
    {
        for (const auto& note : chord.notes)
        {
            const int pitchClass = note.getPitchClass();
            CHECK(pitchClass >= 0);
            CHECK(pitchClass <= 11);
        }
    }
}
