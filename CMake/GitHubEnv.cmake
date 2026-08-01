# Writes CMake project variables to .env for CI steps (installer packaging scripts, GitHub Actions
# workflow) to read via $GITHUB_ENV, so PRODUCT_NAME/VERSION/etc. live in one place (this file)
# instead of being duplicated/hardcoded in the workflow. No-op outside CI, so normal local dev
# builds are unaffected.
if (DEFINED ENV{CI})
    set(env_file "${CMAKE_CURRENT_SOURCE_DIR}/.env")
    file(WRITE  "${env_file}" "PROJECT_NAME=${PROJECT_NAME}\n")
    file(APPEND "${env_file}" "PRODUCT_NAME=${PRODUCT_NAME}\n")
    file(APPEND "${env_file}" "VERSION=${VERSION}\n")
    file(APPEND "${env_file}" "BUNDLE_ID=${BUNDLE_ID}\n")
    file(APPEND "${env_file}" "COMPANY_NAME=${COMPANY_NAME}\n")
endif ()
