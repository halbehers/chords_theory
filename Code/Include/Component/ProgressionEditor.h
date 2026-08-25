#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <nierika_dsp/nierika_dsp.h>

#include "Audio/ProgressionPlayer.h"
#include "Component/DragOutButton.h"
#include "Component/MidiEditor.h"
#include "Component/ProgressionPresetPicker.h"
#include "Theory/Chord.h"
#include "Theory/Key.h"
#include "Theory/MidiEditorState.h"
#include "Theory/ProgressionPreset.h"
#include "Theory/ProgressionSlot.h"
#include "Theory/Scale.h"

namespace component
{

// The progression header (preset picker/save, whole-progression drag-to-DAW button) plus the
// MidiEditor piano-roll surface below it. MidiEditor is the sole data model for the progression -
// presets load into it and save from it (see loadPreset/getPopulatedSlots), the drag-out button
// exports its exact live content, and its pure-data snapshot (getMidiEditorState/
// restoreMidiEditorState) is what AppLayout persists as DAW-project session state.
class ProgressionEditor : public nui::Component,
                           public MidiEditor::Listener,
                           public ProgressionPresetPicker::Listener,
                           public DragOutButton::Listener,
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

        // The user started dragging the "insert whole progression" button.
        virtual void onProgressionDragStarted() = 0;

        // Fired after any mutation to the MidiEditor's content (add/move/resize/delete, or a
        // preset load) - the owner uses this to keep persisted session state in sync, rather than
        // needing a separate hook per mutation path. Mirrors MidiEditor::Listener::onContentChanged.
        virtual void onContentChanged() = 0;

        // Fired whenever the owned MidiEditor's playback starts/stops - mirrors
        // MidiEditor::Listener::onPlaybackStateChanged exactly. Lets any number of other UI surfaces
        // (e.g. a play/stop button and a live playhead display elsewhere in the app) stay in sync
        // with the single MidiEditor that's actually the source of truth for playback state.
        virtual void onPlaybackStateChanged(bool isPlaying) = 0;

        // The user started dragging chordBlockIndex's segment out of the owned MidiEditor's own
        // chord lane - mirrors MidiEditor::Listener::onChordBlockDragStarted exactly, same index
        // space as getChordBlockStartBeat/getChordBlockLengthBeats/getChordBlockLabel above.
        virtual void onChordBlockDragStarted(int chordBlockIndex) = 0;
    };

    // progressionPlayer is nullable (defaults to null so any test constructing without one keeps
    // working) - forwarded straight into the owned MidiEditor.
    ProgressionEditor(const std::string& identifier, ChordResolver chordResolver, audio::ProgressionPlayer* progressionPlayer = nullptr);
    ~ProgressionEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // Refreshes the preset picker's filtered list for the new scale, and forwards both Key and Scale
    // to the owned MidiEditor so its auto-detected chord lane re-labels against the new context -
    // call whenever Key/Scale changes. Does NOT otherwise touch the MidiEditor's existing note
    // content.
    void setKeyAndScale(theory::Key key, theory::Scale scale);

    // Clears the MidiEditor and places preset.slots[i]'s resolved chord at bar i, in order - an
    // unresolvable slot (e.g. a degree absent under the current scale) simply leaves its bar empty
    // rather than shifting later slots to fill the gap.
    void loadPreset(const theory::ProgressionPreset& preset);

    // The MidiEditor's chord blocks, sorted by startBeat, each reporting the ProgressionSlot
    // currently detected from its notes (see MidiEditor.h/ChordIdentifier) - used for "save as
    // preset" and was previously also used for the drag-out button's export (now reads
    // getMidiEditorState() instead, for exact note-level fidelity).
    [[nodiscard]] std::vector<theory::ProgressionSlot> getPopulatedSlots() const;

    // Thin forward to the owned MidiEditor's own addChordAtBeat() - the caller (AppLayout) resolves
    // a chord-file drop's Degree to a Chord itself, then hands it here; this class never reaches
    // past its own MidiEditor member, and AppLayout never reaches past this class.
    void addChordAtBeat(double startBeat, const theory::Chord& chord);

    // Pure-data snapshot of the MidiEditor's content, and the inverse - used by AppLayout to
    // persist/restore DAW-project session state without reaching past this class into MidiEditor
    // directly.
    [[nodiscard]] theory::MidiEditorState getMidiEditorState() const { return _midiEditor.getState(); }
    void restoreMidiEditorState(const theory::MidiEditorState& state) { _midiEditor.restoreState(state); }

    // Thin forwards to the owned MidiEditor's own chord-block accessors, for other components (e.g.
    // a mini progression timeline elsewhere in the app) that want to display the current chord lane
    // without reaching past this class into MidiEditor directly (see MidiEditor.h's own comment on
    // that invariant). getChordBlockSlot isn't forwarded - nothing outside this class needs it yet.
    [[nodiscard]] int getChordBlockCount() const { return _midiEditor.getChordBlockCount(); }
    [[nodiscard]] std::optional<double> getChordBlockStartBeat(int index) const { return _midiEditor.getChordBlockStartBeat(index); }
    [[nodiscard]] std::optional<double> getChordBlockLengthBeats(int index) const { return _midiEditor.getChordBlockLengthBeats(index); }
    [[nodiscard]] std::optional<std::string> getChordBlockLabel(int index) const { return _midiEditor.getChordBlockLabel(index); }

    // Thin forwards to the owned MidiEditor's own playback controls, so another play/stop button
    // elsewhere in the app (see AppLayout's Synth-tab one) drives playback through the same path
    // this class's own _playButton already uses - never call audio::ProgressionPlayer::play()/stop()
    // directly from outside MidiEditor, that would bypass its note-buffer setup and skip the
    // onPlaybackStateChanged notification that keeps every listening button's icon in sync.
    [[nodiscard]] bool isPlaying() const { return _midiEditor.isPlaying(); }
    void startPlayback() { _midiEditor.startPlayback(); }
    void stopPlayback() { _midiEditor.stopPlayback(); }

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    void onChordFileDropped(double startBeat, const juce::String& filePath) override;
    void onContentChanged() override;
    void onPlaybackStateChanged(bool isPlaying) override;
    void onChordBlockDragStarted(int chordBlockIndex) override;
    void onPresetSelected(const theory::ProgressionPreset& preset) override;
    void onDragStarted() override; // DragOutButton::Listener - bubbles to _listeners' onProgressionDragStarted
    void onButtonClick(const std::string& buttonID) override;

    ChordResolver _chordResolver;
    theory::Key _currentKey = theory::Key::C;
    theory::Scale _currentScale = theory::Scale::Major;

    nelement::Text _presetsLabel { "progression-presets-label" };
    ProgressionPresetPicker _presetPicker;
    nelement::SVGButton _savePresetButton;
    nelement::SVGButton _playButton { "progression-play-button", nui::Icons::getPlay() };

    DragOutButton _dragOutButton;
    MidiEditor _midiEditor;

    nlayout::GridLayout<nui::Component> _layout { *this };

    std::vector<Listener*> _listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProgressionEditor)
};

}
