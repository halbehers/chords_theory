#include <catch2/catch_test_macros.hpp>

#include "Theory/SessionStateSerializer.h"

using theory::Degree;
using theory::Key;
using theory::Scale;
using theory::SessionState;
using theory::SessionStateSerializer;

TEST_CASE("SessionStateSerializer round-trips a fully populated session through a ValueTree", "[SessionStateSerializer]")
{
    SessionState state;
    state.key = Key::Db;
    state.scale = Scale::HarmonicMinor;
    state.degreeVoicings = {
        { Degree::I, "Dbm" },
        { Degree::IV, "Gb7" },
        { Degree::V, "Ab7" },
    };
    state.progressionEditorState.notes = {
        { 60, 0.0, 4.0 },
        { 64, 0.0, 4.0 },
        { 67, 4.0, 2.5 }, // deliberately off the bar grid, to confirm exact doubles survive
    };

    const auto tree = SessionStateSerializer::toValueTree(state);
    const auto restored = SessionStateSerializer::fromValueTree(tree);

    CHECK(restored == state);
}

TEST_CASE("SessionStateSerializer round-trips through binary ValueTree serialization (the actual getStateInformation path)", "[SessionStateSerializer]")
{
    SessionState state;
    state.key = Key::G;
    state.scale = Scale::MinorBlues;
    state.degreeVoicings = { { Degree::I, "G7" }, { Degree::IV, "C7" }, { Degree::V, "D7" } };
    state.progressionEditorState.notes = { { 55, 0.0, 4.0 } };

    const auto tree = SessionStateSerializer::toValueTree(state);

    juce::MemoryBlock block;
    {
        juce::MemoryOutputStream stream(block, false);
        tree.writeToStream(stream);
    }

    const auto reloadedTree = juce::ValueTree::readFromData(block.getData(), block.getSize());
    REQUIRE(reloadedTree.isValid());

    const auto restored = SessionStateSerializer::fromValueTree(reloadedTree);
    CHECK(restored == state);
}

TEST_CASE("SessionStateSerializer::fromValueTree returns defaults for an invalid or wrong-type tree", "[SessionStateSerializer]")
{
    const auto fromInvalid = SessionStateSerializer::fromValueTree(juce::ValueTree());
    CHECK(fromInvalid == SessionState {});

    const auto fromWrongType = SessionStateSerializer::fromValueTree(juce::ValueTree("SomeOtherTree"));
    CHECK(fromWrongType == SessionState {});
}

TEST_CASE("SessionStateSerializer skips malformed degree/key/scale entries instead of failing the whole parse", "[SessionStateSerializer]")
{
    auto tree = SessionStateSerializer::toValueTree(SessionState {});
    tree.setProperty("key", "NotARealKey", nullptr);
    tree.setProperty("scale", "NotARealScale", nullptr);

    juce::ValueTree badDegree("Degree");
    badDegree.setProperty("degree", "XIV", nullptr);
    badDegree.setProperty("chordSymbol", "Whatever", nullptr);
    tree.getChildWithName("DegreeVoicings").appendChild(badDegree, nullptr);

    const auto restored = SessionStateSerializer::fromValueTree(tree);

    // Falls back to the default Key/Scale rather than throwing, and the malformed entry is simply
    // absent from the result (not aborting the whole parse).
    CHECK(restored.key == Key::C);
    CHECK(restored.scale == Scale::Major);
    CHECK(restored.degreeVoicings.empty());
}
