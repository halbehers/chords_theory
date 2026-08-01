#include <juce_core/system/juce_TargetPlatform.h>

#if JUCE_MAC

#include "StandaloneWindowStyling.h"

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

namespace standalone
{

namespace
{
    NSWindow* getNSWindow(juce::DocumentWindow& window)
    {
        auto* peer = window.getPeer();
        if (peer == nullptr)
            return nil;

        auto* view = static_cast<NSView*>(peer->getNativeHandle());
        if (view == nil)
            return nil;

        return view.window;
    }
}

}

// Keeps the traffic lights repositioned for as long as the window lives. AppKit re-lays them out
// to their own cramped default position on its own whenever the window is shown/made key or
// resized - including continuously throughout an interactive/live resize drag - so a one-off
// repositioning (or one merely deferred a run-loop turn via dispatch_async, which doesn't reliably
// get serviced while AppKit's live-resize tracking loop is running) gets silently undone moments
// later. Observing NSWindowDidResizeNotification reacts synchronously, on the very native event
// that causes AppKit's own reset, so there's no race to lose.
//
// This project's Objective-C++ files aren't built with ARC (see JUCE's own .mm files, which use
// explicit retain/release throughout), so this is written the same way: a bare (non-retaining)
// pointer back to the window - retaining it would create a reference cycle, since the window is
// what keeps this object alive in the first place, via objc_setAssociatedObject below.
@interface AppInDAWTrafficLightPositioner : NSObject
- (instancetype) initWithWindow: (NSWindow*) window;
@end

@implementation AppInDAWTrafficLightPositioner
{
    NSWindow* _window;
    NSTimer* _startupTimer;
    int _startupTicksRemaining;
    // AppKit's own native y for each button, captured the first time it's seen (before this object
    // ever touches it) - every reposition call after that computes y as an absolute offset from
    // this fixed baseline, never from the button's current (possibly already-adjusted-by-us) frame,
    // so repeated calls (the startup timer, multiple resize notifications, ...) stay idempotent
    // instead of the offset compounding further on every call.
    NSMapTable<NSButton*, NSNumber*>* _baselineYByButton;
}

- (instancetype) initWithWindow: (NSWindow*) window
{
    self = [super init];
    if (self != nil)
    {
        _window = window;
        _baselineYByButton = [[NSMapTable weakToStrongObjectsMapTable] retain];
        [[NSNotificationCenter defaultCenter] addObserver: self
                                                  selector: @selector (reposition)
                                                      name: NSWindowDidResizeNotification
                                                    object: window];
        [[NSNotificationCenter defaultCenter] addObserver: self
                                                  selector: @selector (reposition)
                                                      name: NSWindowDidBecomeKeyNotification
                                                    object: window];

        // Neither of the above notifications reliably catches whatever internal layout pass
        // AppKit does to the traffic lights around window creation/first show (tried both, the
        // buttons still end up back at their cramped default at launch) - rather than keep
        // guessing at its exact timing, brute-force past it by reasserting our desired position on
        // a short timer for the first second after creation, which wins regardless of exactly
        // when AppKit's own pass happens.
        _startupTicksRemaining = 20;
        _startupTimer = [[NSTimer scheduledTimerWithTimeInterval: 0.05
                                                            target: self
                                                          selector: @selector (startupTick)
                                                          userInfo: nil
                                                           repeats: YES] retain];
        [self reposition];
    }
    return self;
}

- (void) dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver: self];
    [_startupTimer invalidate];
    [_startupTimer release];
    [_baselineYByButton release];
    [super dealloc];
}

- (void) startupTick
{
    [self reposition];

    if (--_startupTicksRemaining <= 0)
    {
        [_startupTimer invalidate];
        [_startupTimer release];
        _startupTimer = nil;
    }
}

- (void) positionButton: (NSButton*) button x: (CGFloat) x
{
    if (button == nil)
        return;

    NSNumber* baseline = [_baselineYByButton objectForKey: button];
    NSRect frame = button.frame;

    if (baseline == nil)
    {
        baseline = @(frame.origin.y);
        [_baselineYByButton setObject: baseline forKey: button];
    }

    frame.origin.x = x;
    // More padding from the top edge - empirically, *increasing* y moved the buttons closer to
    // the top instead of further away, so this subtracts rather than adds.
    frame.origin.y = baseline.doubleValue - 8.0;
    button.frame = frame;

    // Hovering near any one traffic light highlights all three together - that grouped hover
    // effect is driven by a tracking area on a shared ancestor view, not on each button's own
    // bounds, so invalidating just the button itself isn't enough (confirmed - it wasn't). Walk up
    // to every ancestor and invalidate each one, since the private AppKit class that actually owns
    // that tracking area isn't known here.
    for (NSView* view = button; view != nil; view = view.superview)
    {
        [view.window invalidateCursorRectsForView: view];
        [view updateTrackingAreas];
    }
}

- (void) reposition
{
    if (_window == nil)
        return;

    constexpr CGFloat xInset = 22.0;
    constexpr CGFloat spacing = 20.0;

    [self positionButton: [_window standardWindowButton: NSWindowCloseButton] x: xInset];
    [self positionButton: [_window standardWindowButton: NSWindowMiniaturizeButton] x: xInset + spacing];
    [self positionButton: [_window standardWindowButton: NSWindowZoomButton] x: xInset + spacing * 2.0];
}

@end

namespace standalone
{

void styleNativeTitleBar(juce::DocumentWindow& window, juce::Colour backgroundColour, float)
{
    NSWindow* nsWindow = getNSWindow(window);
    if (nsWindow == nil)
        return;

    // Let JUCE's own content paint through where the title bar used to be, instead of the OS
    // drawing a separate grey strip above it.
    nsWindow.styleMask |= NSWindowStyleMaskFullSizeContentView;
    nsWindow.titlebarAppearsTransparent = YES;
    nsWindow.titleVisibility = NSWindowTitleHidden;

    // With the title bar transparent, the window's own backgroundColor (AppKit's default is a
    // light system colour) shows through anywhere JUCE's content view doesn't itself paint - a
    // sub-pixel gap right at the top edge was showing up as a thin white line. Matching it to the
    // app's own theme background means even that sliver is the right colour.
    nsWindow.backgroundColor = [NSColor colorWithSRGBRed: static_cast<CGFloat>(backgroundColour.getFloatRed())
                                                     green: static_cast<CGFloat>(backgroundColour.getFloatGreen())
                                                      blue: static_cast<CGFloat>(backgroundColour.getFloatBlue())
                                                     alpha: 1.0];

    // backgroundColor alone didn't get rid of that line - AppKit also draws its own subtle
    // highlight along a titled window's top border, coloured for whichever appearance (light/dark)
    // the window is currently using. The window doesn't infer "dark" just because its content
    // happens to be dark-filled, so without this it keeps using its light-appearance highlight
    // colour, which reads as a stark white line against dark content.
    nsWindow.appearance = [NSAppearance appearanceNamed: backgroundColour.getPerceivedBrightness() < 0.5f
                                                          ? NSAppearanceNameDarkAqua
                                                          : NSAppearanceNameAqua];

    // Belt-and-braces: the caller also implements dragging manually (JUCE's content view now
    // covers this area and owns its own mouse events instead of the OS), but this costs nothing
    // to also enable.
    nsWindow.movableByWindowBackground = YES;

    static char positionerAssociationKey;
    auto* positioner = [[AppInDAWTrafficLightPositioner alloc] initWithWindow: nsWindow];
    objc_setAssociatedObject (nsWindow, &positionerAssociationKey, positioner, OBJC_ASSOCIATION_RETAIN);
    [positioner release];
}

// No-op here: the native title is already permanently hidden by styleNativeTitleBar() above
// (NSWindowTitleHidden), in favour of StandaloneApp.cpp's own _titleLabel overlay - there's no
// native title text left for this to toggle on macOS.
void setNativeTitleVisible(juce::DocumentWindow&, bool) {}

}

#endif // JUCE_MAC
