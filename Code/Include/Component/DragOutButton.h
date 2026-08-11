#pragma once

#include <string>
#include <vector>

#include <nierika_dsp/nierika_dsp.h>

namespace component
{

// A small icon-only button (the handle icon) whose action is "drag me out" rather than "click me" -
// same click-vs-drag threshold gesture ChordCard's own drag gesture uses, wrapping a single
// nelement::SVGButton so the gesture-detection plumbing lives in exactly one place and can be
// reused wherever a bare drag-out affordance is needed (e.g. ProgressionEditor's own header,
// AppLayout's Synth-tab header). Fires no file drag itself - purely a "the user started dragging
// me" signal; the listener resolves what to actually export and performs the OS-level drag (see
// AppLayout::onProgressionDragStarted).
class DragOutButton : public nui::Component
{
public:
    struct Listener
    {
        virtual ~Listener() = default;

        // Past the minimum-distance threshold, so this never fires for a plain click.
        virtual void onDragStarted() = 0;
    };

    explicit DragOutButton(const std::string& identifier);
    ~DragOutButton() override;

    void resized() override;

    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

private:
    nelement::SVGButton _button;

    bool _dragGestureStarted = false;

    std::vector<Listener*> _listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DragOutButton)
};

}
