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
    _closeButton.setVisible(false); // hover-only, see updateHoverVisibility()

    addAndMakeVisible(_viewport);
    _viewport.setViewedComponent(&_voicingRow, false); // false: _voicingRow is an owned member, not viewport-owned
    _viewport.setScrollBarsShown(false, true); // horizontal only
    _viewport.setScrollBarThickness(kScrollbarThickness);
    // LookAndFeel_V4::drawScrollbar only ever paints the thumb (no track/background), so this is
    // the only colour that actually shows. juce::Component::findColour defaults to
    // inheritFromParent=false, and ScrollBar::paint() uses that default - so this has to be set
    // directly on the actual ScrollBar instance, not on _viewport (which findColour would never
    // walk up to).
    _viewport.getHorizontalScrollBar().setColour(juce::ScrollBar::thumbColourId, nui::Theme::newColor(nui::Theme::ThemeColor::BACKGROUND).asJuce().withAlpha(0.5f));

    // wantsEventsForAllNestedChildComponents=true: lets mouseEnter/mouseExit below track hovering
    // over the panel as a whole (including its buttons/viewport/close button), not just bare space.
    addMouseListener(this, true);

    // A global listener (rather than relying on the local one above) is what lets
    // handleGlobalMouseDown below actually see clicks on sibling components elsewhere in the app -
    // the local listener can only ever report events within this component's own subtree, never a
    // click on, say, a ChordCard. See GlobalMouseListener's own comment for why this is a separate
    // object rather than `this`.
    juce::Desktop::getInstance().addGlobalMouseListener(&_globalMouseListener);
}

VoicingSelector::~VoicingSelector()
{
    juce::Desktop::getInstance().removeGlobalMouseListener(&_globalMouseListener);

    _closeButton.removeListener(this);

    for (auto& button : _buttons)
        button->removeListener(this);
}

void VoicingSelector::paint(juce::Graphics& g)
{
    Component::paint(g);

    const auto bodyBounds = getLocalBounds().withTrimmedTop(kTriangleHeight).toFloat();

    g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::ACCENT).asJuce().withAlpha(.2f));
    g.fillRoundedRectangle(bodyBounds, 0.f);

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

    const auto closeButtonArea = contentBounds.removeFromRight(kCloseButtonSize / 2).removeFromTop(kCloseButtonSize / 2).translated(-kCloseButtonEdgeGap, kCloseButtonEdgeGap);
    _closeButton.setBounds(closeButtonArea);

    _viewport.setBounds(contentBounds.withTrimmedRight(kBodyInset));

    layoutVoicingRow();
}

void VoicingSelector::mouseEnter(const juce::MouseEvent&)
{
    _isHovering = true;
    updateHoverVisibility();
}

void VoicingSelector::mouseExit(const juce::MouseEvent& event)
{
    // addMouseListener(this, true) means this also fires for every child's own boundary crossings
    // (e.g. moving from one voicing button to its neighbour) - only treat it as a real "left the
    // whole panel" exit if the pointer has actually left this component's own bounds.
    if (getLocalBounds().contains(event.getEventRelativeTo(this).getPosition()))
        return;

    _isHovering = false;
    updateHoverVisibility();
}

void VoicingSelector::handleGlobalMouseDown(const juce::MouseEvent& event)
{
    // Delivered for every mouseDown in the app (see _globalMouseListener, registered in the
    // constructor) - a click landing inside this panel or one of its children (the close button,
    // a voicing button) is handled by that component's own click handler, not this; only a press
    // that lands genuinely outside both this panel and the exempt component (if any) dismisses it.
    if (!isVisible() || getScreenBounds().contains(event.getScreenPosition()))
        return;

    if (_dismissExemptComponent != nullptr && _dismissExemptComponent->getScreenBounds().contains(event.getScreenPosition()))
        return;

    close();
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
        button->setEnabled(chord.symbol != _currentSymbol);
        button->setBackgroundColour(juce::Colours::transparentBlack);
        button->setSelectedBackgroundColour(nui::Theme::newColor(nui::Theme::ThemeColor::ACCENT).asJuce().withAlpha(.4f));
        button->setHoverBackgroundColour(nui::Theme::newColor(nui::Theme::ThemeColor::ACCENT).asJuce().withAlpha(.2f));
        button->setFontSize(nui::Theme::PARAGRAPH);
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
    if (!isVisible())
        return;

    setVisible(false);

    for (auto* listener : _listeners)
        listener->onVoicingSelectorClosed();
}

void VoicingSelector::setArrowTargetX(int arrowTargetX)
{
    _arrowTargetX = arrowTargetX;
    repaint();
}

void VoicingSelector::addListener(Listener* listener)
{
    _listeners.push_back(listener);
}

void VoicingSelector::removeListener(Listener* listener)
{
    _listeners.erase(std::remove(_listeners.begin(), _listeners.end(), listener), _listeners.end());
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
        _buttons[i]->setEnabled(_voicings[i].symbol != _currentSymbol);
        _buttons[i]->repaint();
    }
}

void VoicingSelector::layoutVoicingRow()
{
    // Full viewport height, not reduced for the scrollbar - each TextButton already vertically
    // centres its own themed-height pill within whatever bounds it's given (see
    // TextButton::resized()'s withSizeKeepingCentre), so handing it the full height centres the
    // whole button row for free. The native horizontal scrollbar (see constructor) only reserves
    // its own thin strip at the very bottom edge when actually shown, rather than shrinking this.
    const auto rowHeight = _viewport.getHeight();
    const auto viewportWidth = _viewport.getWidth();
    const auto numVoicings = static_cast<int>(_buttons.size());

    if (numVoicings == 0)
    {
        _voicingRow.setSize(viewportWidth, rowHeight);
        return;
    }

    const auto naturalRowWidth = numVoicings * kButtonWidth + (numVoicings - 1) * kButtonGap;
    // At least the full viewport width, so a short row that fits has room to be centred in rather
    // than sitting flush left with the viewport's own leftover space to its right.
    _voicingRow.setSize(juce::jmax(naturalRowWidth, viewportWidth), rowHeight);

    // Centres the whole strip when it fits within the viewport; otherwise starts flush left so it
    // scrolls normally rather than starting part-way through an off-centre strip.
    auto x = naturalRowWidth < viewportWidth ? (viewportWidth - naturalRowWidth) / 2 : 0;
    for (auto& button : _buttons)
    {
        button->setBounds(x, 0, kButtonWidth, rowHeight);
        x += kButtonWidth + kButtonGap;
    }

    updateHoverVisibility();
}

void VoicingSelector::updateHoverVisibility()
{
    const bool needsScrolling = _voicingRow.getWidth() > _viewport.getWidth();
    _viewport.getHorizontalScrollBar().setVisible(needsScrolling && _isHovering);
    _closeButton.setVisible(_isHovering);
}

}
