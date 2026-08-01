# End-to-end plugin validation via Tracktion's pluginval (https://github.com/Tracktion/pluginval),
# the de facto standard tool for exercising a real built plugin bundle end-to-end: parameter
# automation, state save/restore round-trips, thread safety, processBlock across sample
# rates/buffer sizes, and repeated editor open/close cycling. Downloaded once into the build tree
# (no manual install step) and registered as a CTest test so `ctest` alone covers both the Catch2
# unit tests and this.
if (APPLE)
    set(PLUGINVAL_DIR "${CMAKE_BINARY_DIR}/pluginval")
    set(PLUGINVAL_APP "${PLUGINVAL_DIR}/pluginval.app")
    set(PLUGINVAL_EXECUTABLE "${PLUGINVAL_APP}/Contents/MacOS/pluginval")

    if (NOT EXISTS "${PLUGINVAL_EXECUTABLE}")
        file(DOWNLOAD
                "https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_macOS.zip"
                "${PLUGINVAL_DIR}/pluginval_macOS.zip"
                SHOW_PROGRESS)
        file(ARCHIVE_EXTRACT INPUT "${PLUGINVAL_DIR}/pluginval_macOS.zip" DESTINATION "${PLUGINVAL_DIR}")

        # Ad-hoc sign, same as the plugin's own AU/VST3 bundles already get post-build (see the
        # "Replacing invalid signature with ad-hoc signature" step JUCE's own CMake support runs).
        execute_process(COMMAND codesign --force --deep --sign - "${PLUGINVAL_APP}")
    endif ()

    if (TARGET ${PROJECT_NAME}_AU)
        add_test(
                NAME pluginval_AU
                COMMAND "${PLUGINVAL_EXECUTABLE}"
                        --validate "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}_AU>"
                        --validate-in-process
                        --strictness-level 5
                        --timeout-ms 60000
        )
    endif ()
endif ()
