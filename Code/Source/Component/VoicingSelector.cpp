#include "Component/VoicingSelector.h"

#include <algorithm>

namespace component
{

VoicingSelector::VoicingSelector(const std::string& identifier):
    Component(identifier)
{
    addAndMakeVisible(_closeButton);
    _closeButton.addOnClickListener(this);
    _closeButton.setIconSize(12.f);
    _closeButton.toFront(false); // defensive z-order parity with SettingsWindow's close button

    addAndMakeVisible(_viewport);
    _viewport.setViewedComponent(&_voicingRow, false); // false: _voicingRow is an owned member, not viewport-owned
    _viewport.setScrollBarsShown(false, true); // horizontal only
}

VoicingSelector::~VoicingSelector()
{
    _closeButton.removeListener(this);

    for (auto& button : _buttons)
        button->removeListener(this);
}

void VoicingSelector::paint(juce::Graphics& g)
{
    Component::paint(g);

    const auto bodyBounds = getLocalBounds().withTrimmedTop(kTriangleHeight).toFloat();

    g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::SECONDARY_BACKGROUND).asJuce());
    g.fillRoundedRectangle(bodyBounds, nui::Theme::getBorderRadius());

    g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::BORDER).asJuce());
    g.drawRoundedRectangle(bodyBounds, nui::Theme::getBorderRadius(), 1.f);

    const auto apexX = static_cast<float>(juce::jlimit(kTriangleHalfWidth, getWidth() - kTriangleHalfWidth, _arrowTargetX));

    juce::Path triangle;
    triangle.addTriangle(apexX - static_cast<float>(kTriangleHalfWidth), static_cast<float>(kTriangleHeight),
                          apexX + static_cast<float>(kTriangleHalfWidth), static_cast<float>(kTriangleHeight),
                          apexX, 0.f);
    g.fillPath(triangle);
}

void VoicingSelector::resized()
{
    Component::resized();

    const auto bodyBounds = getLocalBounds().withTrimmedTop(kTriangleHeight);
    auto contentBounds = bodyBounds.reduced(kBodyInset);

    const auto closeButtonArea = contentBounds.removeFromRight(kCloseButtonSize).removeFromTop(kCloseButtonSize);
    _closeButton.setBounds(closeButtonArea);

    _viewport.setBounds(contentBounds.withTrimmedRight(kBodyInset));

    layoutVoicingRow();
}

void VoicingSelector::show(const std::vector<theory::Chord>& voicings, const std::string& currentSymbol, OnVoicingChosen onChosen)
{
    for (auto& button : _buttons)
        button->removeListener(this);
    _buttons.clear();

    _voicings = voicings;
    _currentSymbol = currentSymbol;
    _onChosen = std::move(onChosen);

    for (const auto& chord : _voicings)
    {
        auto button = std::make_unique<nelement::TextButton>(chord.symbol, chord.readableName);
        button->setIsSelected(chord.symbol == _currentSymbol);
        button->setHeightType(nui::Theme::HeightType::THIN);
        button->addOnClickListener(this);

        _voicingRow.addAndMakeVisible(*button);
        _buttons.push_back(std::move(button));
    }

    layoutVoicingRow();
    _viewport.setViewPosition(0, 0);

    setVisible(true);
}

void VoicingSelector::close()
{
    setVisible(false);
}

void VoicingSelector::setArrowTargetX(int arrowTargetX)
{
    _arrowTargetX = arrowTargetX;
    repaint();
}

void VoicingSelector::onButtonClick(const std::string& componentID)
{
    if (componentID == _closeButton.getComponentID())
    {
        close();
        return;
    }

    const auto it = std::find_if(_voicings.begin(), _voicings.end(),
        [&componentID](const theory::Chord& chord) { return chord.symbol == componentID; });

    if (it == _voicings.end())
        return;

    _currentSymbol = it->symbol;
    refreshSelectedStates();

    if (_onChosen)
        _onChosen(*it);
}

void VoicingSelector::refreshSelectedStates()
{
    for (std::size_t i = 0; i < _voicings.size(); ++i)
    {
        _buttons[i]->setIsSelected(_voicings[i].symbol == _currentSymbol);
        _buttons[i]->repaint();
    }
}

void VoicingSelector::layoutVoicingRow()
{
    // Deliberately not using _viewport.getMaximumVisibleHeight() here: it only subtracts the
    // scrollbar's thickness once a horizontal scrollbar is already showing, which depends on
    // _voicingRow's width - the very thing this method is about to set. On the first show() call
    // for a new (still empty/zero-width) row, that makes it return the viewport's full height,
    // one call too early. Reserving fixed space against the viewport's own bounds instead sizes
    // the row correctly on every call, regardless of the scrollbar's current visibility.
    const auto rowHeight = juce::jmax(0, _viewport.getHeight() - kScrollbarThickness - kScrollbarGap);
    const auto numVoicings = static_cast<int>(_buttons.size());

    if (numVoicings == 0)
    {
        _voicingRow.setSize(0, rowHeight);
        return;
    }

    const auto rowWidth = numVoicings * kButtonWidth + (numVoicings - 1) * kButtonGap;
    _voicingRow.setSize(rowWidth, rowHeight);

    auto x = 0;
    for (auto& button : _buttons)
    {
        button->setBounds(x, 0, kButtonWidth, rowHeight);
        x += kButtonWidth + kButtonGap;
    }
}

}
