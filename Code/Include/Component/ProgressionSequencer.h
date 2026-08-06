#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include <nierika_dsp/nierika_dsp.h>

#include "Component/ProgressionDragHandle.h"
#include "Component/ProgressionPresetPicker.h"
#include "Component/ProgressionSlotView.h"
#include "Theory/Chord.h"
#include "Theory/ProgressionPreset.h"
#include "Theory/ProgressionSlot.h"
#include "Theory/Scale.h"

namespace component
{

// The progression-building timeline: a preset picker, a horizontally-scrollable row of drop-target
// slots (dragging a ChordCard from ChordDegreeBrowser onto any slot - empty or occupied - replaces
// its chord) with "+"/"-" controls to grow/shrink the step count (no upper limit - the row scrolls
// once it no longer fits), a "save as preset" affordance, and a drag handle to export the whole
// populated sequence as one MIDI clip. Never resolves chord data itself - always goes through the
// ChordResolver supplied at construction (AppLayout wires this to ChordDegreeBrowser::resolveSlot),
// so an unpinned slot reflects whatever voicing the user currently has selected for its degree,
// while a slot loaded from a user preset (see setSlot/loadPreset) resolves to its pinned voicing
// instead.
class ProgressionSequencer : public nui::Component,
                              public ProgressionSlotView::Listener,
                              public ProgressionPresetPicker::Listener,
                              public ProgressionDragHandle::Listener,
                              public nelement::SVGButton::OnClickListener
{
public:
    using ChordResolver = std::function<const theory::Chord*(const theory::ProgressionSlot&)>;

    struct Listener
    {
        virtual ~Listener() = default;

        // A ChordCard export was dropped on slotIndex - the owner resolves filePath against its
        // in-flight drag map and calls setSlotDegree() back with the result.
        virtual void onSlotFileDropped(int slotIndex, const juce::String& filePath) = 0;

        // The user started dragging the "insert whole progression" handle.
        virtual void onProgressionDragStarted() = 0;

        // Fired after any slot's content changes for any reason (drop, preset load, clear) - the
        // owner uses this to keep persisted session state in sync, rather than needing a separate
        // hook per mutation path.
        virtual void onSlotsChanged() = 0;

        // The user clicked a populated slot - already resolved to its actual chord via
        // _chordResolver, since a slot's "identity" for preview purposes is just its chord.
        virtual void onChordPreviewRequested(const theory::Chord& chord) = 0;
    };

    ProgressionSequencer(const std::string& identifier, ChordResolver chordResolver);
    ~ProgressionSequencer() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // Refreshes the preset picker's filtered list for the new scale - call whenever Key/Scale
    // changes. Does NOT clear existing slots (a key/scale change re-voices existing degrees via
    // the ChordResolver automatically on next repaint/export, it doesn't invalidate the sequence).
    void setScale(theory::Scale scale);

    // Degree-only, unpinned - used for dragging a ChordCard from the browser onto a slot, which
    // keeps tracking that degree's live voicing (same as today). For a fully-specified slot
    // (e.g. loading a preset that may pin a popularityOrder), use setSlot.
    void setSlotDegree(int slotIndex, theory::Degree degree);

    // Stores slot exactly as given (including any popularityOrder pin), marks the slot occupied,
    // refreshes its view, and notifies listeners - the general form setSlotDegree wraps. Grows the
    // progression (via addStep()) if slotIndex is beyond the current step count, so restoring a
    // session saved with more occupied slots than a freshly-opened sequencer starts with never
    // silently loses data.
    void setSlot(int slotIndex, const theory::ProgressionSlot& slot);

    void clearSlot(int slotIndex);

    // Removes slotIndex (must currently be occupied - a no-op otherwise), shifts every later slot
    // back by one position so the progression stays packed with no gap left behind, and shrinks
    // the step count by one (unless already at the minimum, in which case the freed slot is just
    // cleared in place).
    void removeSlotAndShift(int slotIndex);

    // nullopt if slotIndex is out of range or empty - used for exact-position session-state persistence.
    [[nodiscard]] std::optional<theory::Degree> getSlotDegree(int slotIndex) const;

    // Populated slots only, in slot order (gaps skipped) - used for export and "save as preset".
    [[nodiscard]] std::vector<theory::ProgressionSlot> getPopulatedSlots() const;

    // Current number of steps in the progression - unbounded, grows/shrinks via addStep()/
    // removeLastSlot() (and their UI-driven counterparts, the "+"/"-" buttons).
    [[nodiscard]] int getSlotCount() const { return static_cast<int>(_slots.size()); }

    // Appends one empty step at the end of the row, scrolling to reveal it.
    void addStep();

    // Removes the trailing step (a no-op once down to the minimum step count).
    void removeLastSlot();

    // Grows or trims the step count to exactly preset.slots.size() (never below the minimum) and
    // fills every slot from preset.slots, clearing any leftover slot beyond the loaded length.
    void loadPreset(const theory::ProgressionPreset& preset);

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

private:
    // Hosts the scrollable strip of ProgressionSlotViews plus the trailing "+"/"-" buttons inside
    // _slotsViewport. Implements FileDragAndDropTarget itself so a drop landing anywhere in the
    // row that isn't caught by a more specific child (e.g. on/near the +/- buttons, or in the gap
    // after the last slot) still resolves to a sensible target - JUCE's drag-target resolution
    // walks up the component hierarchy from the deepest hit-tested component, so individual
    // ProgressionSlotViews (already FileDragAndDropTargets) keep first claim on drops landing
    // directly on them.
    class SlotsRow : public nui::Component, public juce::FileDragAndDropTarget
    {
    public:
        explicit SlotsRow(ProgressionSequencer& owner);

        bool isInterestedInFileDrag(const juce::StringArray& files) override;
        void fileDragEnter(const juce::StringArray& files, int x, int y) override;
        void fileDragExit(const juce::StringArray& files) override;
        void filesDropped(const juce::StringArray& files, int x, int y) override;

    private:
        ProgressionSequencer& _owner;
    };

    void onFileDropped(int slotIndex, const juce::String& filePath) override;
    void onSlotPreviewRequested(int slotIndex) override;
    void onSlotRemoveRequested(int slotIndex) override;
    void onPresetSelected(const theory::ProgressionPreset& preset) override;
    void onProgressionDragStarted() override;
    void onButtonClick(const std::string& componentID) override;

    // A file was dropped somewhere in _slotsRow that wasn't claimed by a specific slot - placed at
    // the first unoccupied slot, or appended as a new step if the progression is fully occupied.
    void onSlotsRowFileDropped(const juce::String& filePath);

    void refreshSlotView(int slotIndex);
    void layoutSlotsRow();

    ChordResolver _chordResolver;
    theory::Scale _currentScale = theory::Scale::Major;

    nelement::Text _presetsLabel { "progression-presets-label" };
    ProgressionPresetPicker _presetPicker;
    nelement::SVGButton _savePresetButton;

    juce::Viewport _slotsViewport;
    SlotsRow _slotsRow { *this };
    std::vector<std::unique_ptr<ProgressionSlotView>> _slots;
    std::vector<theory::ProgressionSlot> _slotData;
    std::vector<bool> _slotOccupied;

    nelement::SVGButton _addStepButton { "progression-add-step", nui::Icons::getPlus() };
    nelement::SVGButton _removeStepButton { "progression-remove-step", nui::Icons::getMinus() };

    ProgressionDragHandle _dragHandle;

    nlayout::GridLayout<nui::Component> _layout { *this };

    std::vector<Listener*> _listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProgressionSequencer)
};

}
