#pragma once

#include <functional>
#include <vector>

#include <nierika_dsp/nierika_dsp.h>

namespace theory
{

// Undo/redo for the whole plugin state, driven entirely off ndsp::ParameterManager's own root
// ValueTree (parameterManager.getState().state) - every synth DSP parameter lives there as a child
// node, and AppLayout::syncStateToValueTree() splices the chord/progression/MIDI-editor state
// ("ChordsTheoryState") into the SAME tree. A single juce::ValueTree::Listener on that one root
// therefore observes every edit from either surface with no per-component instrumentation.
//
// Edits are coalesced with a short debounce timer (rather than one history entry per intermediate
// drag value) so a whole knob-drag or note-drag gesture becomes one undo step. undo()/redo() apply
// a stored snapshot via AudioProcessorValueTreeState::replaceState(), which JUCE cascades through
// every attached SliderAttachment/ButtonAttachment/ParameterAttachment automatically (including
// ParameterManager's own audio-thread atomic resync) - callers only need to separately re-derive
// whatever isn't parameter-backed (see onStateRestored below).
//
// juce::ValueTree copies/assigns by reference, not by value - every snapshot this class stores, and
// every tree handed to replaceState(), must go through ValueTree::createCopy() or a later live edit
// would silently mutate an already-stored "past" entry.
class HistoryManager : private juce::ValueTree::Listener, private juce::Timer
{
public:
    // debounceMs/maxDepth are constructor parameters (not fixed constants) purely so tests can use
    // a much smaller debounce/depth than the real UI does.
    HistoryManager(ndsp::ParameterManager& parameterManager,
                   std::function<void()> onStateRestored,
                   std::function<void(bool canUndo, bool canRedo)> onHistoryChanged,
                   int debounceMs = 500, int maxDepth = 100);
    ~HistoryManager() override;

    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;

    // Flushes any pending (not-yet-debounced) edit into its own snapshot first, so an in-progress
    // edit is never silently lost - then navigates.
    void undo();
    void redo();

private:
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override { scheduleSnapshot(); }
    void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&) override { scheduleSnapshot(); }
    void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, int) override { scheduleSnapshot(); }
    void valueTreeChildOrderChanged(juce::ValueTree&, int, int) override { scheduleSnapshot(); }
    void valueTreeParentChanged(juce::ValueTree&) override {}
    // Fires from our OWN applySnapshot()'s replaceState() call too - scheduleSnapshot() below is a
    // no-op while _isRestoring, so restoring never schedules a spurious snapshot of itself.
    void valueTreeRedirected(juce::ValueTree&) override { scheduleSnapshot(); }

    void timerCallback() override;
    void scheduleSnapshot();
    void pushSnapshot();
    void applySnapshot(int index);

    void notifyHistoryChanged() const;

    ndsp::ParameterManager& _parameterManager;
    std::function<void()> _onStateRestored;
    std::function<void(bool, bool)> _onHistoryChanged;
    int _debounceMs;
    int _maxDepth;

    // _snapshots[_currentIndex] is always the live state's own last-known snapshot. Undo/redo just
    // move _currentIndex and reapply; a new edit after undoing truncates everything past it (the
    // discarded "redo" branch), standard undo-stack semantics.
    std::vector<juce::ValueTree> _snapshots;
    int _currentIndex = -1;
    bool _isRestoring = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HistoryManager)
};

}
