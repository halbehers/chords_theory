#pragma once

#include <optional>
#include <vector>

#include <nierika_dsp/nierika_dsp.h>

#include "Theory/Chord.h"

namespace component
{

// A piano-roll MIDI note editor: a scrollable/zoomable pitch grid (pitch-labeled gutter on the
// left, beat/bar ruler on top) with a "chord lane" strip along the bottom. Dropping a ChordCard's
// exported .mid file onto it (see AppLayout's in-flight-drag-map resolution, the same mechanism
// ProgressionSlotView used) adds a labeled chord block to the lane and splits the chord into
// individually movable/resizable note blocks in the grid above, via addChordAtBeat(). Owns its own
// in-memory note/chord-block state - deliberately not wired into Theory::SessionState/MidiExporter/
// ProgressionPresetLibrary yet (a later pass), matching ProgressionEditor's own current scope.
//
// Hand-paints everything itself (no juce::Viewport) - there's no existing precedent in this
// codebase or nierika_dsp for a Viewport scrolling on both axes with a frozen gutter/ruler synced
// against it, and hand-painting sidesteps that entirely: the gutter/ruler are simply drawn last, at
// screen-fixed rects regardless of scroll offset. Two real juce::ScrollBars are still used (not
// wrapped in a Viewport) purely for their native thumb/drag/click-to-page UI, driving plain
// scroll-offset member state directly.
class MidiEditor : public nui::Component,
                    public juce::FileDragAndDropTarget,
                    public juce::ScrollBar::Listener,
                    private juce::Timer
{
public:
    struct Listener
    {
        virtual ~Listener() = default;

        // Mirrors ProgressionSlotView::Listener::onFileDropped exactly, beat- not slot-indexed:
        // the owner resolves filePath via its in-flight drag map and calls addChordAtBeat() back
        // with the result - this class never resolves chord data itself.
        virtual void onChordFileDropped(double startBeat, const juce::String& filePath) = 0;
    };

    explicit MidiEditor(const std::string& identifier);
    ~MidiEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // Snaps to the whole-bar (4-beat) cell startBeat falls in, then splits chord into N note
    // blocks (via theory::NoteConvertor::voiceChordCloseToMiddleC) plus one chord-lane block
    // (chord.readableName), spanning whatever's actually free in that cell: the full bar if
    // empty, the remaining gap if another block partially overlaps it, or the full bar again
    // (replacing that block and the notes it originally created) if another block already fully
    // occupies it. A no-op if the cell has no free room at all.
    void addChordAtBeat(double startBeat, const theory::Chord& chord);

    [[nodiscard]] int getNoteCount() const { return static_cast<int>(_notes.size()); }
    [[nodiscard]] int getChordBlockCount() const { return static_cast<int>(_chordBlocks.size()); }
    [[nodiscard]] std::optional<int> getNoteMidiPitch(int index) const;
    [[nodiscard]] std::optional<double> getNoteStartBeat(int index) const;
    [[nodiscard]] std::optional<double> getNoteLengthBeats(int index) const;
    [[nodiscard]] std::optional<double> getChordBlockStartBeat(int index) const;
    [[nodiscard]] std::optional<double> getChordBlockLengthBeats(int index) const;

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    void scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart) override;

    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify(const juce::MouseEvent&, float scaleFactor) override;

private:
    struct MidiNoteBlock
    {
        int midiNote = 60;
        double startBeat = 0.0;
        double lengthBeats = 1.0;
        int sourceChordId = -1; // which addChordAtBeat() call created this note; -1 = created
                                 // directly (double-click) or its origin chord block was replaced
    };

    struct ChordBlockData
    {
        int id = -1;             // stable id from a monotonic counter - exists only so a later
                                  // full-beat chord drop can find and remove this block's own
                                  // notes when replacing it; not a live editing link
        std::string label;       // frozen snapshot of Chord::readableName at drop time
        double startBeat = 0.0;
        double lengthBeats = 1.0;
    };

    enum class DragMode { None, MoveNote, ResizeNoteStart, ResizeNoteEnd, MoveChordBlock };

    void timerCallback() override; // drag-triggered auto-scroll only

    // paint helpers
    void paintGridlines(juce::Graphics&) const;
    void paintChordLane(juce::Graphics&) const;
    void paintNotes(juce::Graphics&) const;
    void paintRuler(juce::Graphics&) const;
    void paintGutter(juce::Graphics&) const;

    // coordinate math
    [[nodiscard]] float beatToX(double beat) const noexcept;
    [[nodiscard]] double xToBeat(float x) const noexcept;
    [[nodiscard]] float pitchToY(int midiNote) const noexcept;
    [[nodiscard]] int yToPitch(float y) const noexcept;
    [[nodiscard]] static double snapBeat(double beat) noexcept;

    // hit-testing
    [[nodiscard]] int hitTestNote(juce::Point<float>) const;
    [[nodiscard]] int hitTestChordBlock(juce::Point<float>) const;
    [[nodiscard]] bool isInNoteResizeZone(int noteIndex, juce::Point<float>, bool leftEdge) const;

    // A chord-lane block visually/logically stays exactly as long as the longest of the notes that
    // still carry its id (falls back to its own stored lengthBeats if none remain, e.g. every note
    // it created was individually deleted) - block.lengthBeats itself is never mutated after
    // creation, this is computed fresh everywhere the block's effective length matters (painting,
    // hit-testing, collision detection against new drops).
    [[nodiscard]] double effectiveChordBlockLength(const ChordBlockData& block) const;

    // gestures
    void applyDragAt(juce::Point<float> position);
    void updateHoverState(juce::Point<float> position);

    // scroll/zoom
    void refreshScrollRanges();
    void zoomHorizontal(float amount, juce::Point<float> anchor);
    void zoomVertical(float amount, juce::Point<float> anchor);
    void updateScrollBarVisibility();

    std::vector<MidiNoteBlock> _notes;
    std::vector<ChordBlockData> _chordBlocks;
    int _nextChordBlockId = 0;
    std::vector<Listener*> _listeners;

    juce::ScrollBar _hScrollBar { false };
    juce::ScrollBar _vScrollBar { true };
    juce::Rectangle<float> _contentArea;

    double _scrollBeat = 0.0;
    float _scrollRow = 0.f; // set from kMaxMidiNote - kInitialTopMidiNote in the constructor
    float _pixelsPerBeat = 0.f; // set from kDefaultPixelsPerBeat in the constructor
    float _rowHeight = 0.f;     // set from kDefaultRowHeight in the constructor

    DragMode _dragMode = DragMode::None;
    int _draggedNoteIndex = -1;
    int _draggedChordIndex = -1;
    juce::Point<float> _dragStartMouse;
    juce::Point<float> _lastMousePosition;
    double _dragStartBeat = 0.0;
    double _dragStartLengthBeats = 0.0;
    int _dragStartMidiNote = 60;

    int _hoveredNoteIndex = -1;
    bool _hoveredIsResizeZone = false;
    bool _hoveredResizeIsLeftEdge = false;
    bool _isHovering = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiEditor)
};

}
