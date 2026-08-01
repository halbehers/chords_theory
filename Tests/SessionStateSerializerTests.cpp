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
    state.progressionSlots = {
        { 0, Degree::I },
        { 5, Degree::IV }, // deliberately non-contiguous, to confirm exact slot indices survive
        { 11, Degree::V },
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
    state.progressionSlots = { { 0, Degree::I }, { 1, Degree::I }, { 2, Degree::IV } };

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

    // Falls back to the default Key/Scale rather than throwing, and the malformed degree entry is
    // simply absent from the result.
    CHECK(restored.key == Key::C);
    CHECK(restored.scale == Scale::Major);
    CHECK(restored.degreeVoicings.empty());
}
