#include "StandaloneWindowStyling.h"

#include <juce_core/system/juce_TargetPlatform.h>

// The macOS implementation lives in StandaloneWindowStyling.mm (Objective-C++) instead.
#if JUCE_WINDOWS

#include <windows.h>
#include <dwmapi.h>

namespace standalone
{

void styleNativeTitleBar(juce::DocumentWindow& window, juce::Colour backgroundColour, float /*titleBarHeight*/)
{
    auto* peer = window.getPeer();
    if (peer == nullptr)
        return;

    auto hwnd = (HWND) peer->getNativeHandle();

    // DWMWA_CAPTION_COLOR (Windows 11 build 22000+) - hardcoded since older Windows SDKs' dwmapi.h
    // may not declare this enumerator yet. DwmSetWindowAttribute simply fails harmlessly on
    // earlier Windows versions that don't support it.
    constexpr DWORD dwmwaCaptionColor = 35;
    const COLORREF colour = RGB(backgroundColour.getRed(), backgroundColour.getGreen(), backgroundColour.getBlue());
    DwmSetWindowAttribute(hwnd, dwmwaCaptionColor, &colour, sizeof(colour));
}

void setNativeTitleVisible(juce::DocumentWindow& window, bool visible)
{
    auto* peer = window.getPeer();
    if (peer == nullptr)
        return;

    auto hwnd = (HWND) peer->getNativeHandle();

    // Blanking the caption text (rather than touching WS_CAPTION or the icon) leaves the icon and
    // min/maximize/close buttons exactly where they are - only the title text itself disappears.
    ::SetWindowTextW(hwnd, visible ? window.getName().toWideCharPointer() : L"");
}

}

#elif ! JUCE_MAC

namespace standalone
{
void styleNativeTitleBar(juce::DocumentWindow&, juce::Colour, float) {}
void setNativeTitleVisible(juce::DocumentWindow&, bool) {}
}

#endif
