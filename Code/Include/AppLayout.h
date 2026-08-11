#pragma once

#include <optional>
#include <unordered_map>

#include <nierika_dsp/nierika_dsp.h>

#include "PluginProcessor.h"
#include "Component/ChordDegreeBrowser.h"
#include "Component/DragOutButton.h"
#include "Component/KeyScaleSelector.h"
#include "Component/ProgressionEditor.h"
#include "Component/ProgressionTimeline.h"
#include "Component/SettingsWindow.h"
#include "Component/SynthEditor.h"
#include "Component/VoicingSelector.h"
#include "Theory/Degree.h"

class AppLayout final : public nlayout::AppLayout,
                         public nelement::SVGButton::OnClickListener,
                         public component::KeyScaleSelector::Listener,
                         public component::ChordDegreeBrowser::Listener,
                         public component::ProgressionEditor::Listener,
                         public component::ProgressionTimeline::Listener,
                         public component::DragOutButton::Listener,
                         public component::VoicingSelector::Listener,
                         public nui::Section::OnPanelChangedListener
{
public:
    AppLayout(ndsp::ParameterManager& parameterManager, PluginAudioProcessor& audioProcessor);
    ~AppLayout() override;

    void resized() override;

    void onButtonClick(const std::string& componentID) override;

private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    void onKeyScaleChanged(theory::Key key, theory::Scale scale) override;
    void onChordChanged(theory::Degree degree, const theory::Chord& newChord) override;
    void onChordDragStarted(theory::Degree degree, const theory::Chord& chord) override;
    void onChordPreviewRequested(theory::Degree degree, const theory::Chord& chord) override;
    void onVoicingSelectorRequested(theory::Degree degree, const std::vector<theory::Chord>& availableVoicings, const std::string& currentSymbol) override;
    void onVoicingSelectorClosed() override;

    void onChordFileDropped(double startBeat, const juce::String& filePath) override;
    void onProgressionDragStarted() override;
    void onContentChanged() override;
    void onPlaybackStateChanged(bool isPlaying) override;

    // _dragOutButton's (DragOutButton::Listener) own gesture - forwards to the same whole-
    // progression export onProgressionDragStarted() above already performs, so both drag-out
    // affordances (this Synth-tab button and _progressionEditor's own header one) share one
    // implementation.
    void onDragStarted() override;

    // The user dragged a single chord segment out of _progressionTimeline OR out of the MidiEditor's
    // own chord lane (component::ProgressionEditor::Listener and component::ProgressionTimeline::
    // Listener declare the identical pure virtual, so this one override satisfies both) - resolves
    // the index back to its own notes (normalized to start at beat 0, so the exported clip plays
    // immediately rather than starting with however much silent lead-in it had in the original
    // progression) and performs the same OS-level file drag onChordDragStarted/
    // onProgressionDragStarted already use.
    void onChordBlockDragStarted(int chordBlockIndex) override;

    // _mainSection's own panel-switch mechanism (GridLayout::setVisible()) unconditionally shows
    // every component registered in a panel the moment it becomes active again - including
    // _voicingSelector, which has its own independent open/closed state unrelated to which tab is
    // showing. Re-asserts that real state whenever the Chords tab becomes active again, so closing
    // the voicing selector then switching to Synth and back doesn't silently reopen it.
    void onPanelChanged(const std::string& newPanelID) override;

    // Shared by both onChordPreviewRequested overrides above.
    void previewChord(const theory::Chord& chord);

    // Re-derives the voicing selector's arrow-target x from the currently open degree's card -
    // called after the layout changes for any reason (new degree opened, or a resize while
    // already open). No-op while the selector is closed.
    void updateVoicingSelectorArrow();

    void setVoicingVisibility(bool isVisible);

    // Rebuilds the "ChordsTheoryState" child of _parameterManager.getState().state from the
    // current UI state - called after any change so the state is always current even if the
    // editor later closes (getStateInformation serializes the whole tree, including this child,
    // regardless of whether an editor exists at save time).
    void syncStateToValueTree();

    // Reads the "ChordsTheoryState" child back out (if present - absent on a fresh/never-saved
    // instance) and applies it to the UI. Called once, at construction.
    void restoreStateFromValueTree();

    PluginAudioProcessor& _audioProcessor;

    nelement::SVGButton _settings;
    component::KeyScaleSelector _keyScaleSelector;
    component::ChordDegreeBrowser _chordBrowser;
    component::VoicingSelector _voicingSelector { "voicing-selector" };
    component::ProgressionEditor _progressionEditor;
    // Declared after _progressionEditor - its constructor registers itself as a listener on it, so
    // _progressionEditor must already be constructed (C++ initializes members in declaration order).
    component::ProgressionTimeline _progressionTimeline;
    // Mirrors _progressionEditor's own play button - lets playback be started/stopped from the
    // Synth tab too, driven through the same MidiEditor::startPlayback()/stopPlayback() path (see
    // ProgressionEditor's forwarding isPlaying()/startPlayback()/stopPlayback()), so both buttons
    // stay in sync via onPlaybackStateChanged regardless of which one was clicked.
    nelement::SVGButton _synthPlayButton { "synth-play-button", nui::Icons::getPlay() };
    component::DragOutButton _dragOutButton { "drag-out-button" };
    component::SynthEditor _synthEditor;

    // Owns the "Chords"/"Synth" tab switcher - everything above except _settings/_keyScaleSelector
    // (row 0, always visible) lives inside one of its two panels. A *new*, nested Section, not a
    // reuse of this class's own inherited nlayout::AppLayout/Section grid (which is what row 0
    // itself lives in) - see AppLayout.cpp for why tabbing the root directly would also hide row 0.
    nui::Section _mainSection;

    nlayout::WindowsManager _windowsManager;

    // tempFilePath -> degree, populated in onChordDragStarted() just before starting the OS-level
    // file drag, consulted by onChordFileDropped() when that same file lands on the MidiEditor
    // inside this same window - see MidiExporter/MidiEditor for the rest of the mechanism.
    std::unordered_map<juce::String, theory::Degree> _inFlightChordDrags;

    // Which degree the voicing selector is currently showing, if open - used to re-derive the
    // arrow-target x on resize and to know what to clear when the key/scale changes underneath it.
    std::optional<theory::Degree> _openVoicingDegree;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AppLayout)
};
