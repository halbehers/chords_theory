#pragma once

#include <optional>
#include <vector>

#include <nierika_dsp/nierika_dsp.h>

#include "Theory/Chord.h"
#include "Theory/Degree.h"

namespace component
{

// One timeline cell in the progression sequencer - empty (a "+" placeholder) or holding a
// resolved chord. Accepts a dropped ChordCard export (see AppLayout's in-flight drag map) via
// juce::FileDragAndDropTarget and replaces whatever it currently holds, per the requirement that
// dropping onto an occupied slot overwrites it rather than being rejected.
class ProgressionSlotView : public nui::Component, public juce::FileDragAndDropTarget
{
public:
    struct Listener
    {
        virtual ~Listener() = default;

        // A file was dropped on this slot - the owner (ProgressionSequencer/AppLayout) resolves
        // which chord it represents via the in-flight drag map and calls setDegree() back in
        // response. This view never resolves chord data itself.
        virtual void onFileDropped(int slotIndex, const juce::String& filePath) = 0;

        // The user clicked a populated slot (empty slots fire nothing) - the owner resolves the
        // slot's degree to an actual chord via its ChordResolver, same as onFileDropped, since
        // this view never resolves chord data itself.
        virtual void onSlotPreviewRequested(int slotIndex) = 0;

        // The user double-clicked a populated slot, requesting its removal (empty slots fire
        // nothing). The owner is responsible for removing it and shifting every later slot back
        // by one so the progression stays packed with no gap left behind.
        virtual void onSlotRemoveRequested(int slotIndex) = 0;
    };

    ProgressionSlotView(const std::string& identifier, int slotIndex);
    ~ProgressionSlotView() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void setChord(theory::Degree degree, const theory::Chord& chord);
    void clear();

    [[nodiscard]] int getSlotIndex() const { return _slotIndex; }
    [[nodiscard]] std::optional<theory::Degree> getDegree() const { return _degree; }

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void refresh();

    int _slotIndex;
    std::optional<theory::Degree> _degree;
    std::string _chordReadableName;

    nelement::Text _label;

    bool _isDragHover = false;

    std::vector<Listener*> _listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProgressionSlotView)
};

}
