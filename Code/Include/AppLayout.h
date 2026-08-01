#pragma once

#include <optional>
#include <unordered_map>

#include <nierika_dsp/nierika_dsp.h>

#include "Audio/ChordSynthEngine.h"
#include "Component/ChordDegreeBrowser.h"
#include "Component/KeyScaleSelector.h"
#include "Component/ProgressionSequencer.h"
#include "Component/SettingsWindow.h"
#include "Component/SynthEditor.h"
#include "Component/VoicingSelector.h"
#include "Theory/Degree.h"

class AppLayout final : public nlayout::AppLayout,
                         public nelement::SVGButton::OnClickListener,
                         public component::KeyScaleSelector::Listener,
                         public component::ChordDegreeBrowser::Listener,
                         public component::ProgressionSequencer::Listener,
                         public nui::Section::OnPanelChangedListener
{
public:
    AppLayout(ndsp::ParameterManager& parameterManager, audio::ChordSynthEngine& synthEngine);
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

    void onSlotFileDropped(int slotIndex, const juce::String& filePath) override;
    void onProgressionDragStarted() override;
    void onSlotsChanged() override;
    void onChordPreviewRequested(const theory::Chord& chord) override;

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

    // Rebuilds the "ChordsTheoryState" child of _parameterManager.getState().state from the
    // current UI state - called after any change so the state is always current even if the
    // editor later closes (getStateInformation serializes the whole tree, including this child,
    // regardless of whether an editor exists at save time).
    void syncStateToValueTree();

    // Reads the "ChordsTheoryState" child back out (if present - absent on a fresh/never-saved
    // instance) and applies it to the UI. Called once, at construction.
    void restoreStateFromValueTree();

    audio::ChordSynthEngine& _synthEngine;

    nelement::SVGButton _settings;
    component::KeyScaleSelector _keyScaleSelector;
    component::ChordDegreeBrowser _chordBrowser;
    component::VoicingSelector _voicingSelector { "voicing-selector" };
    component::ProgressionSequencer _progressionSequencer;
    component::SynthEditor _synthEditor;

    // Owns the "Chords"/"Synth" tab switcher - everything above except _settings/_keyScaleSelector
    // (row 0, always visible) lives inside one of its two panels. A *new*, nested Section, not a
    // reuse of this class's own inherited nlayout::AppLayout/Section grid (which is what row 0
    // itself lives in) - see AppLayout.cpp for why tabbing the root directly would also hide row 0.
    nui::Section _mainSection;

    nlayout::WindowsManager _windowsManager;

    // tempFilePath -> degree, populated in onChordDragStarted() just before starting the OS-level
    // file drag, consulted by onSlotFileDropped() when that same file lands on a progression slot
    // inside this same window - see MidiExporter/ProgressionSlotView for the rest of the mechanism.
    std::unordered_map<juce::String, theory::Degree> _inFlightChordDrags;

    // Which degree the voicing selector is currently showing, if open - used to re-derive the
    // arrow-target x on resize and to know what to clear when the key/scale changes underneath it.
    std::optional<theory::Degree> _openVoicingDegree;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AppLayout)
};
