# Packages a Release build into a platform installer as part of the normal build (not a separate
# opt-in step) - `cmake --build --preset release` (or CI's equivalent Release build) produces
# "${PRODUCT_NAME}-${VERSION}-macOS.pkg" / "...-Windows.exe" under ${CMAKE_BINARY_DIR}/Packaging,
# alongside the existing ${PROJECT_NAME}_artefacts. A no-op for Debug builds.
#
# Signing/notarization use real credentials if DEVELOPER_ID_APPLICATION/DEVELOPER_ID_INSTALLER
# (macOS) are set in the environment at build time, else fall back to ad-hoc/unsigned - same
# graceful-degradation behavior locally and in CI (see .github/workflows/build_and_test.yml).

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(PACKAGING_OUTPUT_DIR "${CMAKE_BINARY_DIR}/Packaging")

    if (APPLE)
        set(PACKAGING_AU_PATH "${CMAKE_BINARY_DIR}/${PROJECT_NAME}_artefacts/Release/AU/${PRODUCT_NAME}.component")
        set(PACKAGING_VST3_PATH "${CMAKE_BINARY_DIR}/${PROJECT_NAME}_artefacts/Release/VST3/${PRODUCT_NAME}.vst3")
        set(PACKAGING_STANDALONE_PATH "${CMAKE_BINARY_DIR}/${PROJECT_NAME}_artefacts/Release/Standalone/${PRODUCT_NAME}.app")

        add_custom_target(package_installer ALL
                DEPENDS ${PROJECT_NAME}_AU ${PROJECT_NAME}_VST3 ${PROJECT_NAME}_Standalone
                COMMAND ${CMAKE_COMMAND} -E env
                        "PRODUCT_NAME=${PRODUCT_NAME}"
                        "BUNDLE_ID=${BUNDLE_ID}"
                        "VERSION=${VERSION}"
                        "AU_PATH=${PACKAGING_AU_PATH}"
                        "VST3_PATH=${PACKAGING_VST3_PATH}"
                        "STANDALONE_PATH=${PACKAGING_STANDALONE_PATH}"
                        "OUTPUT_DIR=${PACKAGING_OUTPUT_DIR}"
                        bash "${CMAKE_CURRENT_SOURCE_DIR}/Scripts/package_macos.sh"
                COMMENT "Packaging ${PRODUCT_NAME} (AU + VST3 + Standalone) into a macOS installer"
                VERBATIM
        )
    elseif (WIN32)
        find_program(ISCC_EXECUTABLE iscc)

        if (ISCC_EXECUTABLE)
            set(PACKAGING_ARTEFACTS_PATH "${CMAKE_BINARY_DIR}/${PROJECT_NAME}_artefacts/Release")

            add_custom_target(package_installer ALL
                    DEPENDS ${PROJECT_NAME}_VST3 ${PROJECT_NAME}_Standalone
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${PACKAGING_OUTPUT_DIR}"
                    COMMAND ${CMAKE_COMMAND} -E env
                            "PROJECT_NAME=${PROJECT_NAME}"
                            "PRODUCT_NAME=${PRODUCT_NAME}"
                            "VERSION=${VERSION}"
                            "BUNDLE_ID=${BUNDLE_ID}"
                            "COMPANY_NAME=${COMPANY_NAME}"
                            "${ISCC_EXECUTABLE}"
                            "/DArtefactsPath=${PACKAGING_ARTEFACTS_PATH}"
                            "/O${PACKAGING_OUTPUT_DIR}"
                            "${CMAKE_CURRENT_SOURCE_DIR}/Packaging/windows/installer.iss"
                    COMMENT "Packaging ${PRODUCT_NAME} (VST3 + Standalone) into a Windows installer"
                    VERBATIM
            )
        else ()
            message(WARNING "Inno Setup ('iscc') not found on PATH - skipping Windows installer "
                    "packaging for this Release build. Install it from https://jrsoftware.org/isdl.php "
                    "and reconfigure to enable it.")
        endif ()
    endif ()
endif ()
