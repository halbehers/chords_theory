#define CATCH_CONFIG_RUNNER
#include <catch2/catch_session.hpp>
#include <juce_gui_basics/juce_gui_basics.h>

int main(int argc, char* argv[])
{
    // Several JUCE internals (ValueTree, AudioProcessor, etc.) assume a message thread exists
    // even when just constructing an AudioProcessor headlessly for testing - this keeps that
    // assumption satisfied for the whole test run without needing an actual GUI/event loop.
    juce::ScopedJuceInitialiser_GUI juceInit;

    return Catch::Session().run(argc, argv);
}
