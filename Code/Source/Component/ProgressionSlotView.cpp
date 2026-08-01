#include "Component/ProgressionSlotView.h"

#include <algorithm>

#include "AppLocalisation.h"

namespace component
{

ProgressionSlotView::ProgressionSlotView(const std::string& identifier, int slotIndex):
    Component(identifier),
    _slotIndex(slotIndex),
    _label(identifier + "-label", "", "")
{
    _label.setFontSize(nui::Theme::PARAGRAPH);
    _label.setJustificationType(juce::Justification::centred);
    _label.setInterceptsMouseClicks(false, false); // decorative - clicks must reach the slot

    addAndMakeVisible(_label);

    nui::Theme::getChangeBroadcaster().addChangeListener(this);
    AppLocalisation::getChangeBroadcaster().addChangeListener(this);

    refresh();
}

ProgressionSlotView::~ProgressionSlotView()
{
    nui::Theme::getChangeBroadcaster().removeChangeListener(this);
    AppLocalisation::getChangeBroadcaster().removeChangeListener(this);
}

void ProgressionSlotView::paint(juce::Graphics& g)
{
    Component::paint(g);

    if (_isDragHover)
    {
        g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::ACCENT).asJuce().withAlpha(0.25f));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), nui::Theme::getBorderRadius());
    }
}

void ProgressionSlotView::resized()
{
    Component::resized();

    _label.setBounds(getLocalBounds());
}

void ProgressionSlotView::setChord(theory::Degree degree, const theory::Chord& chord)
{
    _degree = degree;
    _chordReadableName = chord.readableName;
    refresh();
}

void ProgressionSlotView::clear()
{
    _degree.reset();
    _chordReadableName.clear();
    refresh();
}

void ProgressionSlotView::refresh()
{
    _label.setText(_degree.has_value() ? _chordReadableName : "+");
    _label.setColor(_degree.has_value() ? nui::Theme::ThemeColor::TEXT : nui::Theme::ThemeColor::DISABLED);

    displayBorder(nui::Theme::ThemeColor::BORDER, 1.f, nui::Theme::getBorderRadius());

    if (_degree.has_value())
        displayBackground(nui::Theme::ThemeColor::SECONDARY_BACKGROUND, nui::Theme::getBorderRadius());
    else
        hideBackground();

    repaint();
}

bool ProgressionSlotView::isInterestedInFileDrag(const juce::StringArray& files)
{
    return files.size() == 1 && files[0].endsWithIgnoreCase(".mid");
}

void ProgressionSlotView::fileDragEnter(const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused(files, x, y);

    _isDragHover = true;
    repaint();
}

void ProgressionSlotView::fileDragExit(const juce::StringArray& files)
{
    juce::ignoreUnused(files);

    _isDragHover = false;
    repaint();
}

void ProgressionSlotView::filesDropped(const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused(x, y);

    _isDragHover = false;
    repaint();

    if (files.isEmpty())
        return;

    for (auto* listener : _listeners)
        listener->onFileDropped(_slotIndex, files[0]);
}

void ProgressionSlotView::mouseUp(const juce::MouseEvent&)
{
    if (!_degree.has_value())
        return;

    for (auto* listener : _listeners)
        listener->onSlotPreviewRequested(_slotIndex);
}

void ProgressionSlotView::mouseDoubleClick(const juce::MouseEvent&)
{
    if (!_degree.has_value())
        return;

    for (auto* listener : _listeners)
        listener->onSlotRemoveRequested(_slotIndex);
}

void ProgressionSlotView::addListener(Listener* listener)
{
    _listeners.push_back(listener);
}

void ProgressionSlotView::removeListener(Listener* listener)
{
    _listeners.erase(std::remove(_listeners.begin(), _listeners.end(), listener), _listeners.end());
}

void ProgressionSlotView::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    Component::changeListenerCallback(source);

    if (source == &nui::Theme::getChangeBroadcaster() || source == &AppLocalisation::getChangeBroadcaster())
        refresh();
}

}
