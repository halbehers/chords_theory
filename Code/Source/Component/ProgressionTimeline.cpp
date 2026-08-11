#include "Component/ProgressionTimeline.h"

#include <algorithm>
#include <cmath>

#include "Component/MidiEditor.h"

namespace component
{

namespace
{
    // One page of the timeline is a full measure - 4 bars - not a single bar.
    constexpr int kBarsPerMeasure = 4;
    constexpr double kBeatsPerMeasure = static_cast<double>(kBarsPerMeasure) * MidiEditor::kBeatsPerBar;

    // Same threshold DragOutButton/ChordCard use for their own click-vs-drag gestures.
    constexpr float kDragStartThreshold = 6.f;
}

ProgressionTimeline::ProgressionTimeline(const std::string& identifier, ProgressionEditor& progressionEditor,
                                          audio::ProgressionPlayer* progressionPlayer):
    Component(identifier),
    _progressionEditor(progressionEditor),
    _progressionPlayer(progressionPlayer)
{
    displayBackground(nui::Theme::ThemeColor::BACKGROUND, nui::Theme::getBorderRadius());

    setMouseCursor(juce::MouseCursor::DraggingHandCursor);

    applyHeightType();

    _progressionEditor.addListener(this);
}

ProgressionTimeline::~ProgressionTimeline()
{
    _progressionEditor.removeListener(this);
}

void ProgressionTimeline::resized()
{
    Component::resized();
    applyHeightType();
}

void ProgressionTimeline::mouseDown(const juce::MouseEvent& event)
{
    _dragGestureStarted = false;
    _pressedChordBlockIndex = hitTestChordBlock(event.position);
}

void ProgressionTimeline::mouseDrag(const juce::MouseEvent& event)
{
    if (_dragGestureStarted || _pressedChordBlockIndex < 0)
        return;

    if (static_cast<float>(event.getDistanceFromDragStart()) < kDragStartThreshold)
        return;

    _dragGestureStarted = true;

    for (auto* listener : _listeners)
        listener->onChordBlockDragStarted(_pressedChordBlockIndex);
}

void ProgressionTimeline::addListener(Listener* listener)
{
    _listeners.push_back(listener);
}

void ProgressionTimeline::removeListener(Listener* listener)
{
    std::erase(_listeners, listener);
}

void ProgressionTimeline::setHeightType(nui::Theme::HeightType type)
{
    _heightType = type;
    applyHeightType();
    repaint();
}

void ProgressionTimeline::applyHeightType()
{
    // Only THIN/LARGE resolve to a height different from whatever we're actually given - AUTO
    // resolves to getHeight() itself, so this is a no-op margin of 0 in that (default) case.
    const auto resolvedHeight = nui::Theme::resolveHeight(_heightType, static_cast<float>(getHeight()));
    const auto verticalMargin = juce::jmax(0.f, (static_cast<float>(getHeight()) - resolvedHeight) / 2.f);
    setMargin(0.f, verticalMargin, 0.f, verticalMargin);
}

void ProgressionTimeline::timerCallback()
{
    repaint();
}

void ProgressionTimeline::onContentChanged()
{
    repaint();
}

void ProgressionTimeline::onPlaybackStateChanged(bool isPlaying)
{
    if (isPlaying)
        startTimerHz(45);
    else
        stopTimer();

    repaint();
}

double ProgressionTimeline::getCurrentMeasureStart() const noexcept
{
    const auto playheadBeat = _progressionPlayer != nullptr ? _progressionPlayer->getPlayheadBeat() : 0.0;
    const auto currentMeasure = std::floor(playheadBeat / kBeatsPerMeasure);
    return currentMeasure * kBeatsPerMeasure;
}

float ProgressionTimeline::beatToX(double beat) noexcept
{
    const auto bounds = getLocalBounds().toFloat();
    return bounds.getX() + static_cast<float>((beat - getCurrentMeasureStart()) / kBeatsPerMeasure) * bounds.getWidth();
}

int ProgressionTimeline::hitTestChordBlock(juce::Point<float> position)
{
    const auto measureStart = getCurrentMeasureStart();
    const auto measureEnd = measureStart + kBeatsPerMeasure;

    const auto chordCount = _progressionEditor.getChordBlockCount();
    for (int i = 0; i < chordCount; ++i)
    {
        const auto startBeat = _progressionEditor.getChordBlockStartBeat(i);
        const auto lengthBeats = _progressionEditor.getChordBlockLengthBeats(i);
        if (!startBeat || !lengthBeats)
            continue;

        const auto blockEnd = *startBeat + *lengthBeats;
        if (blockEnd <= measureStart || *startBeat >= measureEnd)
            continue; // doesn't overlap the measure currently on screen

        const auto clippedStart = juce::jmax(*startBeat, measureStart);
        const auto clippedEnd = juce::jmin(blockEnd, measureEnd);

        if (position.x >= beatToX(clippedStart) && position.x <= beatToX(clippedEnd))
            return i;
    }

    return -1;
}

void ProgressionTimeline::paint(juce::Graphics& g)
{
    Component::paint(g);

    const auto bounds = getLocalBounds().toFloat();
    const auto measureStart = getCurrentMeasureStart();
    const auto measureEnd = measureStart + kBeatsPerMeasure;

    const auto accent = nui::Theme::newColor(nui::Theme::ThemeColor::ACCENT).asJuce();
    g.setFont(nui::Theme::newFont(nui::Theme::REGULAR, nui::Theme::SMALL));

    const auto chordCount = _progressionEditor.getChordBlockCount();
    for (int i = 0; i < chordCount; ++i)
    {
        const auto startBeat = _progressionEditor.getChordBlockStartBeat(i);
        const auto lengthBeats = _progressionEditor.getChordBlockLengthBeats(i);
        const auto label = _progressionEditor.getChordBlockLabel(i);
        if (!startBeat || !lengthBeats || !label)
            continue;

        const auto blockEnd = *startBeat + *lengthBeats;
        if (blockEnd <= measureStart || *startBeat >= measureEnd)
            continue; // doesn't overlap the measure currently on screen

        const auto clippedStart = juce::jmax(*startBeat, measureStart);
        const auto clippedEnd = juce::jmin(blockEnd, measureEnd);

        const juce::Rectangle<float> segmentBounds(beatToX(clippedStart) + 2.f, bounds.getY() + 2.f,
            beatToX(clippedEnd) - beatToX(clippedStart) - 4.f, bounds.getHeight() - 4.f);
        if (segmentBounds.getWidth() <= 0.f)
            continue;

        g.setColour(accent.withAlpha(0.2f));
        g.fillRoundedRectangle(segmentBounds, 4.f);
        g.setColour(accent);
        g.drawRoundedRectangle(segmentBounds, 4.f, 1.f);

        g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::TEXT).asJuce());
        g.drawText(*label, segmentBounds, juce::Justification::centred, true);
    }

    if (chordCount == 0)
        return; // nothing placed yet - no meaningful playhead position to show either

    const auto playheadBeat = _progressionPlayer != nullptr ? _progressionPlayer->getPlayheadBeat() : 0.0;
    const auto x = beatToX(playheadBeat);

    g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::TEXT).asJuce());
    g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());

    juce::Path flag;
    flag.addTriangle(x - 4.f, bounds.getY(), x + 4.f, bounds.getY(), x, bounds.getY() + 8.f);
    g.fillPath(flag);
}

}
