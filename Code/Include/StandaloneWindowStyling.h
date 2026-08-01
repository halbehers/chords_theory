#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Standalone-window-only native title-bar styling that JUCE's public API doesn't expose:
// transparent/colour-matched title bar on macOS (with the content view extending into it, so the
// window's own content can paint through it, and the traffic-light buttons repositioned/kept
// positioned - see the .mm file), and a matching caption colour on Windows 11. A harmless no-op on
// any other platform.
namespace standalone
{

void styleNativeTitleBar(juce::DocumentWindow& window, juce::Colour backgroundColour, float titleBarHeight);

// Toggles the native OS-drawn title text in the standalone window's title bar (icon and
// min/maximize/close buttons are untouched). A harmless no-op on macOS, where the native title is
// already permanently hidden in favour of a custom overlay label instead (see StandaloneApp.cpp) -
// this only meaningfully does anything on Windows.
void setNativeTitleVisible(juce::DocumentWindow& window, bool visible);

}
