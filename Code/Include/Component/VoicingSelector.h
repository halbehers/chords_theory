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

    struct Listener
    {
        virtual ~Listener() = default;

        // Fired whenever this panel actually transitions from visible to hidden, regardless of
        // what triggered it - its own close button, or the owner calling close() directly. Lets
        // the owner keep its own visibility/layout state (e.g. AppLayout's fixed row height for
        // this panel) in sync without duplicating that reset logic at every call site that might
        // close this panel.
        virtual void onVoicingSelectorClosed() = 0;
    };

    explicit VoicingSelector(const std::string& identifier);
    ~VoicingSelector() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;

    // Rebuilds the button row for `voicings` (clearing any previous set), highlights
    // currentSymbol, resets scroll position to the start, makes itself visible. Does not touch
    // the arrow - see setArrowTargetX.
    void show(const std::vector<theory::Chord>& voicings, const std::string& currentSymbol, OnVoicingChosen onChosen);

    // Hides the panel and notifies Listeners via onVoicingSelectorClosed(). Safe to call even if
    // already hidden (a no-op in that case, and does not re-notify).
    void close();

    // In THIS component's own local coordinate space - the caller computes it via
    // Component::getLocalPoint() against the actual anchor component, since this panel has no
    // knowledge of what it's pointing at.
    void setArrowTargetX(int arrowTargetX);

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

private:
    void onButtonClick(const std::string& componentID) override;
    void refreshSelectedStates();
    void layoutVoicingRow();

    // Shows the horizontal scrollbar only while genuinely needed (content wider than the viewport)
    // AND the pointer is over this panel - an auto-hide overlay scrollbar, rather than one that's
    // permanently visible whenever there are enough voicings to scroll through.
    void updateScrollBarVisibility();

    static constexpr int kTriangleHeight = 10;
    static constexpr int kTriangleHalfWidth = 8;
    static constexpr int kButtonWidth = 70;
    static constexpr int kButtonGap = 4;
    static constexpr int kCloseButtonSize = 20;
    static constexpr int kBodyInset = 6;
    // Native scrollbar thickness, explicitly set on _viewport (see constructor) rather than left at
    // the LookAndFeel's default - deliberately NOT reserved out of the button row's own height (see
    // layoutVoicingRow), so it only ever occupies its own thin strip at the very bottom edge when
    // actually shown, instead of shrinking/pushing up the (vertically centred) button row above it.
    static constexpr int kScrollbarThickness = 8;

    std::vector<theory::Chord> _voicings;
    std::string _currentSymbol;
    OnVoicingChosen _onChosen;
    int _arrowTargetX = 0;

    nelement::SVGButton _closeButton { "voicing-selector-close", nui::Icons::getCross() };
    juce::Viewport _viewport;
    nui::Component _voicingRow { "voicing-selector-row" }; // 1D strip - manually positions its own buttons, GridLayout's 2D machinery is unneeded here
    std::vector<std::unique_ptr<nelement::TextButton>> _buttons;
    std::vector<Listener*> _listeners;
    bool _isHovering = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoicingSelector)
};

}
