#include "Component/SettingsWindow.h"
#include "AppLocalisation.h"

namespace component
{

SettingsWindow::SettingsWindow(const std::string& identifier, nlayout::WindowsManager& windowsManager):
    Window(identifier, nlayout::Distance<>(100, nlayout::Distance<>::PERCENTAGE), nlayout::Distance<>(100, nlayout::Distance<>::PERCENTAGE)),
    _windowsManager(windowsManager)
{
    setMovable(false);

    addAndMakeVisible(_closeButton);
    _closeButton.addOnClickListener(this);
    _closeButton.setIconSize(16.f);

    addAndMakeVisible(_titleIcon);
    _titleIcon.setInterceptsMouseClicks(false, false); // purely decorative, not a real button
    _titleIcon.setIconSize(20.f);

    _title.setText(juce::translate("settings_title").toStdString(), false);
    _title.setFontSize(nui::Theme::HEADING);
    _title.setJustificationType(juce::Justification::topLeft);
    _title.setPadding(28.f, 0.f, 0.f, 0.f); // room for _titleIcon at the left edge of the cell

    AppLocalisation::getChangeBroadcaster().addChangeListener(this);

    _layout.setGap(24.f);
    _layout.setDisplayGrid(false);
    _layout.init({ 1 }, { 1 });

    _layout.setFixedRowHeight(0, 42.f);

    _layout.addComponent(_title, 0, 0, 1, 1);

    _titleIcon.toFront(false);
    _closeButton.toFront(false);
}

SettingsWindow::~SettingsWindow()
{
    _closeButton.removeListener(this);
    AppLocalisation::getChangeBroadcaster().removeChangeListener(this);
}

void SettingsWindow::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    Component::changeListenerCallback(source);

    if (source != &AppLocalisation::getChangeBroadcaster())
        return;

    _title.setText(juce::translate("settings_title").toStdString());
    repaint();
}

void SettingsWindow::paint(juce::Graphics& g)
{
    Component::paint(g);

    g.setColour(juce::Colours::black.withAlpha(0.9f));
    g.fillRect(getLocalBounds());

    const auto cardBounds = getCardBounds().toFloat();
    g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::PRIMARY).asJuce());
    g.fillRoundedRectangle(cardBounds, nui::Theme::getBorderRadius());

    g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::LIGHT_SHADE).asJuce().withAlpha(0.2f));
    g.drawRoundedRectangle(cardBounds, nui::Theme::getBorderRadius(), 1.f);

    _layout.paint(g);
}

void SettingsWindow::resized()
{
    Window::resized();

    const auto cardBounds = getCardBounds();
    constexpr float inset = 24.f;

    _layout.setMargin(
        static_cast<float>(cardBounds.getX()) + inset,
        static_cast<float>(cardBounds.getY()) + inset,
        static_cast<float>(getWidth() - cardBounds.getRight()) + inset,
        static_cast<float>(getHeight() - cardBounds.getBottom()) + inset);
    _layout.resized();

    constexpr int closeButtonSize = 24;
    _closeButton.setBounds(cardBounds.getRight() - closeButtonSize - 16, cardBounds.getY() + 24, closeButtonSize, closeButtonSize);

    constexpr int titleIconSize = 20;
    const auto titleCellBounds = _layout.getBounds(_title.getComponentID().toStdString());
    _titleIcon.setBounds(juce::Rectangle<float>(static_cast<float>(titleIconSize), static_cast<float>(titleIconSize))
        .withPosition(titleCellBounds.getX(), titleCellBounds.getCentreY() - static_cast<float>(titleIconSize) / 2.f + 2.f).toNearestInt());
}

juce::Rectangle<int> SettingsWindow::getCardBounds()
{
    constexpr int maxCardWidth = 650;
    constexpr int maxCardHeight = 400;

    const auto margin = juce::jmax(32, juce::jmin(getWidth(), getHeight()) / 8);
    const auto available = getLocalBounds().reduced(margin);

    return getLocalBounds().withSizeKeepingCentre(
        juce::jmin(available.getWidth(), maxCardWidth),
        juce::jmin(available.getHeight(), maxCardHeight));
}

void SettingsWindow::mouseDown(const juce::MouseEvent&)
{
    // Intentionally not forwarding to Window::mouseDown
}

void SettingsWindow::mouseUp(const juce::MouseEvent& event)
{
    if (!event.mouseWasClicked() || event.originalComponent != this)
        return;

    if (!getCardBounds().contains(event.getPosition()))
        close();
}

bool SettingsWindow::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        close();
        return true;
    }

    return Window::keyPressed(key);
}

void SettingsWindow::onButtonClick(const std::string& componentID)
{
    if (componentID == _closeButton.getComponentID())
        close();
}

void SettingsWindow::close()
{
    _windowsManager.hideWindow(getID());
}

}
