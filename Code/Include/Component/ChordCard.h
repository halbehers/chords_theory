#pragma once

#include <vector>

#include <nierika_dsp/nierika_dsp.h>

#include "Theory/Chord.h"
#include "Theory/Degree.h"

namespace component
{

// One clickable/draggable chord card: shows a scale degree's currently selected voicing. A single
// click previews the chord's audio (see Listener::onChordPreviewRequested); a double-click asks
// the Listener to open the inline voicing selector (see Listener::onVoicingSelectorRequested and
// component::VoicingSelector) - this card never opens anything itself, it just reports the
// gesture and its own current data; drag reports the gesture to its Listener, which owns writing
// the temp MIDI file and performing the actual OS-level drag (see MidiExporter / AppLayout) -
// this widget only knows about chord data and UI, never about MIDI files or audio.
class ChordCard : public nui::Component
{
public:
    struct Listener
    {
        virtual ~Listener() = default;

        // The user picked a different voicing via the voicing selector for this card's degree.
        virtual void onChordChanged(theory::Degree degree, const theory::Chord& newChord) = 0;

        // The user started dragging this card - past the minimum-distance threshold, so this
        // never fires for a plain click.
        virtual void onChordDragStarted(theory::Degree degree, const theory::Chord& chord) = 0;

        // The user clicked (not dragged) this card - fired on every such click, including both
        // clicks of a double-click gesture (see mouseDoubleClick, which additionally fires
        // onVoicingSelectorRequested below).
        virtual void onChordPreviewRequested(theory::Degree degree, const theory::Chord& chord) = 0;

        // The user double-clicked this card - the owner shows/refreshes the inline voicing
        // selector for this degree's availableVoicings/currentSymbol. Never fires if this card
        // has no voicings to choose between.
        virtual void onVoicingSelectorRequested(theory::Degree degree, const std::vector<theory::Chord>& availableVoicings, const std::string& currentSymbol) = 0;
    };

    ChordCard(const std::string& identifier, theory::Degree degree);
    ~ChordCard() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void setChord(const theory::Chord& chord);
    [[nodiscard]] const theory::Chord& getChord() const { return _chord; }
    [[nodiscard]] theory::Degree getDegree() const { return _degree; }

    // Sorted by popularityOrder - i.e. ScaleDegreeData::chords, unmodified. Needed to populate the
    // voicing selector on double-click (see onVoicingSelectorRequested).
    void setAvailableVoicings(std::vector<theory::Chord> voicings);

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void refreshLabels();

    theory::Degree _degree;
    theory::Chord _chord;
    std::vector<theory::Chord> _availableVoicings;

    nelement::Text _degreeLabel;
    nelement::Text _chordNameLabel;

    nlayout::GridLayout<nui::Component> _layout { *this };

    std::vector<Listener*> _listeners;

    bool _dragGestureStarted = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChordCard)
};

}
