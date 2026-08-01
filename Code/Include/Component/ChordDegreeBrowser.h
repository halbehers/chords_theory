#pragma once

#include <memory>
#include <string>
#include <vector>

#include <nierika_dsp/nierika_dsp.h>

#include "Component/ChordCard.h"
#include "Theory/Key.h"
#include "Theory/ProgressionSlot.h"
#include "Theory/Scale.h"

namespace component
{

// One ChordCard per available scale degree for the current Key/Scale (7 normally, 3 for Minor
// Blues), each showing its most-popular voicing by default. Bubbles ChordCard events up to its own
// Listener so AppLayout can keep shared per-degree-voicing state in sync and drive MIDI export.
class ChordDegreeBrowser : public nui::Component, public ChordCard::Listener
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void onChordChanged(theory::Degree degree, const theory::Chord& newChord) = 0;
        virtual void onChordDragStarted(theory::Degree degree, const theory::Chord& chord) = 0;
        virtual void onChordPreviewRequested(theory::Degree degree, const theory::Chord& chord) = 0;
        virtual void onVoicingSelectorRequested(theory::Degree degree, const std::vector<theory::Chord>& availableVoicings, const std::string& currentSymbol) = 0;
    };

    explicit ChordDegreeBrowser(const std::string& identifier);
    ~ChordDegreeBrowser() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void setKeyAndScale(theory::Key key, theory::Scale scale);

    // Restores a specific voicing for a degree (used when restoring session state) - falls back
    // to the current default if chordSymbol isn't found among that degree's available voicings.
    void setDegreeVoicing(theory::Degree degree, const std::string& chordSymbol);

    // The chord currently displayed for a degree, i.e. the single source of truth other
    // components (progression sequencer, session-state serialization) should read from rather
    // than tracking their own copy - nullptr if this degree doesn't exist for the current scale.
    [[nodiscard]] const theory::Chord* getCurrentChord(theory::Degree degree) const;

    // Resolves a progression slot to an actual chord: if slot.popularityOrder is pinned (> 0),
    // looks up the voicing at that position among this degree's available voicings under the
    // current key/scale (positions are stable across keys - see ProgressionSlot's doc comment for
    // why this is used instead of Chord::symbol), falling back to getCurrentChord(slot.degree) if
    // unpinned or that position doesn't exist under the current key/scale - nullptr if this
    // degree doesn't exist for the current scale.
    [[nodiscard]] const theory::Chord* resolveSlot(const theory::ProgressionSlot& slot) const;

    // The underlying card for a degree - used by AppLayout to compute where the voicing
    // selector's arrow should point. Returns non-const (even though this method is
    // const-qualified) because nui::Component declares its own non-const getLocalBounds() that
    // hides (not overrides) juce::Component's const one - a const ChordCard* would fail to
    // compile at the call site. nullptr if this degree doesn't exist for the current scale.
    [[nodiscard]] ChordCard* getCard(theory::Degree degree) const;

    // Applies a voicing picked via the inline voicing selector: sets the card's chord and
    // notifies listeners via onChordChanged, exactly as if the user had picked it from the card
    // itself - the voicing selector never touches ChordCard directly.
    void selectVoicing(theory::Degree degree, const theory::Chord& chord);

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

private:
    void onChordChanged(theory::Degree degree, const theory::Chord& newChord) override;
    void onChordDragStarted(theory::Degree degree, const theory::Chord& chord) override;
    void onChordPreviewRequested(theory::Degree degree, const theory::Chord& chord) override;
    void onVoicingSelectorRequested(theory::Degree degree, const std::vector<theory::Chord>& availableVoicings, const std::string& currentSymbol) override;

    std::vector<std::unique_ptr<ChordCard>> _cards;
    nlayout::GridLayout<nui::Component> _layout { *this };
    std::vector<Listener*> _listeners;

    theory::Key _currentKey = theory::Key::C;
    theory::Scale _currentScale = theory::Scale::Major;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChordDegreeBrowser)
};

}
