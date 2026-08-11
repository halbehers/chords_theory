#include "Component/MidiEditor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "Theory/ChordIdentifier.h"
#include "Theory/NoteConvertor.h"

namespace component
{

namespace
{
    constexpr int kMinMidiNote = 24;  // C1
    constexpr int kMaxMidiNote = 108; // C8 - voiceChordCloseToMiddleC always lands ~49-84,
                                       // comfortably inside this with headroom for manual edits
    constexpr int kNumPitchRows = kMaxMidiNote - kMinMidiNote + 1;
    constexpr int kInitialTopMidiNote = 67; // G4, matches the mockup's initial visible top row

    constexpr float kGutterWidth = 40.f;
    constexpr float kRulerHeight = 24.f;
    constexpr float kChordLaneHeight = 28.f;
    constexpr float kScrollbarThickness = 8.f;

    constexpr double kDefaultContentBars = 32.0;
    constexpr double kDefaultNoteLengthBeats = 1.0; // chords pack ~1 beat apart in the mockup, not
                                                     // one-bar-per-chord like MidiExporter's export
    constexpr double kSnapBeats = 0.25; // sixteenth notes at 4 beats/bar
    constexpr double kChordOnsetToleranceBeats = 0.5; // see recomputeChordBlocksFromNotes
    constexpr float kResizeHandleWidth = 7.f; // fixed screen pixels regardless of zoom
    constexpr float kClickVsDragThreshold = 3.f; // fixed screen pixels - below this, a gesture that
                                                   // grabbed a note/started a marquee is treated as
                                                   // a plain click instead (see mouseUp)

    constexpr float kRulerTickHeight = 16.f; // bottom-aligned tick mark height within the ruler row
    constexpr float kRulerTickLabelGap = 8.f; // horizontal gap between a tick and its beat/bar label

    constexpr float kMinPixelsPerBeat = 20.f, kMaxPixelsPerBeat = 400.f, kDefaultPixelsPerBeat = 80.f;
    constexpr float kMinRowHeight = 8.f, kMaxRowHeight = 40.f, kDefaultRowHeight = 16.f;
    constexpr float kZoomStepMultiplier = 1.08f; // multiplicative, not additive - additive steps
                                                  // feel wildly different at low vs. high zoom
    constexpr double kWheelScrollBeats = 2.0; // per full wheel notch
    constexpr float kWheelScrollRows = 3.0f;

    constexpr std::array<bool, 12> kIsBlackKey { false,true,false,true,false,false,true,false,true,false,true,false };
    const std::array<juce::String, 12> kNoteNames { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

    // Tries every subset of `candidates`, smallest first, appended to `ownNotes`, returning the
    // first combination that theory::ChordIdentifier matches (or std::nullopt if none do) - see
    // MidiEditor::recomputeChordBlocksFromNotes for why "smallest first" matters: a still-ringing
    // note should only get pulled into a later chord's identity if it's actually needed to complete
    // a match, not just because it happens to still be sounding. Enumerates subsets via the standard
    // "permute a boolean picker" idiom (std::prev_permutation over a sorted-descending bool vector).
    // 2^candidates.size() combinations worst case - trivial for the handful of overlapping notes a
    // real piano-roll boundary can realistically have.
    std::optional<theory::DetectedChord> identifyWithMinimalBorrowing(const std::vector<int>& ownNotes,
        const std::vector<int>& candidates, theory::Key key, theory::Scale scale)
    {
        if (const auto detected = theory::ChordIdentifier::identify(ownNotes, key, scale))
            return detected;

        for (std::size_t size = 1; size <= candidates.size(); ++size)
        {
            std::vector<bool> picker(candidates.size(), false);
            std::fill(picker.begin(), picker.begin() + static_cast<std::ptrdiff_t>(size), true);

            do
            {
                auto attempt = ownNotes;
                for (std::size_t i = 0; i < candidates.size(); ++i)
                    if (picker[i])
                        attempt.push_back(candidates[i]);

                if (const auto detected = theory::ChordIdentifier::identify(attempt, key, scale))
                    return detected;
            }
            while (std::prev_permutation(picker.begin(), picker.end()));
        }

        return std::nullopt;
    }
}

MidiEditor::MidiEditor(const std::string& identifier, audio::ProgressionPlayer* progressionPlayer):
    Component(identifier),
    _progressionPlayer(progressionPlayer)
{
    displayBackground(nui::Theme::ThemeColor::BACKGROUND, nui::Theme::getBorderRadius());
    displayBorder(nui::Theme::ThemeColor::BORDER, 1.f, nui::Theme::getBorderRadius(), 1.f);

    _scrollRow = static_cast<float>(kMaxMidiNote - kInitialTopMidiNote);
    _pixelsPerBeat = kDefaultPixelsPerBeat;
    _rowHeight = kDefaultRowHeight;

    addAndMakeVisible(_hScrollBar);
    addAndMakeVisible(_vScrollBar);
    _hScrollBar.addListener(this);
    _vScrollBar.addListener(this);

    // Matches VoicingSelector's own scrollbar exactly: same thickness/colour, and the same
    // auto-hide-until-hovering-and-actually-needed behaviour (see mouseEnter/mouseExit,
    // updateScrollBarVisibility).
    _hScrollBar.setColour(juce::ScrollBar::thumbColourId, nui::Theme::newColor(nui::Theme::ThemeColor::BACKGROUND).asJuce().withAlpha(0.5f));
    _vScrollBar.setColour(juce::ScrollBar::thumbColourId, nui::Theme::newColor(nui::Theme::ThemeColor::BACKGROUND).asJuce().withAlpha(0.5f));

    // See ChildBoundaryListener's own comment (MidiEditor.h) for why this registers a dedicated
    // forwarding object instead of `this` directly.
    addMouseListener(&_childBoundaryListener, true);

    // Needed for keyPressed() (Delete/arrow-key nudge of the current selection) to ever fire -
    // mouseDown grabs focus on every click so keyboard input reliably routes here afterward.
    setWantsKeyboardFocus(true);
}

MidiEditor::~MidiEditor()
{
    // Playback lives on ChordSynthEngine (owned by the audio processor, outliving this editor
    // Component) - without this, closing the editor while a progression is playing would leave it
    // looping forever with no way left to stop it.
    if (_progressionPlayer != nullptr)
        _progressionPlayer->stop();

    _hScrollBar.removeListener(this);
    _vScrollBar.removeListener(this);
}

void MidiEditor::paint(juce::Graphics& g)
{
    Component::paint(g);

    {
        juce::Graphics::ScopedSaveState saved(g);
        g.reduceClipRegion(_contentArea.toNearestInt());
        paintGridlines(g);
        paintNotes(g);
        paintMarqueeSelection(g);
    }

    {
        // Extends all the way to every remaining edge - left (under the gutter) and right/bottom
        // (under the scrollbars' own reserved space) - since the chord lane isn't relative to
        // either the gutter or the scrollbars; there's nothing else claiming that space, and its
        // bottom corners need to reach the widget's own true bottom-left/right corners for their
        // rounding (see paintChordLane) to land exactly on the widget's own outer silhouette.
        const juce::Rectangle<float> chordLaneBounds(0.f, _contentArea.getBottom(),
            static_cast<float>(getWidth()), static_cast<float>(getHeight()) - _contentArea.getBottom());
        juce::Graphics::ScopedSaveState saved(g);
        g.reduceClipRegion(chordLaneBounds.toNearestInt());
        paintChordLane(g);
    }

    paintRuler(g);
    paintGutter(g);

    // Both drawn last, fully on top (low-alpha fills so notes/gridlines/ruler stay legible
    // underneath) - the loop region's ruler-band highlight needs to sit over paintRuler's own
    // opaque fill to be visible at all, and the playhead is a transport cursor, conventionally
    // always drawn on top of everything else.
    paintLoopRegion(g);
    paintPlayhead(g);
}

void MidiEditor::resized()
{
    Component::resized();

    const auto bounds = getLocalBounds().toFloat();

    _hScrollBar.setBounds(juce::Rectangle<float>(kGutterWidth, bounds.getHeight() - kScrollbarThickness,
        bounds.getWidth() - kGutterWidth - kScrollbarThickness, kScrollbarThickness).toNearestInt());
    _vScrollBar.setBounds(juce::Rectangle<float>(bounds.getWidth() - kScrollbarThickness, kRulerHeight,
        kScrollbarThickness, bounds.getHeight() - kRulerHeight - kScrollbarThickness).toNearestInt());

    const auto gridBottom = bounds.getHeight() - kScrollbarThickness - kChordLaneHeight;
    _contentArea = juce::Rectangle<float>(kGutterWidth, kRulerHeight,
        juce::jmax(0.f, bounds.getWidth() - kGutterWidth - kScrollbarThickness),
        juce::jmax(0.f, gridBottom - kRulerHeight));

    refreshScrollRanges();
}

void MidiEditor::addChordAtBeat(double startBeat, const theory::Chord& chord)
{
    const auto barIndex = std::floor(juce::jmax(0.0, startBeat) / kBeatsPerBar);
    const auto cellStart = barIndex * kBeatsPerBar;
    const auto cellEnd = cellStart + kBeatsPerBar;

    bool fullyCovered = false;
    auto leftBound = cellStart;
    auto rightBound = cellEnd;

    for (const auto& note : _notes)
    {
        const auto noteEnd = note.startBeat + note.lengthBeats;
        if (note.startBeat >= cellEnd || noteEnd <= cellStart)
            continue; // no overlap with the target cell at all

        if (note.startBeat <= cellStart && noteEnd >= cellEnd)
        {
            fullyCovered = true;
            break;
        }

        // Simplified two-sided model: a note touching the cell's left edge caps how far left the
        // new chord can start, one touching (or starting inside) the right side caps how far right
        // it can end. Doesn't handle a note sitting as an island fully inside the cell with free
        // space on both sides of it - an edge case only reachable via manual note placement, not
        // from a plain chord-after-chord drop sequence.
        if (note.startBeat <= cellStart)
            leftBound = juce::jmax(leftBound, noteEnd);
        else
            rightBound = juce::jmin(rightBound, note.startBeat);
    }

    double resolvedStart = cellStart;
    double resolvedLength = kBeatsPerBar;

    if (fullyCovered)
    {
        _notes.erase(std::remove_if(_notes.begin(), _notes.end(),
            [cellStart, cellEnd](const MidiNoteBlock& note)
            {
                const auto noteEnd = note.startBeat + note.lengthBeats;
                return !(note.startBeat >= cellEnd || noteEnd <= cellStart);
            }), _notes.end());
        _selectedNoteIndices.clear(); // indices are no longer meaningful after this bulk erase
    }
    else
    {
        if (leftBound >= rightBound)
            return; // no free room left in this cell at all

        resolvedStart = leftBound;
        resolvedLength = rightBound - leftBound;
    }

    for (const auto midiNote : theory::NoteConvertor::voiceChordCloseToMiddleC(chord))
        _notes.push_back({ midiNote, resolvedStart, resolvedLength });

    refreshScrollRanges();
    repaint();
    notifyContentChanged(); // recomputes _chordBlocks from the notes just placed
}

void MidiEditor::clear()
{
    stopPlayback();

    _notes.clear();
    _chordBlocks.clear();
    _loopStartBeat = 0.0;
    _loopEndBeat = kBeatsPerBar;
    _loopManuallyAdjusted = false;
    _selectedNoteIndices.clear();
    refreshScrollRanges();
    repaint();
}

void MidiEditor::setKeyAndScale(theory::Key key, theory::Scale scale)
{
    _currentKey = key;
    _currentScale = scale;
    recomputeChordBlocksFromNotes();
    repaint();
}

std::optional<int> MidiEditor::getNoteMidiPitch(int index) const
{
    if (index < 0 || index >= static_cast<int>(_notes.size()))
        return std::nullopt;
    return _notes[static_cast<std::size_t>(index)].midiNote;
}

std::optional<double> MidiEditor::getNoteStartBeat(int index) const
{
    if (index < 0 || index >= static_cast<int>(_notes.size()))
        return std::nullopt;
    return _notes[static_cast<std::size_t>(index)].startBeat;
}

std::optional<double> MidiEditor::getNoteLengthBeats(int index) const
{
    if (index < 0 || index >= static_cast<int>(_notes.size()))
        return std::nullopt;
    return _notes[static_cast<std::size_t>(index)].lengthBeats;
}

std::optional<double> MidiEditor::getChordBlockStartBeat(int index) const
{
    if (index < 0 || index >= static_cast<int>(_chordBlocks.size()))
        return std::nullopt;
    return _chordBlocks[static_cast<std::size_t>(index)].startBeat;
}

std::optional<double> MidiEditor::getChordBlockLengthBeats(int index) const
{
    if (index < 0 || index >= static_cast<int>(_chordBlocks.size()))
        return std::nullopt;
    return _chordBlocks[static_cast<std::size_t>(index)].lengthBeats;
}

std::optional<theory::ProgressionSlot> MidiEditor::getChordBlockSlot(int index) const
{
    if (index < 0 || index >= static_cast<int>(_chordBlocks.size()))
        return std::nullopt;
    return _chordBlocks[static_cast<std::size_t>(index)].detectedSlot;
}

std::optional<std::string> MidiEditor::getChordBlockLabel(int index) const
{
    if (index < 0 || index >= static_cast<int>(_chordBlocks.size()))
        return std::nullopt;
    return _chordBlocks[static_cast<std::size_t>(index)].label;
}

theory::MidiEditorState MidiEditor::getState() const
{
    theory::MidiEditorState state;

    state.notes.reserve(_notes.size());
    for (const auto& note : _notes)
        state.notes.push_back({ note.midiNote, note.startBeat, note.lengthBeats });

    return state;
}

void MidiEditor::restoreState(const theory::MidiEditorState& state)
{
    stopPlayback();
    _loopManuallyAdjusted = false;
    _selectedNoteIndices.clear();

    _notes.clear();
    _notes.reserve(state.notes.size());
    for (const auto& note : state.notes)
        _notes.push_back({ note.midiNote, note.startBeat, note.lengthBeats });

    recomputeChordBlocksFromNotes(); // restoreState deliberately skips notifyContentChanged()
    recomputeLoopBoundsFromContent();
    refreshScrollRanges();
    repaint();
}

void MidiEditor::startPlayback()
{
    if (_progressionPlayer == nullptr || _notes.empty() || _progressionPlayer->isPlaying())
        return;

    std::vector<audio::ScheduledNote> scheduledNotes;
    scheduledNotes.reserve(_notes.size());
    for (const auto& note : _notes)
        scheduledNotes.push_back({ note.midiNote, note.startBeat, note.lengthBeats });

    _progressionPlayer->setNotes(scheduledNotes);
    _progressionPlayer->setLoopBounds(_loopStartBeat, _loopEndBeat);
    _progressionPlayer->play();

    if (!isTimerRunning())
        startTimerHz(45);

    repaint();

    for (auto* listener : _listeners)
        listener->onPlaybackStateChanged(true);
}

void MidiEditor::stopPlayback()
{
    if (_progressionPlayer == nullptr || !_progressionPlayer->isPlaying())
        return;

    _progressionPlayer->stop();

    if (_dragMode == DragMode::None)
        stopTimer();

    repaint();

    for (auto* listener : _listeners)
        listener->onPlaybackStateChanged(false);
}

bool MidiEditor::isPlaying() const
{
    return _progressionPlayer != nullptr && _progressionPlayer->isPlaying();
}

void MidiEditor::addListener(Listener* listener)
{
    _listeners.push_back(listener);
}

void MidiEditor::removeListener(Listener* listener)
{
    _listeners.erase(std::remove(_listeners.begin(), _listeners.end(), listener), _listeners.end());
}

void MidiEditor::notifyContentChanged()
{
    recomputeChordBlocksFromNotes();
    recomputeLoopBoundsFromContent();

    // Keep a playing loop in sync with live edits - audible on the next pass rather than stale.
    if (_progressionPlayer != nullptr && _progressionPlayer->isPlaying())
    {
        std::vector<audio::ScheduledNote> scheduledNotes;
        scheduledNotes.reserve(_notes.size());
        for (const auto& note : _notes)
            scheduledNotes.push_back({ note.midiNote, note.startBeat, note.lengthBeats });

        _progressionPlayer->setNotes(scheduledNotes);
        _progressionPlayer->setLoopBounds(_loopStartBeat, _loopEndBeat);
    }

    for (auto* listener : _listeners)
        listener->onContentChanged();
}

void MidiEditor::recomputeChordBlocksFromNotes()
{
    _chordBlocks.clear();

    // Phase 1: cluster note indices by onset-time proximity into candidate chord-change boundaries -
    // sort by startBeat, then sweep: a note joins the current cluster if its own onset falls within
    // kChordOnsetToleranceBeats of that cluster's FIRST onset (a fixed window, not chained note-to-
    // note) - chaining against only the immediately preceding note would let a "staircase" of
    // closely-spaced onsets transitively bridge two genuinely unrelated chords across a much wider
    // span than the tolerance itself.
    std::vector<int> order(_notes.size());
    for (int i = 0; i < static_cast<int>(_notes.size()); ++i)
        order[static_cast<std::size_t>(i)] = i;
    std::sort(order.begin(), order.end(),
        [this](int a, int b) { return _notes[static_cast<std::size_t>(a)].startBeat < _notes[static_cast<std::size_t>(b)].startBeat; });

    std::vector<std::vector<int>> clusters;
    auto clusterFirstOnset = -std::numeric_limits<double>::infinity();
    for (const auto index : order)
    {
        const auto& note = _notes[static_cast<std::size_t>(index)];
        if (clusters.empty() || note.startBeat - clusterFirstOnset > kChordOnsetToleranceBeats)
        {
            clusters.push_back({});
            clusterFirstOnset = note.startBeat;
        }
        clusters.back().push_back(index);
    }

    // Phase 2: each cluster is a candidate chord at its own boundary (= its first onset). Its pitch
    // content is that cluster's own notes, plus - only if its own notes alone don't already form a
    // match - the SMALLEST combination of still-ringing notes from EARLIER clusters (still sounding
    // exactly at this boundary) that completes one. A still-ringing note (pedal tone, tied note,
    // long sustain) genuinely contributes to whatever harmony is sounding while it rings, even
    // though it didn't newly onset here - but an ordinary trailing overlap (the previous chord's own
    // notes releasing a beat-fraction after this one begins, an everyday consequence of hand-played
    // or hand-edited timing) must not get dragged in just because it's technically still sounding,
    // if the new chord already stands complete without it. This is deliberately NOT symmetric (a
    // later cluster's notes never reach back into an earlier boundary's content) and a note's
    // sustain length plays no part in Phase 1's boundaries - only in which later boundaries' content
    // it happens to still overlap and might help complete.
    for (std::size_t clusterIndex = 0; clusterIndex < clusters.size(); ++clusterIndex)
    {
        const auto& cluster = clusters[clusterIndex];
        const auto boundary = _notes[static_cast<std::size_t>(cluster.front())].startBeat;

        std::vector<int> ownNotes;
        auto groupEnd = boundary;
        for (const auto index : cluster)
        {
            const auto& note = _notes[static_cast<std::size_t>(index)];
            ownNotes.push_back(note.midiNote);
            groupEnd = juce::jmax(groupEnd, note.startBeat + note.lengthBeats);
        }

        std::vector<int> borrowCandidates;
        for (std::size_t earlierClusterIndex = 0; earlierClusterIndex < clusterIndex; ++earlierClusterIndex)
        {
            for (const auto index : clusters[earlierClusterIndex])
            {
                const auto& note = _notes[static_cast<std::size_t>(index)];
                if (note.startBeat <= boundary && note.startBeat + note.lengthBeats > boundary)
                    borrowCandidates.push_back(note.midiNote);
            }
        }

        if (const auto detected = identifyWithMinimalBorrowing(ownNotes, borrowCandidates, _currentKey, _currentScale))
            _chordBlocks.push_back({ detected->label, boundary, groupEnd - boundary, detected->slot });
    }

    // Several callers (and tests) index chord blocks assuming left-to-right order, which grouping
    // order alone doesn't guarantee.
    std::sort(_chordBlocks.begin(), _chordBlocks.end(),
        [](const ChordBlockData& a, const ChordBlockData& b) { return a.startBeat < b.startBeat; });
}

void MidiEditor::recomputeLoopBoundsFromContent()
{
    if (_loopManuallyAdjusted || _notes.empty())
        return;

    auto minStart = std::numeric_limits<double>::max();
    auto maxEnd = 0.0;
    for (const auto& note : _notes)
    {
        minStart = juce::jmin(minStart, note.startBeat);
        maxEnd = juce::jmax(maxEnd, note.startBeat + note.lengthBeats);
    }

    _loopStartBeat = std::floor(minStart / kBeatsPerBar) * kBeatsPerBar;
    _loopEndBeat = std::ceil(maxEnd / kBeatsPerBar) * kBeatsPerBar;
}

bool MidiEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    return files.size() == 1 && files[0].endsWithIgnoreCase(".mid");
}

void MidiEditor::fileDragEnter(const juce::StringArray&, int, int)
{
}

void MidiEditor::fileDragExit(const juce::StringArray&)
{
}

void MidiEditor::filesDropped(const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused(y);

    if (files.isEmpty())
        return;

    const auto dropBeat = juce::jmax(0.0, static_cast<double>(xToBeat(static_cast<float>(x))));
    for (auto* listener : _listeners)
        listener->onChordFileDropped(dropBeat, files[0]);
}

void MidiEditor::scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart)
{
    if (scrollBarThatHasMoved == &_hScrollBar)
        _scrollBeat = juce::jmax(0.0, newRangeStart);
    else if (scrollBarThatHasMoved == &_vScrollBar)
        _scrollRow = static_cast<float>(juce::jmax(0.0, newRangeStart));

    repaint();
}

void MidiEditor::mouseEnter(const juce::MouseEvent&)
{
    _isHovering = true;
    updateScrollBarVisibility();
}

void MidiEditor::mouseExit(const juce::MouseEvent& event)
{
    // _childBoundaryListener (see MidiEditor.h) forwards here for every child's own boundary
    // crossings too (e.g. moving from the content area onto a scrollbar) - only treat it as a real
    // "left the whole editor" exit if the pointer has actually left this component's own bounds.
    if (getLocalBounds().contains(event.getEventRelativeTo(this).getPosition()))
        return;

    _isHovering = false;
    updateScrollBarVisibility();
}

void MidiEditor::mouseMove(const juce::MouseEvent& event)
{
    updateHoverState(event.position);
}

void MidiEditor::mouseDown(const juce::MouseEvent& event)
{
    // So a subsequent Delete/arrow-key press reaches keyPressed(). Guarded on isShowing() only to
    // avoid grabKeyboardFocus()'s own jassert in unit tests that drive mouse events without a real
    // window/peer - always true for a real mouseDown, since JUCE only dispatches mouse events to
    // components that are actually on screen.
    if (isShowing())
        grabKeyboardFocus();

    _dragStartMouse = event.position;
    _lastMousePosition = event.position;
    _pressedChordBlockIndex = -1;
    _chordBlockDragGestureStarted = false;

    if (isInLoopHandleZone(event.position, true))
    {
        _dragMode = DragMode::ResizeLoopStart;
        _dragStartBeat = _loopStartBeat;
        startTimerHz(45);
        return;
    }
    if (isInLoopHandleZone(event.position, false))
    {
        _dragMode = DragMode::ResizeLoopEnd;
        _dragStartBeat = _loopEndBeat;
        startTimerHz(45);
        return;
    }

    const auto noteIndex = hitTestNote(event.position);
    if (noteIndex >= 0)
    {
        // Clicking a note that isn't already part of the current selection replaces the selection
        // immediately; clicking one that already is keeps the whole multi-selection intact for a
        // possible group-drag (mouseUp collapses it to just this note if the click turns out not
        // to have been a drag at all - see mouseUp).
        _mouseDownNoteWasAlreadySelected = std::find(_selectedNoteIndices.begin(), _selectedNoteIndices.end(), noteIndex) != _selectedNoteIndices.end();
        if (!_mouseDownNoteWasAlreadySelected)
        {
            _selectedNoteIndices = { noteIndex };
            repaint();
        }

        _dragStartSnapshots.clear();
        _dragStartSnapshots.reserve(_selectedNoteIndices.size());
        for (const auto selected : _selectedNoteIndices)
            _dragStartSnapshots.push_back(_notes[static_cast<std::size_t>(selected)]);

        const auto& note = _notes[static_cast<std::size_t>(noteIndex)];
        _draggedNoteIndex = noteIndex; // the anchor - determines resize direction for the whole selection
        if (isInNoteResizeZone(noteIndex, event.position, true))
            _dragMode = DragMode::ResizeNoteStart;
        else if (isInNoteResizeZone(noteIndex, event.position, false))
            _dragMode = DragMode::ResizeNoteEnd;
        else
            _dragMode = DragMode::MoveNote;
        _dragStartBeat = note.startBeat;
        _dragStartLengthBeats = note.lengthBeats;
        _dragStartMidiNote = note.midiNote;
        startTimerHz(45);
        return;
    }

    if (isInChordLaneZone(event.position))
    {
        // A click that lands in the lane but hits no block's segment (the gaps between chords, or
        // past the last one) starts no drag mode at all - it's neither a note gesture nor a
        // marquee-select region, so mouseDrag/mouseUp both just see DragMode::None and no-op.
        _pressedChordBlockIndex = hitTestChordBlock(event.position);
        if (_pressedChordBlockIndex >= 0)
            _dragMode = DragMode::DragChordBlockOut;
        return;
    }

    // Nothing hit - starts a rubber-band selection; mouseUp decides whether this was actually a
    // drag (select everything the rect ends up covering) or just a plain click (deselect).
    _dragMode = DragMode::MarqueeSelect;
    _marqueeRect = juce::Rectangle<float>();
    startTimerHz(45);
}

void MidiEditor::mouseDrag(const juce::MouseEvent& event)
{
    _lastMousePosition = event.position;

    if (_dragMode == DragMode::DragChordBlockOut)
    {
        if (_chordBlockDragGestureStarted || _pressedChordBlockIndex < 0)
            return;

        if (_dragStartMouse.getDistanceFrom(event.position) < kClickVsDragThreshold)
            return;

        _chordBlockDragGestureStarted = true;

        for (auto* listener : _listeners)
            listener->onChordBlockDragStarted(_pressedChordBlockIndex);
        return;
    }

    applyDragAt(event.position);
}

void MidiEditor::mouseUp(const juce::MouseEvent&)
{
    const auto finishedDragMode = _dragMode;
    const auto didDrag = finishedDragMode != DragMode::None;
    const auto finishedNoteIndex = _draggedNoteIndex;
    const auto mouseActuallyMoved = _dragStartMouse.getDistanceFrom(_lastMousePosition) > kClickVsDragThreshold;

    _dragMode = DragMode::None;
    _draggedNoteIndex = -1;
    _dragStartSnapshots.clear();
    _pressedChordBlockIndex = -1;
    _chordBlockDragGestureStarted = false;

    // The timer is now shared with playback repaint (see timerCallback) - only stop it here if
    // nothing else still needs it running.
    if (!isPlaying())
        stopTimer();

    refreshScrollRanges();
    repaint();

    if (finishedDragMode == DragMode::ResizeLoopStart || finishedDragMode == DragMode::ResizeLoopEnd)
    {
        _loopManuallyAdjusted = true;
        if (_progressionPlayer != nullptr)
            _progressionPlayer->setLoopBounds(_loopStartBeat, _loopEndBeat);
    }
    else if (finishedDragMode == DragMode::DragChordBlockOut)
    {
        // A pure export gesture (or a plain click on a chord segment) - no note content changed,
        // so no notifyContentChanged() here, unlike the generic didDrag branch below.
    }
    else if (finishedDragMode == DragMode::MarqueeSelect)
    {
        const auto isTrivialClick = _marqueeRect.getWidth() < kClickVsDragThreshold && _marqueeRect.getHeight() < kClickVsDragThreshold;
        if (isTrivialClick)
        {
            // A plain click where there's no note block - the literal "deselect by clicking
            // somewhere with no midi note blocks" requirement.
            _selectedNoteIndices.clear();
        }
        else
        {
            _selectedNoteIndices.clear();
            for (int i = 0; i < static_cast<int>(_notes.size()); ++i)
            {
                const auto& note = _notes[static_cast<std::size_t>(i)];
                const juce::Rectangle<float> noteBounds(beatToX(note.startBeat), pitchToY(note.midiNote),
                    static_cast<float>(note.lengthBeats * static_cast<double>(_pixelsPerBeat)), _rowHeight);
                if (_marqueeRect.intersects(noteBounds))
                    _selectedNoteIndices.push_back(i);
            }
        }
        _marqueeRect = juce::Rectangle<float>();
        repaint();
    }
    else if (didDrag)
    {
        // A plain click (no real movement) on a note that was already part of a bigger selection
        // narrows the selection down to just that one - the "select a single block by clicking on
        // it" requirement, without ever preventing a drag of the whole group (see mouseDown).
        if (!mouseActuallyMoved && _mouseDownNoteWasAlreadySelected && finishedNoteIndex >= 0
            && (finishedDragMode == DragMode::MoveNote || finishedDragMode == DragMode::ResizeNoteStart || finishedDragMode == DragMode::ResizeNoteEnd))
        {
            _selectedNoteIndices = { finishedNoteIndex };
            repaint();
        }

        notifyContentChanged();
    }
}

void MidiEditor::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (event.position.y >= 0.f && event.position.y <= kRulerHeight)
    {
        zoomToLoop();
        return;
    }

    const auto noteIndex = hitTestNote(event.position);
    if (noteIndex >= 0)
    {
        _notes.erase(_notes.begin() + noteIndex);
        _hoveredNoteIndex = -1;
        _selectedNoteIndices.clear(); // a single-index erase shifts every later index - simplest to just drop the selection
        refreshScrollRanges();
        repaint();
        notifyContentChanged();
        return;
    }

    if (!_contentArea.contains(event.position))
        return;

    _notes.push_back({ yToPitch(event.position.y), juce::jmax(0.0, snapBeat(xToBeat(event.position.x))), kDefaultNoteLengthBeats });
    refreshScrollRanges();
    repaint();
    notifyContentChanged();
}

void MidiEditor::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (event.mods.isCommandDown() && event.mods.isShiftDown())
    {
        zoomVertical(std::pow(kZoomStepMultiplier, wheel.deltaY), event.position);
        return;
    }
    if (event.mods.isCommandDown())
    {
        zoomHorizontal(std::pow(kZoomStepMultiplier, wheel.deltaY), event.position);
        return;
    }

    if (event.mods.isShiftDown())
    {
        _scrollBeat = juce::jmax(0.0, _scrollBeat - static_cast<double>(wheel.deltaY) * kWheelScrollBeats);
    }
    else
    {
        _scrollRow = juce::jlimit(0.f, static_cast<float>(kNumPitchRows) - 1.f, _scrollRow - wheel.deltaY * kWheelScrollRows);
        if (wheel.deltaX != 0.f)
            _scrollBeat = juce::jmax(0.0, _scrollBeat - static_cast<double>(wheel.deltaX) * kWheelScrollBeats);
    }

    refreshScrollRanges();
    repaint();
}

void MidiEditor::mouseMagnify(const juce::MouseEvent& event, float scaleFactor)
{
    zoomHorizontal(scaleFactor, event.position);
}

bool MidiEditor::keyPressed(const juce::KeyPress& key)
{
    if (_selectedNoteIndices.empty())
        return false;

    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        // Descending order so erasing an earlier index doesn't shift a later one out from under us.
        auto sortedIndices = _selectedNoteIndices;
        std::sort(sortedIndices.begin(), sortedIndices.end(), std::greater<int>());
        for (const auto index : sortedIndices)
            _notes.erase(_notes.begin() + index);

        _selectedNoteIndices.clear();
        _hoveredNoteIndex = -1;
        refreshScrollRanges();
        repaint();
        notifyContentChanged();
        return true;
    }

    if (key == juce::KeyPress::leftKey || key == juce::KeyPress::rightKey
        || key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
    {
        const auto beatDelta = key == juce::KeyPress::leftKey ? -kSnapBeats : key == juce::KeyPress::rightKey ? kSnapBeats : 0.0;
        const auto pitchDelta = key == juce::KeyPress::upKey ? 1 : key == juce::KeyPress::downKey ? -1 : 0;

        for (const auto index : _selectedNoteIndices)
        {
            auto& note = _notes[static_cast<std::size_t>(index)];
            note.startBeat = juce::jmax(0.0, note.startBeat + beatDelta);
            note.midiNote = juce::jlimit(kMinMidiNote, kMaxMidiNote, note.midiNote + pitchDelta);
        }

        refreshScrollRanges();
        repaint();
        notifyContentChanged();
        return true;
    }

    return false;
}

void MidiEditor::timerCallback()
{
    // Auto-scroll while dragging near an edge - nudge the scroll offset, then re-apply the drag at
    // the same (now-stale-relative-to-content) mouse position so it keeps tracking it. mouseDrag
    // alone only fires on actual OS mouse-move events, never while the pointer holds still.
    constexpr float kEdgeMargin = 24.f;

    if (_dragMode == DragMode::None)
    {
        // No drag in progress - the timer can only still be running because playback is active
        // (see startPlayback/stopPlayback), so its only job here is to keep the playhead's visual
        // position in sync with the audio thread's actual position.
        if (isPlaying())
            repaint();
        return;
    }

    if (_lastMousePosition.x < _contentArea.getX() + kEdgeMargin)
        _scrollBeat = juce::jmax(0.0, _scrollBeat - 0.15);
    else if (_lastMousePosition.x > _contentArea.getRight() - kEdgeMargin)
        _scrollBeat += 0.15;

    if (_dragMode != DragMode::ResizeLoopStart && _dragMode != DragMode::ResizeLoopEnd)
    {
        if (_lastMousePosition.y < _contentArea.getY() + kEdgeMargin)
            _scrollRow = juce::jmax(0.f, _scrollRow - 0.3f);
        else if (_lastMousePosition.y > _contentArea.getBottom() - kEdgeMargin)
            _scrollRow = juce::jmin(static_cast<float>(kNumPitchRows) - 1.f, _scrollRow + 0.3f);
    }

    refreshScrollRanges();
    applyDragAt(_lastMousePosition);
}

void MidiEditor::paintGridlines(juce::Graphics& g) const
{
    const auto border = nui::Theme::newColor(nui::Theme::ThemeColor::BORDER).asJuce();

    g.setColour(border.withAlpha(0.15f));
    for (int midiNote = kMinMidiNote; midiNote <= kMaxMidiNote; ++midiNote)
    {
        const auto y = pitchToY(midiNote);
        if (y < _contentArea.getY() - _rowHeight || y > _contentArea.getBottom())
            continue;

        g.drawHorizontalLine(static_cast<int>(y), _contentArea.getX(), _contentArea.getRight());
    }

    const auto firstBeat = juce::jmax(0, static_cast<int>(std::floor(xToBeat(_contentArea.getX()))));
    const auto lastBeat = static_cast<int>(std::ceil(xToBeat(_contentArea.getRight())));
    for (int beat = firstBeat; beat <= lastBeat; ++beat)
    {
        const auto x = beatToX(static_cast<double>(beat));
        const auto isBarLine = beat % static_cast<int>(kBeatsPerBar) == 0;
        g.setColour(border.withAlpha(isBarLine ? 0.35f : 0.15f));
        g.drawVerticalLine(static_cast<int>(x), _contentArea.getY(), _contentArea.getBottom());
    }
}

void MidiEditor::paintChordLane(juce::Graphics& g) const
{
    const auto laneTop = _contentArea.getBottom();
    const auto laneWidth = static_cast<float>(getWidth());
    const auto laneHeight = static_cast<float>(getHeight()) - laneTop;
    const auto radius = nui::Theme::getBorderRadius();

    // Reaches every remaining edge (left, right, bottom) so its bottom-left/bottom-right corners
    // land exactly on the whole widget's own outer corners - only the bottom two are rounded here,
    // matching the widget-level displayBackground/displayBorder radius (see the constructor and
    // paintRuler's own top-rounded counterpart), since a plain fillRect/drawRect would otherwise
    // square off those corners over top of the widget's own rounded background/border.
    juce::Path laneBand;
    laneBand.addRoundedRectangle(0.f, laneTop, laneWidth, laneHeight, radius, radius, false, false, true, true);

    g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::BACKGROUND).asJuce());
    g.fillPath(laneBand);
    g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::BORDER).asJuce());
    g.strokePath(laneBand, juce::PathStrokeType(1.f));

    const auto accent = nui::Theme::newColor(nui::Theme::ThemeColor::ACCENT).asJuce();
    g.setFont(nui::Theme::newFont(nui::Theme::REGULAR, nui::Theme::SMALL));

    for (const auto& block : _chordBlocks)
    {
        const auto bounds = juce::Rectangle<float>(beatToX(block.startBeat), laneTop + 2.f,
            static_cast<float>(block.lengthBeats * static_cast<double>(_pixelsPerBeat)) - 4.f, kChordLaneHeight - 4.f);
        if (bounds.getRight() < _contentArea.getX() || bounds.getX() > _contentArea.getRight())
            continue;

        g.setColour(accent.withAlpha(0.2f));
        g.fillRoundedRectangle(bounds, 4.f);
        g.setColour(accent);
        g.drawRoundedRectangle(bounds, 4.f, 1.f);

        g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::TEXT).asJuce());
        g.drawText(block.label, bounds, juce::Justification::centred, true);
    }
}

void MidiEditor::paintNotes(juce::Graphics& g) const
{
    const auto accent = nui::Theme::newColor(nui::Theme::ThemeColor::ACCENT).asJuce();

    for (int i = 0; i < static_cast<int>(_notes.size()); ++i)
    {
        const auto& note = _notes[static_cast<std::size_t>(i)];
        auto bounds = juce::Rectangle<float>(beatToX(note.startBeat), pitchToY(note.midiNote) + 1.f,
            static_cast<float>(note.lengthBeats * static_cast<double>(_pixelsPerBeat)) - 2.f, _rowHeight - 2.f);

        if (bounds.getBottom() < _contentArea.getY() || bounds.getY() > _contentArea.getBottom()
            || bounds.getRight() < _contentArea.getX() || bounds.getX() > _contentArea.getRight())
            continue;

        g.setColour(accent.withAlpha(.6f));
        g.fillRoundedRectangle(bounds, 3.f);

        if (std::find(_selectedNoteIndices.begin(), _selectedNoteIndices.end(), i) != _selectedNoteIndices.end())
        {
            g.setColour(accent);
            g.drawRoundedRectangle(bounds, 3.f, 2.f);
        }

        if (i == _hoveredNoteIndex && _hoveredIsResizeZone)
        {
            g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::EMPTY_SHADE).asJuce());
            g.fillRoundedRectangle(_hoveredResizeIsLeftEdge ? bounds.removeFromLeft(kResizeHandleWidth) : bounds.removeFromRight(kResizeHandleWidth), 3.f);
        }
    }
}

void MidiEditor::paintMarqueeSelection(juce::Graphics& g) const
{
    if (_dragMode != DragMode::MarqueeSelect || _marqueeRect.isEmpty())
        return;

    // Same low-alpha-fill-plus-stroke idiom as paintLoopRegion's bands.
    const auto accent = nui::Theme::newColor(nui::Theme::ThemeColor::ACCENT).asJuce();
    g.setColour(accent.withAlpha(0.15f));
    g.fillRect(_marqueeRect);
    g.setColour(accent);
    g.drawRect(_marqueeRect, 1.f);
}

void MidiEditor::paintRuler(juce::Graphics& g) const
{
    const juce::Rectangle<float> rulerBounds(kGutterWidth, 0.f, static_cast<float>(getWidth()) - kGutterWidth, kRulerHeight);
    const auto radius = nui::Theme::getBorderRadius();

    // Spans the full width (not just rulerBounds, which starts past the gutter) so its top-left/
    // top-right corners land exactly on the whole widget's own outer corners - only the top two are
    // rounded here, matching the widget-level displayBackground/displayBorder radius (see the
    // constructor and paintChordLane's own bottom-rounded counterpart). This also covers what used
    // to be the gutter's own separate top-left corner patch, so paintGutter no longer draws one.
    juce::Path topBand;
    topBand.addRoundedRectangle(0.f, 0.f, static_cast<float>(getWidth()), kRulerHeight, radius, radius, true, true, false, false);

    g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::BACKGROUND).asJuce());
    g.fillPath(topBand);
    g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::BORDER).asJuce());
    g.strokePath(topBand, juce::PathStrokeType(1.f));

    g.setFont(nui::Theme::newFont(nui::Theme::REGULAR, nui::Theme::SMALL));

    const auto firstBeat = juce::jmax(0, static_cast<int>(std::floor(xToBeat(rulerBounds.getX()))));
    const auto lastBeat = static_cast<int>(std::ceil(xToBeat(rulerBounds.getRight())));
    for (int beat = firstBeat; beat <= lastBeat; ++beat)
    {
        const auto x = beatToX(static_cast<double>(beat));
        if (x < rulerBounds.getX() || x > rulerBounds.getRight())
            continue;

        const auto bar = beat / static_cast<int>(kBeatsPerBar) + 1;
        const auto beatInBar = beat % static_cast<int>(kBeatsPerBar);
        const auto label = beatInBar == 0 ? juce::String(bar) : juce::String(bar) + "." + juce::String(beatInBar + 1);

        g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::TEXT).asJuce());
        g.drawText(label, juce::Rectangle<float>(x + kRulerTickLabelGap, 0.f, 60.f, kRulerHeight), juce::Justification::centredLeft, true);

        g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::BORDER).asJuce());
        g.drawVerticalLine(static_cast<int>(x), kRulerHeight - kRulerTickHeight, kRulerHeight);
    }
}

void MidiEditor::paintGutter(juce::Graphics& g) const
{
    // The corner above the grid (behind the ruler's own left edge) is now covered by paintRuler's
    // own top-rounded band, which spans the full widget width - painting it again here would just
    // square off the widget's rounded top-left corner over top of that. Only the actual per-row key
    // cells below get the white/black key colouring, clipped strictly to the grid's own vertical
    // span so nothing bleeds into the ruler or chord lane rows.
    juce::Graphics::ScopedSaveState saved(g);
    g.reduceClipRegion(juce::Rectangle<float>(0.f, _contentArea.getY(), kGutterWidth, _contentArea.getHeight()).toNearestInt());

    const auto whiteKeyColour = juce::Colour(0xFFE2DBDC);
    const auto blackKeyColour = juce::Colour(0xFF222122);

    for (int midiNote = kMinMidiNote; midiNote <= kMaxMidiNote; ++midiNote)
    {
        const auto y = pitchToY(midiNote);
        if (y + _rowHeight < _contentArea.getY() || y > _contentArea.getBottom())
            continue;

        const auto isBlackKey = kIsBlackKey[static_cast<std::size_t>(midiNote % 12)];
        g.setColour(isBlackKey ? blackKeyColour : whiteKeyColour);
        g.fillRect(juce::Rectangle<float>(0.f, y, kGutterWidth, _rowHeight));
    }

    // Drawn as dedicated 1px lines rather than a per-row drawRect border - two adjacent rows each
    // drawing a float-stroked rect on their shared edge can anti-alias to a visibly thicker seam
    // whenever _rowHeight (zoom-adjustable) doesn't land on a whole pixel; drawHorizontalLine/
    // drawVerticalLine always draw exactly one physical pixel, and each boundary is only drawn once.
    g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::BORDER).asJuce());
    for (int midiNote = kMinMidiNote; midiNote <= kMaxMidiNote; ++midiNote)
    {
        const auto y = pitchToY(midiNote);
        if (y + _rowHeight < _contentArea.getY() || y > _contentArea.getBottom())
            continue;

        g.drawHorizontalLine(static_cast<int>(y), 0.f, kGutterWidth);
    }
    g.drawVerticalLine(0, _contentArea.getY(), _contentArea.getBottom());
    g.drawVerticalLine(static_cast<int>(kGutterWidth), _contentArea.getY(), _contentArea.getBottom());

    g.setColour(juce::Colour(0xFF222122)); // dark text over the light #D9D9D9 white-key cells
    g.setFont(nui::Theme::newFont(nui::Theme::REGULAR, nui::Theme::SMALL));

    for (int midiNote = kMinMidiNote; midiNote <= kMaxMidiNote; ++midiNote)
    {
        if (kIsBlackKey[static_cast<std::size_t>(midiNote % 12)])
            continue;

        const auto y = pitchToY(midiNote);
        if (y + _rowHeight < _contentArea.getY() || y > _contentArea.getBottom())
            continue;

        const auto label = kNoteNames[static_cast<std::size_t>(midiNote % 12)] + juce::String(midiNote / 12 - 1);
        g.drawText(label, juce::Rectangle<float>(2.f, y, kGutterWidth - 4.f, _rowHeight), juce::Justification::centred, true);
    }
}

void MidiEditor::paintLoopRegion(juce::Graphics& g) const
{
    const auto x0 = beatToX(_loopStartBeat);
    const auto x1 = beatToX(_loopEndBeat);
    const auto width = static_cast<float>(getWidth());
    if (x1 < kGutterWidth || x0 > width)
        return; // entirely scrolled off-screen

    const auto accent = nui::Theme::newColor(nui::Theme::ThemeColor::ACCENT).asJuce();
    const auto visibleX0 = juce::jmax(x0, kGutterWidth);
    const auto visibleX1 = juce::jmin(x1, width);

    // Low-alpha so the ruler/gridlines/notes underneath stay fully legible - the ruler-band strip
    // needs to sit over paintRuler's own opaque fill (this is drawn later, in the "on top" tier) to
    // be visible at all; the fainter content/chord-lane band just needs to read as "this is the
    // loop's extent" without competing with the notes for attention.
    g.setColour(accent.withAlpha(0.2f));
    g.fillRect(juce::Rectangle<float>(visibleX0, 0.f, visibleX1 - visibleX0, kRulerHeight));

    g.setColour(accent.withAlpha(0.08f));
    g.fillRect(juce::Rectangle<float>(visibleX0, _contentArea.getY(), visibleX1 - visibleX0, static_cast<float>(getHeight()) - _contentArea.getY()));

    constexpr float kHandleWidth = 4.f;
    g.setColour(accent);
    if (x0 >= kGutterWidth && x0 <= width)
        g.fillRect(juce::Rectangle<float>(x0 - kHandleWidth * 0.5f, 0.f, kHandleWidth, kRulerHeight));
    if (x1 >= kGutterWidth && x1 <= width)
        g.fillRect(juce::Rectangle<float>(x1 - kHandleWidth * 0.5f, 0.f, kHandleWidth, kRulerHeight));
}

void MidiEditor::paintPlayhead(juce::Graphics& g) const
{
    // Only once there's (or has been) real content to loop over - an untouched, contentless editor
    // has no meaningful playhead position to show.
    if (_notes.empty() && !_loopManuallyAdjusted)
        return;

    const auto playing = _progressionPlayer != nullptr && _progressionPlayer->isPlaying();
    const auto playheadBeat = playing ? _progressionPlayer->getPlayheadBeat() : _loopStartBeat;
    const auto x = beatToX(playheadBeat);
    const auto width = static_cast<float>(getWidth());
    if (x < kGutterWidth || x > width)
        return;

    g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::TEXT).asJuce());
    g.drawVerticalLine(static_cast<int>(x), 0.f, static_cast<float>(getHeight()));

    juce::Path flag;
    flag.addTriangle(x - 4.f, 0.f, x + 4.f, 0.f, x, 8.f);
    g.fillPath(flag);
}

float MidiEditor::beatToX(double beat) const noexcept
{
    return kGutterWidth + static_cast<float>((beat - _scrollBeat) * static_cast<double>(_pixelsPerBeat));
}

double MidiEditor::xToBeat(float x) const noexcept
{
    return _scrollBeat + static_cast<double>((x - kGutterWidth) / _pixelsPerBeat);
}

float MidiEditor::pitchToY(int midiNote) const noexcept
{
    const auto rowIndex = static_cast<float>(kMaxMidiNote - midiNote); // higher pitch = higher on screen
    return kRulerHeight + (rowIndex - _scrollRow) * _rowHeight;
}

int MidiEditor::yToPitch(float y) const noexcept
{
    const auto rowIndex = static_cast<int>(std::floor(_scrollRow + (y - kRulerHeight) / _rowHeight));
    return juce::jlimit(kMinMidiNote, kMaxMidiNote, kMaxMidiNote - rowIndex);
}

double MidiEditor::snapBeat(double beat) noexcept
{
    return juce::jmax(0.0, std::round(beat / kSnapBeats) * kSnapBeats);
}

int MidiEditor::hitTestNote(juce::Point<float> position) const
{
    for (int i = static_cast<int>(_notes.size()) - 1; i >= 0; --i)
    {
        const auto& note = _notes[static_cast<std::size_t>(i)];
        const juce::Rectangle<float> bounds(beatToX(note.startBeat), pitchToY(note.midiNote),
            static_cast<float>(note.lengthBeats * static_cast<double>(_pixelsPerBeat)), _rowHeight);
        if (bounds.contains(position))
            return i;
    }
    return -1;
}

bool MidiEditor::isInNoteResizeZone(int noteIndex, juce::Point<float> position, bool leftEdge) const
{
    if (noteIndex < 0 || noteIndex >= static_cast<int>(_notes.size()))
        return false;

    const auto& note = _notes[static_cast<std::size_t>(noteIndex)];

    if (leftEdge)
    {
        const auto edge = beatToX(note.startBeat);
        return position.x >= edge && position.x <= edge + kResizeHandleWidth;
    }

    const auto edge = beatToX(note.startBeat + note.lengthBeats);
    return position.x >= edge - kResizeHandleWidth && position.x <= edge;
}

bool MidiEditor::isInLoopHandleZone(juce::Point<float> position, bool startHandle) const
{
    if (position.y < 0.f || position.y > kRulerHeight)
        return false;

    // Symmetric around the edge (unlike a note's resize zone, which extends only into its own
    // body) - a loop handle has no "inside" it belongs to more than the other side.
    const auto edge = beatToX(startHandle ? _loopStartBeat : _loopEndBeat);
    return position.x >= edge - kResizeHandleWidth && position.x <= edge + kResizeHandleWidth;
}

bool MidiEditor::isInChordLaneZone(juce::Point<float> position) const
{
    return position.y >= _contentArea.getBottom() && position.y <= static_cast<float>(getHeight());
}

int MidiEditor::hitTestChordBlock(juce::Point<float> position) const
{
    if (!isInChordLaneZone(position))
        return -1;

    for (int i = 0; i < static_cast<int>(_chordBlocks.size()); ++i)
    {
        const auto& block = _chordBlocks[static_cast<std::size_t>(i)];
        const auto startX = beatToX(block.startBeat);
        const auto endX = beatToX(block.startBeat + block.lengthBeats);
        if (position.x >= startX && position.x <= endX)
            return i;
    }

    return -1;
}

void MidiEditor::applyDragAt(juce::Point<float> position)
{
    if (_dragMode == DragMode::None)
        return;

    const auto snap = !juce::ModifierKeys::currentModifiers.isShiftDown();
    const auto deltaBeats = xToBeat(position.x) - xToBeat(_dragStartMouse.x);

    if (_dragMode == DragMode::MoveNote && _draggedNoteIndex >= 0)
    {
        // deltaPitchRows as a *relative* shift, applied to every selected note's own drag-start
        // pitch, rather than snapping each one absolutely to yToPitch(position.y) - for a single
        // selected note this is mathematically identical to the old absolute behavior (hit-testing
        // guarantees yToPitch(_dragStartMouse.y) already equals that note's own original pitch), so
        // this one formula covers both the single- and multi-select cases with no behavior change
        // for the former.
        const auto deltaPitchRows = yToPitch(position.y) - yToPitch(_dragStartMouse.y);
        for (std::size_t i = 0; i < _selectedNoteIndices.size(); ++i)
        {
            auto& note = _notes[static_cast<std::size_t>(_selectedNoteIndices[i])];
            const auto& snapshot = _dragStartSnapshots[i];
            const auto rawStart = snapshot.startBeat + deltaBeats;
            note.startBeat = juce::jmax(0.0, snap ? snapBeat(rawStart) : rawStart);
            note.midiNote = juce::jlimit(kMinMidiNote, kMaxMidiNote, snapshot.midiNote + deltaPitchRows);
        }
        repaint();
    }
    else if (_dragMode == DragMode::ResizeNoteEnd && _draggedNoteIndex >= 0)
    {
        // The anchor's own new length, computed exactly as before a selection could be more than
        // one note; every other selected note is extended/reduced by that *same delta* (not
        // snapped to the anchor's new absolute end), so "extend them all together" preserves each
        // note's own length relationship rather than collapsing them all to one end time.
        const auto rawEnd = _dragStartBeat + _dragStartLengthBeats + deltaBeats;
        const auto snappedEnd = snap ? snapBeat(rawEnd) : rawEnd;
        const auto anchorNewLength = juce::jmax(kSnapBeats, snappedEnd - _dragStartBeat);
        const auto lengthDelta = anchorNewLength - _dragStartLengthBeats;

        for (std::size_t i = 0; i < _selectedNoteIndices.size(); ++i)
        {
            auto& note = _notes[static_cast<std::size_t>(_selectedNoteIndices[i])];
            note.lengthBeats = juce::jmax(kSnapBeats, _dragStartSnapshots[i].lengthBeats + lengthDelta);
        }
        repaint();
    }
    else if (_dragMode == DragMode::ResizeNoteStart && _draggedNoteIndex >= 0)
    {
        const auto originalEnd = _dragStartBeat + _dragStartLengthBeats; // anchor's own end, fixed
        const auto rawStart = _dragStartBeat + deltaBeats;
        const auto snappedStart = snap ? snapBeat(rawStart) : rawStart;
        const auto clampedStart = juce::jmin(snappedStart, originalEnd - kSnapBeats);
        const auto anchorNewStart = juce::jmax(0.0, clampedStart);
        const auto startDelta = anchorNewStart - _dragStartBeat;

        // Every other selected note keeps *its own* end fixed and shifts its start by the same
        // delta the anchor's start just moved by.
        for (std::size_t i = 0; i < _selectedNoteIndices.size(); ++i)
        {
            auto& note = _notes[static_cast<std::size_t>(_selectedNoteIndices[i])];
            const auto& snapshot = _dragStartSnapshots[i];
            const auto noteOriginalEnd = snapshot.startBeat + snapshot.lengthBeats;
            const auto noteNewStart = juce::jmax(0.0, juce::jmin(snapshot.startBeat + startDelta, noteOriginalEnd - kSnapBeats));
            note.startBeat = noteNewStart;
            note.lengthBeats = noteOriginalEnd - noteNewStart;
        }
        repaint();
    }
    else if (_dragMode == DragMode::MarqueeSelect)
    {
        _marqueeRect = juce::Rectangle<float>(_dragStartMouse, position);
        repaint();
    }
    else if (_dragMode == DragMode::ResizeLoopStart)
    {
        const auto rawStart = _dragStartBeat + deltaBeats;
        const auto snappedStart = snap ? snapBeat(rawStart) : rawStart;
        _loopStartBeat = juce::jlimit(0.0, _loopEndBeat - kSnapBeats, snappedStart); // can't pass beat 0, or the loop end
        repaint();
    }
    else if (_dragMode == DragMode::ResizeLoopEnd)
    {
        const auto rawEnd = _dragStartBeat + deltaBeats;
        const auto snappedEnd = snap ? snapBeat(rawEnd) : rawEnd;
        _loopEndBeat = juce::jmax(_loopStartBeat + kSnapBeats, snappedEnd); // no upper bound
        repaint();
    }
}

void MidiEditor::updateHoverState(juce::Point<float> position)
{
    const auto noteIndex = hitTestNote(position);
    const auto isLeftResize = noteIndex >= 0 && isInNoteResizeZone(noteIndex, position, true);
    const auto isResize = noteIndex >= 0 && (isLeftResize || isInNoteResizeZone(noteIndex, position, false));
    // hitTestChordBlock already returns -1 outside the lane, so this is naturally -1 whenever a note
    // was already hit above (the lane and the note grid never overlap spatially anyway).
    const auto chordBlockIndex = hitTestChordBlock(position);

    if (noteIndex == _hoveredNoteIndex && isResize == _hoveredIsResizeZone && isLeftResize == _hoveredResizeIsLeftEdge
        && chordBlockIndex == _hoveredChordBlockIndex)
        return;

    _hoveredNoteIndex = noteIndex;
    _hoveredIsResizeZone = isResize;
    _hoveredResizeIsLeftEdge = isLeftResize;
    _hoveredChordBlockIndex = chordBlockIndex;

    if (noteIndex >= 0)
        setMouseCursor(isResize ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::DraggingHandCursor);
    else if (chordBlockIndex >= 0)
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    else
        setMouseCursor(juce::MouseCursor::NormalCursor);

    repaint();
}

void MidiEditor::refreshScrollRanges()
{
    auto furthestBeat = kDefaultContentBars * kBeatsPerBar;
    for (const auto& note : _notes)
        furthestBeat = juce::jmax(furthestBeat, note.startBeat + note.lengthBeats);
    for (const auto& block : _chordBlocks)
        furthestBeat = juce::jmax(furthestBeat, block.startBeat + block.lengthBeats);
    const auto contentBeats = furthestBeat + 4.0 * kBeatsPerBar;

    const auto visibleBeats = _pixelsPerBeat > 0.f ? static_cast<double>(_contentArea.getWidth() / _pixelsPerBeat) : 0.0;
    _scrollBeat = juce::jlimit(0.0, juce::jmax(0.0, contentBeats - visibleBeats), _scrollBeat);
    _hScrollBar.setRangeLimits(0.0, contentBeats, juce::dontSendNotification);
    _hScrollBar.setCurrentRange(_scrollBeat, visibleBeats, juce::dontSendNotification);

    const auto visibleRows = _rowHeight > 0.f ? _contentArea.getHeight() / _rowHeight : 0.f;
    _scrollRow = juce::jlimit(0.f, juce::jmax(0.f, static_cast<float>(kNumPitchRows) - visibleRows), _scrollRow);
    _vScrollBar.setRangeLimits(0.0, static_cast<double>(kNumPitchRows), juce::dontSendNotification);
    _vScrollBar.setCurrentRange(static_cast<double>(_scrollRow), static_cast<double>(visibleRows), juce::dontSendNotification);

    updateScrollBarVisibility();
}

void MidiEditor::updateScrollBarVisibility()
{
    // Matches VoicingSelector::updateScrollBarVisibility exactly: hidden unless both hovering and
    // actually needed - re-evaluated here too (not just from mouseEnter/mouseExit) since zoom or
    // content growth/shrinkage can flip "needed" without any hover-state change at all.
    const auto needsHorizontalScroll = _hScrollBar.getCurrentRangeSize() < _hScrollBar.getMaximumRangeLimit() - _hScrollBar.getMinimumRangeLimit();
    const auto needsVerticalScroll = _vScrollBar.getCurrentRangeSize() < _vScrollBar.getMaximumRangeLimit() - _vScrollBar.getMinimumRangeLimit();

    _hScrollBar.setVisible(_isHovering && needsHorizontalScroll);
    _vScrollBar.setVisible(_isHovering && needsVerticalScroll);
}

void MidiEditor::zoomHorizontal(float factor, juce::Point<float> anchor)
{
    const auto anchorBeat = xToBeat(anchor.x);
    _pixelsPerBeat = juce::jlimit(kMinPixelsPerBeat, kMaxPixelsPerBeat, _pixelsPerBeat * factor);
    _scrollBeat = juce::jmax(0.0, anchorBeat - static_cast<double>((anchor.x - kGutterWidth) / _pixelsPerBeat));
    refreshScrollRanges();
    repaint();
}

void MidiEditor::zoomVertical(float factor, juce::Point<float> anchor)
{
    const auto anchorRow = _scrollRow + (anchor.y - kRulerHeight) / _rowHeight;
    _rowHeight = juce::jlimit(kMinRowHeight, kMaxRowHeight, _rowHeight * factor);
    _scrollRow = juce::jmax(0.f, anchorRow - (anchor.y - kRulerHeight) / _rowHeight);
    refreshScrollRanges();
    repaint();
}

void MidiEditor::zoomToLoop()
{
    const auto loopLengthBeats = _loopEndBeat - _loopStartBeat;
    if (loopLengthBeats <= 0.0 || _contentArea.getWidth() <= 0.f)
        return;

    _pixelsPerBeat = juce::jlimit(kMinPixelsPerBeat, kMaxPixelsPerBeat, _contentArea.getWidth() / static_cast<float>(loopLengthBeats));
    _scrollBeat = _loopStartBeat;

    refreshScrollRanges();
    repaint();
}

}
