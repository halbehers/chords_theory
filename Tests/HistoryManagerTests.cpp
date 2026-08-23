#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <utility>
#include <vector>

#include "Parameters.h"
#include "PluginProcessor.h"
#include "Theory/HistoryManager.h"
#include "Theory/SessionState.h"
#include "Theory/SessionStateSerializer.h"

using theory::HistoryManager;
using theory::Key;
using theory::SessionState;
using theory::SessionStateSerializer;

namespace
{
    // Comfortably shorter/longer than each other so timing-based assertions have real margin
    // without making the whole suite slow.
    constexpr int kTestDebounceMs = 30;

    // Mirrors exactly what AppLayout::syncStateToValueTree() does - remove+re-append the
    // "ChordsTheoryState" child - so HistoryManager's ValueTree::Listener sees the same shape of
    // mutation a real chord/progression/MIDI-editor edit would produce, without needing a real
    // AppLayout/UI.
    void writeSessionState(juce::AudioProcessorValueTreeState& parameterState, theory::Key key)
    {
        SessionState state;
        state.key = key;

        auto rootState = parameterState.state;
        rootState.removeChild(rootState.getChildWithName(SessionStateSerializer::kStateTag), nullptr);
        rootState.appendChild(SessionStateSerializer::toValueTree(state), nullptr);
    }

    // Repeatedly pumps the dispatch loop in short bursts, checking `predicate` after each, until
    // it's true or maxWaitMs of *real* time elapses - a fixed "wait N ms then check once" (this
    // file's original approach) has to guess a duration that's simultaneously "long enough for a
    // slow/loaded CI runner" and "not annoyingly slow locally"; polling needs no such guess, since
    // it returns the moment the condition is actually satisfied. The deadline is tracked via
    // Time::getMillisecondCounter() rather than accumulating the requested poll step - each
    // runDispatchLoopUntil() call can genuinely take longer than the duration it was asked for
    // (message-queue/thread-sleep granularity is coarser than a few ms on some platforms), so
    // counting nominal/requested time instead of real elapsed time let a very short poll step
    // silently blow past maxWaitMs by 5-10x before this fix.
    template <typename Predicate>
    void waitUntil(Predicate&& predicate, int maxWaitMs = 2000)
    {
        constexpr int kPollStepMs = 25;
        const auto deadline = juce::Time::getMillisecondCounter() + static_cast<juce::uint32>(maxWaitMs);
        while (!predicate() && juce::Time::getMillisecondCounter() < deadline)
            juce::MessageManager::getInstance()->runDispatchLoopUntil(kPollStepMs);
    }
}

TEST_CASE("HistoryManager: fresh construction has nothing to undo or redo", "[HistoryManager]")
{
    PluginAudioProcessor processor;
    HistoryManager history(processor, [] {}, [](bool, bool) {}, kTestDebounceMs);

    CHECK_FALSE(history.canUndo());
    CHECK_FALSE(history.canRedo());
}

TEST_CASE("HistoryManager: a debounced edit becomes undoable", "[HistoryManager]")
{
    PluginAudioProcessor processor;
    HistoryManager history(processor, [] {}, [](bool, bool) {}, kTestDebounceMs);

    writeSessionState(processor.getState(), Key::G);

    CHECK_FALSE(history.canUndo()); // not yet debounced

    waitUntil([&] { return history.canUndo(); });

    CHECK(history.canUndo());
    CHECK_FALSE(history.canRedo());
}

TEST_CASE("HistoryManager: undo restores the prior tree content, fires onStateRestored, and enables redo", "[HistoryManager]")
{
    PluginAudioProcessor processor;

    int restoredCount = 0;
    HistoryManager history(processor, [&restoredCount] { ++restoredCount; }, [](bool, bool) {}, kTestDebounceMs);

    const auto baseline = processor.getState().state.createCopy();

    writeSessionState(processor.getState(), Key::D);
    waitUntil([&] { return history.canUndo(); });
    REQUIRE(history.canUndo());

    history.undo();

    CHECK(restoredCount == 1);
    CHECK(processor.getState().state.isEquivalentTo(baseline));
    CHECK_FALSE(history.canUndo());
    CHECK(history.canRedo());
}

TEST_CASE("HistoryManager: redo restores the edited content again", "[HistoryManager]")
{
    PluginAudioProcessor processor;
    HistoryManager history(processor, [] {}, [](bool, bool) {}, kTestDebounceMs);

    writeSessionState(processor.getState(), Key::D);
    waitUntil([&] { return history.canUndo(); });
    REQUIRE(history.canUndo());

    const auto editedTree = processor.getState().state.createCopy();

    history.undo();
    REQUIRE(history.canRedo());

    history.redo();

    CHECK(processor.getState().state.isEquivalentTo(editedTree));
    CHECK(history.canUndo());
    CHECK_FALSE(history.canRedo());
}

TEST_CASE("HistoryManager: a new edit after undo truncates the old redo branch", "[HistoryManager]")
{
    PluginAudioProcessor processor;

    // canUndo() alone can't tell "edit B's snapshot was pushed" apart from "still true from edit
    // A" once more than one edit is involved - count pushes/restores via onHistoryChanged instead,
    // so each wait below is tied to the specific edit it follows.
    int historyChangedCount = 0;
    HistoryManager history(processor, [] {}, [&historyChangedCount](bool, bool) { ++historyChangedCount; }, kTestDebounceMs);

    writeSessionState(processor.getState(), Key::D); // edit A
    waitUntil([&] { return historyChangedCount >= 1; });
    const auto treeA = processor.getState().state.createCopy();

    writeSessionState(processor.getState(), Key::E); // edit B
    waitUntil([&] { return historyChangedCount >= 2; });

    history.undo(); // back to A

    writeSessionState(processor.getState(), Key::F); // edit C - branches away from B
    waitUntil([&] { return historyChangedCount >= 4; }); // +1 for the undo, +1 for edit C's own push

    CHECK_FALSE(history.canRedo()); // B was discarded, not just hidden

    history.undo(); // should land back on A, not B
    CHECK(processor.getState().state.isEquivalentTo(treeA));
}

TEST_CASE("HistoryManager: rapid-fire edits within one debounce window coalesce into a single entry", "[HistoryManager]")
{
    PluginAudioProcessor processor;

    // A much wider margin than the other tests use (and its own dedicated debounce, decoupled from
    // kTestDebounceMs) - five successive runDispatchLoopUntil() waits accumulate scheduling
    // jitter, and this test's whole premise is that the timer must NOT fire between any two of
    // them, so it needs more headroom than a single settle-and-check test does.
    constexpr int kCoalesceDebounceMs = 300;
    constexpr int kCoalesceStepMs = 20;
    HistoryManager history(processor, [] {}, [](bool, bool) {}, kCoalesceDebounceMs);

    for (auto key : { Key::D, Key::E, Key::F, Key::G, Key::A })
    {
        writeSessionState(processor.getState(), key);
        // Deliberately much shorter than kCoalesceDebounceMs, and a plain fixed wait (not a poll -
        // canUndo() is expected to stay false the whole loop, so polling on it would just degrade
        // to this same fixed wait anyway), so each edit restarts the timer instead of letting it
        // fire - the whole burst should coalesce into one history entry.
        juce::MessageManager::getInstance()->runDispatchLoopUntil(kCoalesceStepMs);
    }

    waitUntil([&] { return history.canUndo(); }, kCoalesceDebounceMs * 3);
    REQUIRE(history.canUndo());

    history.undo();
    CHECK_FALSE(history.canUndo()); // exactly one entry was created, not five
}

TEST_CASE("HistoryManager: undo/redo flush a pending debounced edit instead of losing it", "[HistoryManager]")
{
    PluginAudioProcessor processor;
    HistoryManager history(processor, [] {}, [](bool, bool) {}, kTestDebounceMs);

    writeSessionState(processor.getState(), Key::D); // edit A - let it settle
    waitUntil([&] { return history.canUndo(); });
    REQUIRE(history.canUndo());
    const auto treeA = processor.getState().state.createCopy();

    writeSessionState(processor.getState(), Key::E); // edit B - deliberately NOT waited out
    const auto treeB = processor.getState().state.createCopy();

    history.undo(); // must flush B into its own entry first, then step back to A

    CHECK(processor.getState().state.isEquivalentTo(treeA));
    CHECK(history.canRedo());

    history.redo();
    CHECK(processor.getState().state.isEquivalentTo(treeB));
}

TEST_CASE("HistoryManager: exceeding the depth cap evicts the oldest entry", "[HistoryManager]")
{
    PluginAudioProcessor processor;
    constexpr int kMaxDepth = 3;

    // Same reasoning as the "truncates the old redo branch" test above - canUndo() can't
    // distinguish "this edit's own push happened" from "still true from an earlier one" once
    // there's more than one edit in the sequence.
    int historyChangedCount = 0;
    HistoryManager history(processor, [] {}, [&historyChangedCount](bool, bool) { ++historyChangedCount; }, kTestDebounceMs, kMaxDepth);

    int expectedHistoryChangedCount = 0;
    for (auto key : { Key::D, Key::E, Key::F, Key::G })
    {
        writeSessionState(processor.getState(), key);
        ++expectedHistoryChangedCount;
        waitUntil([&] { return historyChangedCount >= expectedHistoryChangedCount; });
    }

    int undoCount = 0;
    while (history.canUndo())
    {
        history.undo();
        ++undoCount;
    }

    CHECK(undoCount == kMaxDepth - 1); // can only step back within the retained window
}

TEST_CASE("HistoryManager: undo reverts a real synth parameter value", "[HistoryManager]")
{
    PluginAudioProcessor processor;
    HistoryManager history(processor, [] {}, [](bool, bool) {}, kTestDebounceMs);

    auto* releaseParameter = dynamic_cast<juce::RangedAudioParameter*>(processor.getState().getParameter(Parameters::ENVELOPE_RELEASE_MS_ID));
    REQUIRE(releaseParameter != nullptr);

    releaseParameter->setValueNotifyingHost(releaseParameter->convertTo0to1(999.0f)); // default is 200

    // Waits out both AudioProcessorValueTreeState's own internal flush timer (which is what
    // actually writes the new value into the tree HistoryManager watches) and HistoryManager's own
    // debounce - polling on canUndo() means this doesn't need to reason about their combined
    // worst-case duration up front.
    waitUntil([&] { return history.canUndo(); });
    REQUIRE(history.canUndo());

    history.undo();

    const auto* restoredValue = processor.getState().getRawParameterValue(Parameters::ENVELOPE_RELEASE_MS_ID);
    REQUIRE(restoredValue != nullptr);
    CHECK(restoredValue->load() == Catch::Approx(200.0f));
}

TEST_CASE("HistoryManager: onHistoryChanged reports the correct canUndo/canRedo pair at each transition", "[HistoryManager]")
{
    PluginAudioProcessor processor;

    std::vector<std::pair<bool, bool>> transitions;
    HistoryManager history(processor, [] {},
        [&transitions](bool canUndo, bool canRedo) { transitions.emplace_back(canUndo, canRedo); },
        kTestDebounceMs);

    writeSessionState(processor.getState(), Key::D);
    waitUntil([&] { return !transitions.empty(); });

    history.undo();
    history.redo();

    REQUIRE(transitions.size() == 3);
    CHECK(transitions[0] == std::make_pair(true, false));  // after the first edit settled
    CHECK(transitions[1] == std::make_pair(false, true));  // after undo
    CHECK(transitions[2] == std::make_pair(true, false));  // after redo
}
