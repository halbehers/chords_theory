#pragma once

#include <functional>
#include <vector>

#include <nierika_dsp/nierika_dsp.h>

#include "Component/MidiEditor.h"
#include "Component/ProgressionDragHandle.h"
#include "Component/ProgressionPresetPicker.h"
#include "Theory/Chord.h"
#include "Theory/MidiEditorState.h"
#include "Theory/ProgressionPreset.h"
#include "Theory/ProgressionSlot.h"
#include "Theory/Scale.h"

namespace component
{

// The progression header (preset picker/save, whole-progression drag-to-DAW handle) plus the
// MidiEditor piano-roll surface below it. MidiEditor is the sole data model for the progression -
// presets load into it and save from it (see loadPreset/getPopulatedSlots), the drag handle exports
// its exact live content, and its pure-data snapshot (getMidiEditorState/restoreMidiEditorState)
// is what AppLayout persists as DAW-project session state.
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

        // Fired after any mutation to the MidiEditor's content (add/move/resize/delete, or a
        // preset load) - the owner uses this to keep persisted session state in sync, rather than
        // needing a separate hook per mutation path. Mirrors MidiEditor::Listener::onContentChanged.
        virtual void onContentChanged() = 0;
    };

    ProgressionEditor(const std::string& identifier, ChordResolver chordResolver);
    ~ProgressionEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // Refreshes the preset picker's filtered list for the new scale - call whenever Key/Scale
    // changes. Does NOT clear the MidiEditor's existing content (a key/scale change re-voices
    // existing chord blocks via the ChordResolver only at the next drop/preset-load, it doesn't
    // retroactively rewrite what's already been dropped - see MidiEditor.h's frozen-at-drop-time
    // note).
    void setScale(theory::Scale scale);

    // Clears the MidiEditor and places preset.slots[i]'s resolved chord at bar i, in order - an
    // unresolvable slot (e.g. a degree absent under the current scale) simply leaves its bar empty
    // rather than shifting later slots to fill the gap.
    void loadPreset(const theory::ProgressionPreset& preset);

    // The MidiEditor's chord blocks, sorted by startBeat, each reporting the ProgressionSlot it was
    // dropped/loaded with (frozen at that time - see MidiEditor.h) - used for "save as preset" and
    // was previously also used for the drag-handle export (now reads getMidiEditorState() instead,
    // for exact note-level fidelity).
    [[nodiscard]] std::vector<theory::ProgressionSlot> getPopulatedSlots() const;

    // Thin forward to the owned MidiEditor's own addChordAtBeat() - the caller (AppLayout) resolves
    // a chord-file drop's Degree to a Chord itself, then hands it here; this class never reaches
    // past its own MidiEditor member, and AppLayout never reaches past this class.
    void addChordAtBeat(double startBeat, const theory::Chord& chord, const theory::ProgressionSlot& sourceSlot);

    // Pure-data snapshot of the MidiEditor's content, and the inverse - used by AppLayout to
    // persist/restore DAW-project session state without reaching past this class into MidiEditor
    // directly.
    [[nodiscard]] theory::MidiEditorState getMidiEditorState() const { return _midiEditor.getState(); }
    void restoreMidiEditorState(const theory::MidiEditorState& state) { _midiEditor.restoreState(state); }

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

private:
    void onChordFileDropped(double startBeat, const juce::String& filePath) override;
    void onContentChanged() override;
    void onPresetSelected(const theory::ProgressionPreset& preset) override;
    void onProgressionDragStarted() override;
    void onButtonClick(const std::string& componentID) override;

    ChordResolver _chordResolver;
    theory::Scale _currentScale = theory::Scale::Major;

    nelement::Text _presetsLabel { "progression-presets-label" };
    ProgressionPresetPicker _presetPicker;
    nelement::SVGButton _savePresetButton;

    ProgressionDragHandle _dragHandle;
    MidiEditor _midiEditor { "progression-midi-editor" };

    nlayout::GridLayout<nui::Component> _layout { *this };

    std::vector<Listener*> _listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProgressionEditor)
};

}
