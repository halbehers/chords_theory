#include "Component/ChordDegreeBrowser.h"

#include <algorithm>

#include "Theory/ChordDatabase.h"

namespace component
{

ChordDegreeBrowser::ChordDegreeBrowser(const std::string& identifier):
    Component(identifier)
{
    _layout.setGap(8.f);
    _layout.setDisplayGrid(false);
}

ChordDegreeBrowser::~ChordDegreeBrowser() = default;

void ChordDegreeBrowser::paint(juce::Graphics& g)
{
    Component::paint(g);

    _layout.paint(g);
}

void ChordDegreeBrowser::resized()
{
    Component::resized();

    _layout.resized();
}

void ChordDegreeBrowser::setKeyAndScale(theory::Key key, theory::Scale scale)
{
    _currentKey = key;
    _currentScale = scale;

    for (auto& card : _cards)
    {
        card->removeListener(this);
        removeChildComponent(card.get());
    }
    _cards.clear();
    _layout.reset();

    const auto& keyScaleData = theory::ChordDatabase::getInstance().get(key, scale);
    const auto numDegrees = static_cast<int>(keyScaleData.degrees.size());

    if (numDegrees == 0)
        return;

    _layout.init({ 1 }, std::vector<int>(static_cast<std::size_t>(numDegrees), 1));

    for (int i = 0; i < numDegrees; ++i)
    {
        const auto& degreeData = keyScaleData.degrees[static_cast<std::size_t>(i)];

        auto card = std::make_unique<ChordCard>("chord-card-" + theory::getDegreeLabel(degreeData.degree), degreeData.degree);
        card->setAvailableVoicings(degreeData.chords);
        card->setChord(degreeData.chords.front()); // most popular voicing, chords are pre-sorted
        card->addListener(this);

        addAndMakeVisible(*card);
        _layout.addComponent(*card, 0, i, 1, 1);

        _cards.push_back(std::move(card));
    }

    resized();
    repaint();
}

void ChordDegreeBrowser::setDegreeVoicing(theory::Degree degree, const std::string& chordSymbol)
{
    const auto cardIt = std::find_if(_cards.begin(), _cards.end(),
        [degree](const std::unique_ptr<ChordCard>& card) { return card->getDegree() == degree; });

    if (cardIt == _cards.end())
        return;

    const auto* degreeData = theory::ChordDatabase::getInstance().get(_currentKey, _currentScale).findDegree(degree);

    if (degreeData == nullptr || degreeData->chords.empty())
        return;

    const auto chordIt = std::find_if(degreeData->chords.begin(), degreeData->chords.end(),
        [&chordSymbol](const theory::Chord& chord) { return chord.symbol == chordSymbol; });

    // Falls back to the default (most popular) voicing if chordSymbol isn't found - e.g. state
    // saved under a different key/scale that no longer has a matching symbol.
    (*cardIt)->setChord(chordIt != degreeData->chords.end() ? *chordIt : degreeData->chords.front());
}

const theory::Chord* ChordDegreeBrowser::getCurrentChord(theory::Degree degree) const
{
    const auto it = std::find_if(_cards.begin(), _cards.end(),
        [degree](const std::unique_ptr<ChordCard>& card) { return card->getDegree() == degree; });

    return it == _cards.end() ? nullptr : &(*it)->getChord();
}

const theory::Chord* ChordDegreeBrowser::resolveSlot(const theory::ProgressionSlot& slot) const
{
    if (slot.popularityOrder <= 0)
        return getCurrentChord(slot.degree);

    const auto* degreeData = theory::ChordDatabase::getInstance().get(_currentKey, _currentScale).findDegree(slot.degree);

    if (degreeData == nullptr)
        return getCurrentChord(slot.degree);

    const auto it = std::find_if(degreeData->chords.begin(), degreeData->chords.end(),
        [&slot](const theory::Chord& chord) { return chord.popularityOrder == slot.popularityOrder; });

    return it != degreeData->chords.end() ? &(*it) : getCurrentChord(slot.degree);
}

ChordCard* ChordDegreeBrowser::getCard(theory::Degree degree) const
{
    const auto it = std::find_if(_cards.begin(), _cards.end(),
        [degree](const std::unique_ptr<ChordCard>& card) { return card->getDegree() == degree; });

    return it == _cards.end() ? nullptr : it->get();
}

void ChordDegreeBrowser::selectVoicing(theory::Degree degree, const theory::Chord& chord)
{
    const auto it = std::find_if(_cards.begin(), _cards.end(),
        [degree](const std::unique_ptr<ChordCard>& card) { return card->getDegree() == degree; });

    if (it == _cards.end())
        return;

    (*it)->setChord(chord);

    for (auto* listener : _listeners)
        listener->onChordChanged(degree, chord);
}

void ChordDegreeBrowser::addListener(Listener* listener)
{
    _listeners.push_back(listener);
}

void ChordDegreeBrowser::removeListener(Listener* listener)
{
    _listeners.erase(std::remove(_listeners.begin(), _listeners.end(), listener), _listeners.end());
}

void ChordDegreeBrowser::onChordChanged(theory::Degree degree, const theory::Chord& newChord)
{
    for (auto* listener : _listeners)
        listener->onChordChanged(degree, newChord);
}

void ChordDegreeBrowser::onChordDragStarted(theory::Degree degree, const theory::Chord& chord)
{
    for (auto* listener : _listeners)
        listener->onChordDragStarted(degree, chord);
}

void ChordDegreeBrowser::onChordPreviewRequested(theory::Degree degree, const theory::Chord& chord)
{
    for (auto* listener : _listeners)
        listener->onChordPreviewRequested(degree, chord);
}

void ChordDegreeBrowser::onVoicingSelectorRequested(theory::Degree degree, const std::vector<theory::Chord>& availableVoicings, const std::string& currentSymbol)
{
    for (auto* listener : _listeners)
        listener->onVoicingSelectorRequested(degree, availableVoicings, currentSymbol);
}

}
