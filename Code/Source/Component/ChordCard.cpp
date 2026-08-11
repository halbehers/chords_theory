#include "Component/ChordCard.h"

#include <algorithm>

#include "AppLocalisation.h"

namespace component
{

namespace
{
    constexpr float kDragStartThreshold = 6.f;
}

ChordCard::ChordCard(const std::string& identifier, theory::Degree degree):
    Component(identifier),
    _degree(degree),
    _degreeLabel(identifier + "-degree", "", theory::getDegreeLabel(degree)),
    _chordNameLabel(identifier + "-name", "", "")
{
    _degreeLabel.setFontSize(nui::Theme::SMALL);
    _degreeLabel.setColor(nui::Theme::ThemeColor::DISABLED);
    _degreeLabel.setJustificationType(juce::Justification::centred);
    _degreeLabel.setInterceptsMouseClicks(false, false); // decorative - clicks must reach the card

    _chordNameLabel.setFontSize(nui::Theme::LABEL);
    _chordNameLabel.setFontWeight(nui::Theme::FontWeight::MEDIUM);
    _chordNameLabel.setJustificationType(juce::Justification::centred);
    _chordNameLabel.setInterceptsMouseClicks(false, false); // decorative - clicks must reach the card

    displayBorder(nui::Theme::ThemeColor::ACCENT, 1.f, nui::Theme::getBorderRadius());
    displayBackground(nui::Theme::newColor(nui::Theme::ThemeColor::ACCENT).asJuce().withAlpha(.2f), nui::Theme::getBorderRadius());

    setTooltip(juce::translate("chord_card_tooltip").toStdString());
    setTooltipEnabled(true);

    setMouseCursor(juce::MouseCursor::DraggingHandCursor);

    _layout.setGap(2.f);
    _layout.setDisplayGrid(false);
    _layout.init({ 1, 2 }, { 1 });
    _layout.addComponent(_degreeLabel, 0, 0, 1, 1);
    _layout.addComponent(_chordNameLabel, 1, 0, 1, 1);

    setPadding(4.f);

    nui::Theme::getChangeBroadcaster().addChangeListener(this);
    AppLocalisation::getChangeBroadcaster().addChangeListener(this);
}

ChordCard::~ChordCard()
{
    nui::Theme::getChangeBroadcaster().removeChangeListener(this);
    AppLocalisation::getChangeBroadcaster().removeChangeListener(this);
}

void ChordCard::paint(juce::Graphics& g)
{
    Component::paint(g);

    _layout.paint(g);
}

void ChordCard::resized()
{
    Component::resized();

    _layout.resized();
}

void ChordCard::setChord(const theory::Chord& chord)
{
    _chord = chord;
    refreshLabels();
}

void ChordCard::refreshLabels()
{
    _chordNameLabel.setText(_chord.readableName);
    repaint();
}

void ChordCard::setAvailableVoicings(std::vector<theory::Chord> voicings)
{
    _availableVoicings = std::move(voicings);
}

void ChordCard::addListener(Listener* listener)
{
    _listeners.push_back(listener);
}

void ChordCard::removeListener(Listener* listener)
{
    _listeners.erase(std::remove(_listeners.begin(), _listeners.end(), listener), _listeners.end());
}

void ChordCard::mouseDown(const juce::MouseEvent&)
{
    _dragGestureStarted = false;
}

void ChordCard::mouseDrag(const juce::MouseEvent& event)
{
    if (_dragGestureStarted)
        return;

    if (static_cast<float>(event.getDistanceFromDragStart()) < kDragStartThreshold)
        return;

    _dragGestureStarted = true;

    for (auto* listener : _listeners)
        listener->onChordDragStarted(_degree, _chord);
}

void ChordCard::mouseUp(const juce::MouseEvent& event)
{
    if (_dragGestureStarted)
    {
        _dragGestureStarted = false;
        return;
    }

    for (auto* listener : _listeners)
        listener->onChordPreviewRequested(_degree, _chord);

    juce::ignoreUnused(event);
}

void ChordCard::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (_availableVoicings.empty())
        return;

    for (auto* listener : _listeners)
        listener->onVoicingSelectorRequested(_degree, _availableVoicings, _chord.symbol);

    juce::ignoreUnused(event);
}

void ChordCard::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    Component::changeListenerCallback(source);

    if (source == &nui::Theme::getChangeBroadcaster())
    {
        repaint();
        return;
    }

    if (source == &AppLocalisation::getChangeBroadcaster())
    {
        setTooltip(juce::translate("chord_card_tooltip").toStdString());
        repaint();
    }
}

}
