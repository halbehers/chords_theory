#pragma once

#include <functional>
#include <optional>
#include <vector>

#include <nierika_dsp/nierika_dsp.h>

#include "Component/MidiEditor.h"
#include "Component/ProgressionDragHandle.h"
#include "Component/ProgressionPresetPicker.h"
#include "Theory/Chord.h"
#include "Theory/ProgressionPreset.h"
#include "Theory/ProgressionSlot.h"
#include "Theory/Scale.h"

namespace component
{

// The progression header (preset picker/save, whole-progression drag-to-DAW handle) plus the new
// MidiEditor piano-roll surface below it. The old discrete "one box per bar" slot view
// (ProgressionSlotView/SlotsRow) has been replaced by MidiEditor's continuous note-level editing.
// _slotData/_slotOccupied remain as a headless (non-visual) data model purely because
// Tests/ProgressionPresetIntegrationTests.cpp and AppLayout's session-state sync/whole-progression
// export still depend on their exact current behavior (see setSlot/getPopulatedSlots/loadPreset) -
// MidiEditor's own notes/chord-lane blocks are a completely separate, not-yet-integrated data
// model (see MidiEditor.h); reconciling the two is deferred to a later "full integration" pass.
class ProgressionEditor : public nui::Component,
                           public MidiEditor::Listener,
                           public ProgressionPresetPicker::Listener,
                           public ProgressionDragHandle::Listener,
                           public nelement::SVGButton::OnClickListener
{
public:
    using ChordResolver = std::function<const theory::Chord*(const theory::ProgressionSlot&)>;

    struct Listener
    {
        virtual ~Listener() = default;

        // A ChordCard export was dropped on the MidiEditor at (unsnapped) startBeat - mirrors
        // MidiEditor::Listener::onChordFileDropped exactly; this class never resolves chord data
        // itself, it only bubbles the event up to its own owner (AppLayout).
        virtual void onChordFileDropped(double startBeat, const juce::String& filePath) = 0;

        // The user started dragging the "insert whole progression" handle.
        virtual void onProgressionDragStarted() = 0;

        // Fired after any slot's content changes for any reason (load, clear) - the owner uses
        // this to keep persisted session state in sync, rather than needing a separate hook per
        // mutation path.
        virtual void onSlotsChanged() = 0;
    };

    ProgressionEditor(const std::string& identifier, ChordResolver chordResolver);
    ~ProgressionEditor() override;

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

    // Stores slot exactly as given (including any popularityOrder pin) and marks the slot
    // occupied - the general form setSlotDegree wraps. Grows the progression (via addStep()) if
    // slotIndex is beyond the current step count, so restoring a session saved with more occupied
    // slots than a freshly-opened instance starts with never silently loses data.
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
    // removeLastSlot() (both now private - setSlot's auto-grow and loadPreset's grow/shrink are
    // the only remaining callers, since there's no longer a +/- button UI).
    [[nodiscard]] int getSlotCount() const { return static_cast<int>(_slotData.size()); }

    // Grows or trims the step count to exactly preset.slots.size() (never below the minimum) and
    // fills every slot from preset.slots, clearing any leftover slot beyond the loaded length.
    void loadPreset(const theory::ProgressionPreset& preset);

    // Thin forward to the owned MidiEditor's own addChordAtBeat() - the caller (AppLayout) resolves
    // a chord-file drop's Degree to a Chord itself, then hands it here; this class never reaches
    // past its own MidiEditor member, and AppLayout never reaches past this class.
    void addChordAtBeat(double startBeat, const theory::Chord& chord);

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

private:
    void onChordFileDropped(double startBeat, const juce::String& filePath) override;
    void onPresetSelected(const theory::ProgressionPreset& preset) override;
    void onProgressionDragStarted() override;
    void onButtonClick(const std::string& componentID) override;

    void addStep();
    void removeLastSlot();

    ChordResolver _chordResolver;
    theory::Scale _currentScale = theory::Scale::Major;

    nelement::Text _presetsLabel { "progression-presets-label" };
    ProgressionPresetPicker _presetPicker;
    nelement::SVGButton _savePresetButton;

    std::vector<theory::ProgressionSlot> _slotData;
    std::vector<bool> _slotOccupied;

    ProgressionDragHandle _dragHandle;
    MidiEditor _midiEditor { "progression-midi-editor" };

    nlayout::GridLayout<nui::Component> _layout { *this };

    std::vector<Listener*> _listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProgressionEditor)
};

}
