#pragma once

#include <vector>

#include <nierika_dsp/nierika_dsp.h>

namespace component
{

// The draggable item at the bottom of the progression sequencer - dragging it out exports every
// populated slot as one multi-chord MIDI clip (see AppLayout::onProgressionDragStarted /
// MidiExporter::writeProgressionMidiFile). Same click-vs-drag threshold pattern as ChordCard, but
// with no click behavior of its own (nothing to open on a plain click).
class ProgressionDragHandle : public nui::Component
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void onProgressionDragStarted() = 0;
    };

    explicit ProgressionDragHandle(const std::string& identifier);
    ~ProgressionDragHandle() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void mouseDrag(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    nelement::SVGButton _icon;
    nelement::Text _label;

    bool _dragGestureStarted = false;

    std::vector<Listener*> _listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProgressionDragHandle)
};

}
