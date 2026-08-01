#include <juce_core/system/juce_TargetPlatform.h>

#if JucePlugin_Build_Standalone

// juce_StandaloneFilterWindow.h isn't includable on its own - it assumes the same preamble
// juce_audio_plugin_client_Standalone.cpp (the stock file this replaces) sets up first, so this
// mirrors that file's include order exactly.
#include <juce_audio_plugin_client/detail/juce_CheckSettingMacros.h>
#include <juce_audio_plugin_client/detail/juce_IncludeSystemHeaders.h>
#include <juce_audio_plugin_client/detail/juce_IncludeModuleHeaders.h>
#include <juce_gui_basics/native/juce_WindowsHooks_windows.h>
#include <juce_audio_plugin_client/detail/juce_PluginUtilities.h>

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

// juce_IncludeModuleHeaders.h (above) leaves "#define Component juce::Component" (and, on macOS,
// "#define Point juce::Point") active for the rest of the translation unit - fine for JUCE's own
// stock file which never includes anything else afterward, but it silently mangles nierika_dsp's
// own Component/Point declarations below if left in place.
#undef Component
#if JUCE_MAC
 #undef Point
#endif

#include <nierika_dsp/nierika_dsp.h>

#include "AppSettings.h"
#include "StandaloneWindowStyling.h"

namespace
{

constexpr int titleBarHeight = 40;
// Roughly clears the native traffic-light button cluster (see StandaloneWindowStyling.mm's own
// button-inset constants) so this doesn't steal clicks meant for them.
constexpr int titleBarDragRegionLeftInset = 76;

// The former title-bar strip is now just ordinary JUCE content (see StandaloneWindowStyling.mm),
// so it no longer triggers the OS's own "drag the title bar to move the window" gesture - this
// reimplements that manually using JUCE's own mouse events, which is what actually owns them now.
class TitleBarDragRegion final : public juce::Component
{
public:
    void mouseDown (const juce::MouseEvent& e) override
    {
        _dragger.startDraggingComponent (getTopLevelComponent(), e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        _dragger.dragComponent (getTopLevelComponent(), e, nullptr);
    }

private:
    juce::ComponentDragger _dragger;
};

// Mirrors juce::StandaloneFilterApp (juce_audio_plugin_client_Standalone.cpp) with two behavioural
// additions: the window opts into native OS title bar chrome instead of JUCE's own custom-drawn
// title bar, and standalone::styleNativeTitleBar() (StandaloneWindowStyling.h) makes that native
// title bar match the app's own background colour, with the app name drawn centered on top of it
// via _titleLabel (on macOS the content view extends into the former title-bar strip - see that
// file's comments; on Windows only the caption colour changes, the native title text stays put,
// so _titleLabel is only added on macOS).
//
// Going native also collapses JUCE's own title-bar row to zero height
// (DocumentWindow::getTitleBarHeight() returns 0 once native title bar is active), which in turn
// collapses the hard-coded "Options" button - positioned at (8, 6, 60, getTitleBarHeight() - 8)
// inside StandaloneFilterWindow itself - to a zero-size, invisible, non-interactive rect. This app
// has its own Settings UI, so that button (and the audio device dialog it opened) is redundant.
class StandaloneApp final : public juce::JUCEApplication, private juce::ComponentListener
{
public:
    StandaloneApp()
    {
        juce::PropertiesFile::Options options;

        options.applicationName     = juce::CharPointer_UTF8 (JucePlugin_Name);
        options.filenameSuffix      = ".settings";
        options.osxLibrarySubFolder = "Application Support";
        options.folderName          = "";

        appProperties.setStorageParameters (options);

        _titleLabel.setText (JucePlugin_Name, false);
        _titleLabel.setJustificationType (juce::Justification::centred);
        _titleLabel.setColor (nui::Theme::ThemeColor::TEXT);
        _titleLabel.setFontWeight (nui::Theme::MEDIUM);
        _titleLabel.setFontSize (nui::Theme::CAPTION);
        _titleLabel.setInterceptsMouseClicks (false, false);
    }

    const juce::String getApplicationName() override             { return juce::CharPointer_UTF8 (JucePlugin_Name); }
    const juce::String getApplicationVersion() override          { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override                   { return true; }
    void anotherInstanceStarted (const juce::String&) override   {}

    void initialise (const juce::String&) override
    {
        if (juce::Desktop::getInstance().getDisplays().displays.isEmpty())
        {
            jassertfalse; // No displays are available, so no window will be created!
            return;
        }

        const auto backgroundColour = nui::Theme::newColor (nui::Theme::ThemeColor::BACKGROUND).asJuce();

        mainWindow = std::make_unique<juce::StandaloneFilterWindow> (
            getApplicationName(), backgroundColour,
            appProperties.getUserSettings(), false);

        // Re-read the theme colour now that mainWindow's construction has run the plugin
        // processor's constructor (which is what actually calls nui::Theme::configure() with the
        // app's real palette) - backgroundColour above was captured before that ran, so it's still
        // nierika_dsp's built-in default preset background, not this app's configured one. Only
        // matters on Windows: styleNativeTitleBar() ignores this argument entirely on macOS (it
        // makes the title bar transparent and lets the correctly-themed content paint through
        // instead), so this staleness was invisible there.
        const auto captionColour = nui::Theme::newColor (nui::Theme::ThemeColor::BACKGROUND).asJuce();

        mainWindow->setUsingNativeTitleBar (true);
        standalone::styleNativeTitleBar (*mainWindow, captionColour, static_cast<float>(titleBarHeight));
        AppSettings::getInstance().setShowStandaloneTitle(false);
        // No-op on macOS - the native title there is already permanently hidden in favour of
        // _titleLabel below, which is what actually reads this setting on that platform.
        standalone::setNativeTitleVisible (*mainWindow, AppSettings::getInstance().getShowStandaloneTitle());

       #if JUCE_MAC
        // ResizableWindow re-declares addAndMakeVisible as protected (it normally only wants a
        // single managed "content component") - cast to the base Component type, where it's
        // still genuinely public, to add this as a plain extra child.
        auto& windowAsComponent = static_cast<juce::Component&> (*mainWindow);
        windowAsComponent.addAndMakeVisible (_dragRegion);
        windowAsComponent.addAndMakeVisible (_titleLabel);
        _titleLabel.setVisible (AppSettings::getInstance().getShowStandaloneTitle());
        mainWindow->addComponentListener (this);
        positionTitleBarChildren();
       #endif

        mainWindow->setVisible (true);

       #if JUCE_MAC
        // The traffic lights' hover/click tracking region doesn't follow their repositioned frame
        // at launch (confirmed: several direct AppKit calls to force that update had no effect),
        // but a real user-driven resize reliably fixes it. Rather than keep guessing at which
        // native call replicates that, trigger a real (1px, effectively invisible) resize
        // ourselves once the window has had a moment to fully settle, piggybacking on the exact
        // mechanism already known to work.
        juce::Timer::callAfterDelay (600, [safePointer = juce::Component::SafePointer<juce::StandaloneFilterWindow> (mainWindow.get())]
        {
            if (safePointer == nullptr)
                return;

            const auto bounds = safePointer->getBounds();
            safePointer->setBounds (bounds.withWidth (bounds.getWidth() + 1));
            safePointer->setBounds (bounds);
        });
       #endif
    }

    void shutdown() override
    {
       #if JUCE_MAC
        if (mainWindow != nullptr)
            mainWindow->removeComponentListener (this);
       #endif

        mainWindow = nullptr;
        appProperties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (mainWindow != nullptr)
            mainWindow->pluginHolder->savePluginState();

        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            juce::Timer::callAfterDelay (100, []()
            {
                if (auto* app = juce::JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            });
        }
        else
        {
            quit();
        }
    }

private:
   #if JUCE_MAC
    void componentMovedOrResized (juce::Component&, bool /*wasMoved*/, bool wasResized) override
    {
        if (wasResized)
            positionTitleBarChildren();
    }

    void positionTitleBarChildren()
    {
        if (mainWindow == nullptr)
            return;

        const auto width = mainWindow->getWidth();
        _titleLabel.setBounds (0, 0, width, titleBarHeight);
        _dragRegion.setBounds (titleBarDragRegionLeftInset, 0, juce::jmax (0, width - titleBarDragRegionLeftInset), titleBarHeight);
    }
   #endif

    juce::ApplicationProperties appProperties;
    std::unique_ptr<juce::StandaloneFilterWindow> mainWindow;
    nelement::Text _titleLabel { "standalone-title-bar-label" };
    TitleBarDragRegion _dragRegion;
};

}

START_JUCE_APPLICATION (StandaloneApp)

#endif // JucePlugin_Build_Standalone
