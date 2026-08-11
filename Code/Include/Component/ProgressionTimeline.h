#pragma once

#include <vector>

#include <nierika_dsp/nierika_dsp.h>

#include "Audio/ProgressionPlayer.h"
#include "Component/ProgressionEditor.h"

namespace component
{

// A small "what's playing right now" strip for the Synth tab's header row - shows the chord
// progression's currently-playing measure (4 bars at a time, segments proportionally sized by
// length) plus a moving playhead, mirroring MidiEditor's own "chord lane"/playhead visuals but
// without any scroll/zoom, since it only ever shows one measure at a time. Pagination is derived
// fresh from the live playhead beat on every paint() call (never cached), so it re-pages itself
// automatically the instant playback crosses a measure boundary (see kBeatsPerMeasure in the .cpp).
// Read-only in every other respect except one gesture: dragging a chord segment out exports just
// that chord's own notes as a temp MIDI file, same drag-to-DAW mechanism as ChordCard/DragOutButton
// (see Listener::onChordBlockDragStarted).
class ProgressionTimeline : public nui::Component,
                             public ProgressionEditor::Listener,
                             private juce::Timer
{
public:
    struct Listener
    {
        virtual ~Listener() = default;

        // The user started dragging chordBlockIndex's segment - past the minimum-distance
        // threshold, so this never fires for a plain click. The owner resolves the index back to
        // its notes (see ProgressionEditor::getChordBlockStartBeat/getChordBlockLengthBeats/
        // getMidiEditorState) and performs the actual OS-level drag itself - this component only
        // knows about chord-block data/timing, never about MIDI files or drag containers, same
        // division of responsibility as ChordCard.
        virtual void onChordBlockDragStarted(int chordBlockIndex) = 0;
    };

    // progressionPlayer is nullable (defaults to null, mirroring MidiEditor/ProgressionEditor's own
    // pattern) - every playhead-reading path in paint() treats null as "beat 0.0, nothing playing".
    ProgressionTimeline(const std::string& identifier, ProgressionEditor& progressionEditor,
                         audio::ProgressionPlayer* progressionPlayer = nullptr);
    ~ProgressionTimeline() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

    // Same HeightType convention as element::TextButton/element::Tabs/element::ComboBox elsewhere -
    // THIN/LARGE resolve to a fixed pixel height (see Theme::resolveHeight), and the strip is
    // vertically centred within whatever taller bounds its grid cell actually gives it, via
    // Component's own setMargin() (see applyHeightType() in the .cpp) - so paint()'s own geometry
    // never needs to know about HeightType at all, it just keeps reading getLocalBounds(). AUTO (the
    // default) fills the full given height, matching this component's original behaviour.
    void setHeightType(nui::Theme::HeightType type);
    [[nodiscard]] nui::Theme::HeightType getHeightType() const { return _heightType; }

private:
    void timerCallback() override;

    void onChordFileDropped(double, const juce::String&) override {}
    void onProgressionDragStarted() override {}
    void onContentChanged() override;
    void onPlaybackStateChanged(bool isPlaying) override;
    // ProgressionEditor::Listener's own onChordBlockDragStarted - fires for a drag gesture on the
    // underlying MidiEditor's own chord lane (a different widget entirely). Deliberately not
    // forwarded to this class's own _listeners - this component's mouseDown/mouseDrag above already
    // fire onChordBlockDragStarted (via ProgressionTimeline::Listener) for drags on ITS OWN segments;
    // forwarding this too would double-fire the same logical action whenever both widgets happen to
    // be visible/registered against the same ProgressionEditor at once.
    void onChordBlockDragStarted(int) override {}

    // Resolves _heightType against the component's current actual height and applies the
    // resulting top/bottom margin - called on construction, from resized() (bounds may have just
    // changed), and from setHeightType() (the type itself just changed).
    void applyHeightType();

    // Shared geometry - both paint() and the drag hit-test below derive the currently-displayed
    // measure and each beat's x position the same way, so there's exactly one place this math lives.
    // Not const - both read getLocalBounds()/getWidth(), which nui::Component doesn't declare const.
    [[nodiscard]] double getCurrentMeasureStart() const noexcept;
    [[nodiscard]] float beatToX(double beat) noexcept;
    [[nodiscard]] int hitTestChordBlock(juce::Point<float> position);

    ProgressionEditor& _progressionEditor;
    audio::ProgressionPlayer* _progressionPlayer = nullptr;
    nui::Theme::HeightType _heightType = nui::Theme::HeightType::AUTO;

    std::vector<Listener*> _listeners;
    bool _dragGestureStarted = false;
    int _pressedChordBlockIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProgressionTimeline)
};

}
