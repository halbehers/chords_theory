add_executable(${PROJECT_NAME}_Tests
        AppSettingsTests.cpp
        PluginProcessorTests.cpp
        ChordDatabaseTests.cpp
        ChordIdentifierTests.cpp
        NoteConvertorTests.cpp
        ProgressionPresetTests.cpp
        MidiExporterTests.cpp
        SessionStateSerializerTests.cpp
        ChordDegreeBrowserTests.cpp
        ChordSynthEngineTests.cpp
        ProgressionPlayerTests.cpp
        ChordCardTests.cpp
        VoicingSelectorTests.cpp
        ProgressionEditorTests.cpp
        ProgressionPresetIntegrationTests.cpp
        DragOutButtonTests.cpp
        DahdsrEnvelopeTests.cpp
        VoiceFilterTests.cpp
        LfoTests.cpp
        OscillatorTests.cpp
        SubOscillatorTests.cpp
        SynthAlgorithmTests.cpp
        MasterCompressorTests.cpp
        MidiEditorTests.cpp
        SectionPanelChangeTests.cpp
        ProgressionTimelineTests.cpp
        HistoryManagerTests.cpp
        main.cpp
)

target_link_libraries(${PROJECT_NAME}_Tests PRIVATE
        ${PROJECT_NAME}
        Catch2::Catch2
        juce::juce_recommended_warning_flags
)

# Test-only: lets PluginProcessorTests.cpp pump the message loop with
# MessageManager::runDispatchLoopUntil() to force AudioProcessorValueTreeState's async
# parameter-to-ValueTree flush (its internal 10Hz timer) before asserting on saved state. Never
# defined for the shipping plugin target itself - blocking modal loops have no place there.
target_compile_definitions(${PROJECT_NAME}_Tests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)

# Catch2's CMake extras (Catch.cmake, providing catch_discover_tests) aren't on CMAKE_MODULE_PATH
# automatically when Catch2 is fetched via CPM/FetchContent as source, unlike an installed package.
list(APPEND CMAKE_MODULE_PATH "${CPM_PACKAGE_CATCH2_SOURCE_DIR}/extras")

include(CTest)
include(Catch)
catch_discover_tests(${PROJECT_NAME}_Tests)
