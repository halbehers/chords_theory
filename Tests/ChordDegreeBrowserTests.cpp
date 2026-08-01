#include <catch2/catch_test_macros.hpp>

#include <utility>
#include <vector>

#include "Component/ChordDegreeBrowser.h"
#include "Theory/ChordDatabase.h"
#include "Theory/ProgressionSlot.h"

using component::ChordDegreeBrowser;
using theory::ChordDatabase;
using theory::Degree;
using theory::Key;
using theory::ProgressionSlot;
using theory::Scale;

TEST_CASE("ChordDegreeBrowser::setKeyAndScale survives repeated key/scale changes, including 7<->3 degree-count transitions", "[ChordDegreeBrowser]")
{
    // Regression test for a real crash: nierika_dsp's GridLayout::reset() cleared row/column
    // sizing but never cleared its internal _itemsById/_componentsById maps. setKeyAndScale()
    // destroys the previous key/scale's ChordCards and rebuilds the layout from scratch (reset()
    // + re-populate) every time it's called - with that bug, the still-populated maps kept
    // referencing already-destroyed ChordCards (dangling juce::Component&), and reused identifiers
    // (the same "chord-card-I".."chord-card-VII" every time) meant unordered_map::emplace() silently
    // no-op'd instead of registering the new cards. The next resized() call (invoked at the end of
    // setKeyAndScale itself) then touched those dangling references via GridLayout::replaceAll(),
    // which iterates every entry unconditionally - producing exactly what was reported: wrong
    // degree counts, and outright crashes, especially when switching to/from Minor Blues (3
    // degrees: I/IV/V only) which is the sharpest degree-count transition available.
    ChordDegreeBrowser browser("test-browser");
    browser.setSize(960, 200);

    const std::vector<std::pair<Key, Scale>> transitions = {
        { Key::C, Scale::Major },
        { Key::G, Scale::MinorBlues },
        { Key::D, Scale::Dorian },
        { Key::Bb, Scale::MinorBlues },
        { Key::F, Scale::HarmonicMinor },
        { Key::Db, Scale::MinorBlues },
        { Key::C, Scale::Major },
        { Key::A, Scale::Locrian },
    };

    for (const auto& [key, scale] : transitions)
    {
        CAPTURE(static_cast<int>(key), static_cast<int>(scale));

        // setKeyAndScale() calls resized() internally at the end, which is exactly the call that
        // used to crash - no separate resized()/setSize() call needed to reproduce this.
        REQUIRE_NOTHROW(browser.setKeyAndScale(key, scale));

        const auto& keyScaleData = ChordDatabase::getInstance().get(key, scale);

        // Every degree that exists for this key/scale must have a resolvable current chord...
        for (const auto& degreeData : keyScaleData.degrees)
            CHECK(browser.getCurrentChord(degreeData.degree) != nullptr);

        // ...and every degree that does NOT exist for this key/scale must not (this is what
        // "sometimes more, sometimes less" degrees looked like in practice - stale entries from a
        // previous, different-shaped key/scale bleeding into the current one).
        if (scale == Scale::MinorBlues)
        {
            CHECK(browser.getCurrentChord(Degree::II) == nullptr);
            CHECK(browser.getCurrentChord(Degree::III) == nullptr);
            CHECK(browser.getCurrentChord(Degree::VI) == nullptr);
            CHECK(browser.getCurrentChord(Degree::VII) == nullptr);
        }
    }
}

TEST_CASE("ChordDegreeBrowser::resolveSlot resolves a pinned popularityOrder when it exists under the current key/scale", "[ChordDegreeBrowser]")
{
    ChordDegreeBrowser browser("test-browser");
    browser.setKeyAndScale(Key::C, Scale::Major);

    const auto& degreeIData = ChordDatabase::getInstance().get(Key::C, Scale::Major).degrees.front();
    REQUIRE(degreeIData.chords.size() > 1);
    const auto& pinnedChord = degreeIData.chords[1]; // deliberately not the default (most popular) voicing

    const ProgressionSlot slot { Degree::I, pinnedChord.popularityOrder };
    const auto* resolved = browser.resolveSlot(slot);

    REQUIRE(resolved != nullptr);
    CHECK(resolved->symbol == pinnedChord.symbol);

    // Confirm this is a genuine pin, not a coincidental match with the live default.
    const auto* live = browser.getCurrentChord(Degree::I);
    REQUIRE(live != nullptr);
    CHECK(resolved->symbol != live->symbol);
}

TEST_CASE("ChordDegreeBrowser::resolveSlot pins survive a key change - the actual bug this feature exists to fix", "[ChordDegreeBrowser]")
{
    // A Chord::symbol is an absolute-pitch string ("Cmaj7" in C Major, "Dmaj7" in D Major) - never
    // shared between keys - so pinning by symbol would always fail to resolve after a key change.
    // popularityOrder is the key-independent identifier instead: this test is the direct
    // regression check for that, reproducing the user's exact repro steps (pin a voicing, change
    // key, confirm the SAME relative voicing - not the default - is still what resolves).
    ChordDegreeBrowser browser("test-browser");
    browser.setKeyAndScale(Key::C, Scale::Major);

    const auto& cDegreeIData = ChordDatabase::getInstance().get(Key::C, Scale::Major).degrees.front();
    REQUIRE(cDegreeIData.chords.size() > 1);
    const auto& pinnedInC = cDegreeIData.chords[1]; // e.g. "Cmaj7"

    const ProgressionSlot slot { Degree::I, pinnedInC.popularityOrder };
    REQUIRE(browser.resolveSlot(slot) != nullptr);
    CHECK(browser.resolveSlot(slot)->symbol == pinnedInC.symbol);

    browser.setKeyAndScale(Key::D, Scale::Major);

    const auto& dDegreeIData = ChordDatabase::getInstance().get(Key::D, Scale::Major).degrees.front();
    const auto& expectedInD = dDegreeIData.chords[static_cast<std::size_t>(pinnedInC.popularityOrder - 1)]; // e.g. "Dmaj7" - same relative voicing, different key

    const auto* resolvedAfterKeyChange = browser.resolveSlot(slot); // same slot object, never re-pinned
    REQUIRE(resolvedAfterKeyChange != nullptr);
    CHECK(resolvedAfterKeyChange->symbol == expectedInD.symbol);

    // And definitely not silently fallen back to D Major's plain default triad.
    CHECK(resolvedAfterKeyChange->symbol != dDegreeIData.chords.front().symbol);
}

TEST_CASE("ChordDegreeBrowser::resolveSlot falls back to the live per-degree voicing when the pinned popularityOrder doesn't exist", "[ChordDegreeBrowser]")
{
    ChordDegreeBrowser browser("test-browser");
    browser.setKeyAndScale(Key::C, Scale::Major);

    const ProgressionSlot slot { Degree::I, 9999 }; // out of range for any degree's voicing list
    const auto* resolved = browser.resolveSlot(slot);
    const auto* live = browser.getCurrentChord(Degree::I);

    REQUIRE(resolved != nullptr);
    REQUIRE(live != nullptr);
    CHECK(resolved->symbol == live->symbol);
}

TEST_CASE("ChordDegreeBrowser::resolveSlot with an unpinned slot tracks live per-degree changes", "[ChordDegreeBrowser]")
{
    ChordDegreeBrowser browser("test-browser");
    browser.setKeyAndScale(Key::C, Scale::Major);

    const auto& degreeIData = ChordDatabase::getInstance().get(Key::C, Scale::Major).degrees.front();
    REQUIRE(degreeIData.chords.size() > 1);
    const auto& newChord = degreeIData.chords[1];

    browser.selectVoicing(Degree::I, newChord);

    const ProgressionSlot unpinnedSlot { Degree::I, 0 };
    const auto* resolved = browser.resolveSlot(unpinnedSlot);

    REQUIRE(resolved != nullptr);
    CHECK(resolved->symbol == newChord.symbol);
}

TEST_CASE("ChordDegreeBrowser::resolveSlot returns nullptr for a degree absent from the current scale", "[ChordDegreeBrowser]")
{
    ChordDegreeBrowser browser("test-browser");
    browser.setKeyAndScale(Key::G, Scale::MinorBlues);

    const ProgressionSlot slot { Degree::II, 0 }; // Minor Blues only has I/IV/V
    CHECK(browser.resolveSlot(slot) == nullptr);
}
