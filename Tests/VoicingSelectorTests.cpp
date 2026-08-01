#include <catch2/catch_test_macros.hpp>

#include "Component/VoicingSelector.h"
#include "Theory/ChordDatabase.h"

using component::VoicingSelector;
using theory::Chord;
using theory::ChordDatabase;
using theory::Key;
using theory::Scale;

namespace
{
    std::vector<Chord> getTestVoicings()
    {
        return ChordDatabase::getInstance().get(Key::C, Scale::Major).degrees.front().chords;
    }

    // Both nelement::SVGButton and nelement::TextButton wrap a single, always-first, internal
    // juce::Button child that actually receives clicks (confirmed by reading their .cpp: both
    // call addAndMakeVisible(_button) once in their constructor, before anything else) -
    // triggerClick() drives their real onClick handler without needing real mouse events or
    // exact on-screen geometry. juce::Button::triggerClick() only posts an async command message
    // (postCommandMessage) rather than invoking onClick synchronously, so the dispatch loop must
    // be pumped briefly afterward - same JUCE_MODAL_LOOPS_PERMITTED-gated pattern already used in
    // PluginProcessorTests.cpp for APVTS's async flush.
    void triggerButtonClick(juce::Component& buttonWrapper)
    {
        auto* button = dynamic_cast<juce::Button*>(buttonWrapper.getChildComponent(0));
        REQUIRE(button != nullptr);
        button->triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
    }
}

TEST_CASE("VoicingSelector::show populates the panel and makes it visible without throwing", "[VoicingSelector]")
{
    VoicingSelector selector("test-voicing-selector");
    selector.setBounds(0, 0, 400, 70);

    const auto voicings = getTestVoicings();
    REQUIRE_NOTHROW(selector.show(voicings, voicings.front().symbol, [](const Chord&) {}));

    CHECK(selector.isVisible());
}

TEST_CASE("VoicingSelector: clicking a voicing button invokes the callback and keeps the panel open", "[VoicingSelector]")
{
    VoicingSelector selector("test-voicing-selector");
    selector.setBounds(0, 0, 400, 70);

    const auto voicings = getTestVoicings();
    REQUIRE(voicings.size() > 1);
    const auto& target = voicings[1]; // deliberately not the current symbol

    int chosenCount = 0;
    std::string lastChosenSymbol;
    selector.show(voicings, voicings.front().symbol,
        [&chosenCount, &lastChosenSymbol](const Chord& chosen)
        {
            ++chosenCount;
            lastChosenSymbol = chosen.symbol;
        });

    auto* viewport = dynamic_cast<juce::Viewport*>(selector.getChildComponent(1));
    REQUIRE(viewport != nullptr);
    auto* voicingRow = viewport->getViewedComponent();
    REQUIRE(voicingRow != nullptr);

    auto* targetButton = voicingRow->findChildWithID(target.symbol);
    REQUIRE(targetButton != nullptr);
    triggerButtonClick(*targetButton);

    CHECK(chosenCount == 1);
    CHECK(lastChosenSymbol == target.symbol);
    CHECK(selector.isVisible()); // picking a voicing does not auto-close the panel
}

TEST_CASE("VoicingSelector: clicking the close button hides the panel", "[VoicingSelector]")
{
    VoicingSelector selector("test-voicing-selector");
    selector.setBounds(0, 0, 400, 70);

    const auto voicings = getTestVoicings();
    selector.show(voicings, voicings.front().symbol, [](const Chord&) {});
    REQUIRE(selector.isVisible());

    auto* closeButton = selector.findChildWithID("voicing-selector-close");
    REQUIRE(closeButton != nullptr);
    triggerButtonClick(*closeButton);

    CHECK_FALSE(selector.isVisible());
}

TEST_CASE("VoicingSelector::setArrowTargetX tolerates out-of-range values without crashing", "[VoicingSelector]")
{
    VoicingSelector selector("test-voicing-selector");
    selector.setBounds(0, 0, 400, 70);

    const auto voicings = getTestVoicings();
    selector.show(voicings, voicings.front().symbol, [](const Chord&) {});

    juce::Image image(juce::Image::ARGB, 400, 70, true);
    juce::Graphics g(image);

    selector.setArrowTargetX(-1000);
    REQUIRE_NOTHROW(selector.paint(g));

    selector.setArrowTargetX(100000);
    REQUIRE_NOTHROW(selector.paint(g));
}
