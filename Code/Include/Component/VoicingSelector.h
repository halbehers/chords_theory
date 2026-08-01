#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <nierika_dsp/nierika_dsp.h>

#include "Theory/Chord.h"

namespace component
{

// Inline, persistent voicing browser embedded in AppLayout's own grid (unlike the popup it
// replaced, this is a standalone widget on the main layout, never a transient popup). Given a
// list of voicings it shows a horizontally-scrollable row of buttons; picking one invokes the
// supplied callback but does NOT close the panel, so the user can keep comparing/re-previewing.
// Degree-agnostic by design - knows nothing about theory::Degree, ChordCard, or
// ChordDegreeBrowser; the caller (AppLayout) owns resolving a pick back to an actual card.
class VoicingSelector : public nui::Component,
                         public nelement::SVGButton::OnClickListener,
                         public nelement::TextButton::OnClickListener
{
public:
    using OnVoicingChosen = std::function<void(const theory::Chord&)>;

    explicit VoicingSelector(const std::string& identifier);
    ~VoicingSelector() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // Rebuilds the button row for `voicings` (clearing any previous set), highlights
    // currentSymbol, resets scroll position to the start, makes itself visible. Does not touch
    // the arrow - see setArrowTargetX.
    void show(const std::vector<theory::Chord>& voicings, const std::string& currentSymbol, OnVoicingChosen onChosen);

    // Hides the panel. Safe to call even if already hidden.
    void close();

    // In THIS component's own local coordinate space - the caller computes it via
    // Component::getLocalPoint() against the actual anchor component, since this panel has no
    // knowledge of what it's pointing at.
    void setArrowTargetX(int arrowTargetX);

private:
    void onButtonClick(const std::string& componentID) override;
    void refreshSelectedStates();
    void layoutVoicingRow();

    static constexpr int kTriangleHeight = 10;
    static constexpr int kTriangleHalfWidth = 8;
    static constexpr int kButtonWidth = 110;
    static constexpr int kButtonGap = 4;
    static constexpr int kCloseButtonSize = 20;
    static constexpr int kBodyInset = 6;
    static constexpr int kScrollbarThickness = 8; // matches the LookAndFeel's default scrollbar width
    static constexpr int kScrollbarGap = 6; // additional vertical space between the button row and the scrollbar below it

    std::vector<theory::Chord> _voicings;
    std::string _currentSymbol;
    OnVoicingChosen _onChosen;
    int _arrowTargetX = 0;

    nelement::SVGButton _closeButton { "voicing-selector-close", nui::Icons::getCross() };
    juce::Viewport _viewport;
    nui::Component _voicingRow { "voicing-selector-row" }; // 1D strip - manually positions its own buttons, GridLayout's 2D machinery is unneeded here
    std::vector<std::unique_ptr<nelement::TextButton>> _buttons;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoicingSelector)
};

}
