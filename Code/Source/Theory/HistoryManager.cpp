#include "Theory/HistoryManager.h"

namespace theory
{

HistoryManager::HistoryManager(ndsp::ParameterManager& parameterManager, std::function<void()> onStateRestored,
                                std::function<void(bool, bool)> onHistoryChanged, int debounceMs, int maxDepth):
    _parameterManager(parameterManager),
    _onStateRestored(std::move(onStateRestored)),
    _onHistoryChanged(std::move(onHistoryChanged)),
    _debounceMs(debounceMs),
    _maxDepth(maxDepth)
{
    // Baseline - whatever the tree looks like right now (DAW-loaded project, or the caller's own
    // just-applied defaults) becomes the first entry, so undo can always return to "state as of
    // construction".
    _snapshots.push_back(_parameterManager.getState().state.createCopy());
    _currentIndex = 0;

    _parameterManager.getState().state.addListener(this);
}

HistoryManager::~HistoryManager()
{
    _parameterManager.getState().state.removeListener(this);
}

bool HistoryManager::canUndo() const
{
    return _currentIndex > 0;
}

bool HistoryManager::canRedo() const
{
    return _currentIndex + 1 < static_cast<int>(_snapshots.size());
}

void HistoryManager::undo()
{
    if (isTimerRunning())
    {
        stopTimer();
        pushSnapshot();
    }

    if (!canUndo())
        return;

    applySnapshot(_currentIndex - 1);
}

void HistoryManager::redo()
{
    if (isTimerRunning())
    {
        stopTimer();
        pushSnapshot();
    }

    if (!canRedo())
        return;

    applySnapshot(_currentIndex + 1);
}

void HistoryManager::timerCallback()
{
    stopTimer();
    pushSnapshot();
}

void HistoryManager::scheduleSnapshot()
{
    if (_isRestoring)
        return;

    startTimer(_debounceMs); // restarts the countdown if already running - coalesces a whole edit burst
}

void HistoryManager::pushSnapshot()
{
    // A new edit branches history - discard whatever "redo" tail existed past the current position.
    if (_currentIndex + 1 < static_cast<int>(_snapshots.size()))
        _snapshots.erase(_snapshots.begin() + _currentIndex + 1, _snapshots.end());

    _snapshots.push_back(_parameterManager.getState().state.createCopy());
    ++_currentIndex;

    if (static_cast<int>(_snapshots.size()) > _maxDepth)
    {
        _snapshots.erase(_snapshots.begin());
        --_currentIndex;
    }

    notifyHistoryChanged();
}

void HistoryManager::applySnapshot(int index)
{
    _isRestoring = true;

    _parameterManager.getState().replaceState(_snapshots[static_cast<std::size_t>(index)].createCopy());
    _currentIndex = index;

    if (_onStateRestored)
        _onStateRestored();

    _isRestoring = false;

    notifyHistoryChanged();
}

void HistoryManager::notifyHistoryChanged() const
{
    if (_onHistoryChanged)
        _onHistoryChanged(canUndo(), canRedo());
}

}
