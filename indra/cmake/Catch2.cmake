# Catch2.cmake
#
# Downloads the Catch2 v2.13.10 single-header on first configure and caches it.
# Defines:
#   CATCH2_INCLUDE_DIR  — directory containing catch.hpp
#   CATCH2_AVAILABLE    — TRUE if catch.hpp is present
#
# We deliberately use v2 (single-header) rather than v3 (compiled library) to
# keep the integration footprint small: no autobuild changes, no link target,
# no subdirectories. Tests just #include <catch.hpp> and use CATCH_CONFIG_MAIN.
#
# To switch to v3 later, this file is the only place that needs to change.

include_guard(GLOBAL)

set(CATCH2_VERSION "v2.13.10")
set(CATCH2_URL "https://github.com/catchorg/Catch2/releases/download/${CATCH2_VERSION}/catch.hpp")
set(CATCH2_DIR "${CMAKE_BINARY_DIR}/_deps/catch2/${CATCH2_VERSION}")
set(CATCH2_HEADER "${CATCH2_DIR}/catch.hpp")

if(NOT EXISTS "${CATCH2_HEADER}")
    file(MAKE_DIRECTORY "${CATCH2_DIR}")
    message(STATUS "Catch2: downloading ${CATCH2_VERSION} single-header")
    file(DOWNLOAD
        "${CATCH2_URL}"
        "${CATCH2_HEADER}"
        TIMEOUT 60
        STATUS _catch2_status
        TLS_VERIFY ON
    )
    list(GET _catch2_status 0 _catch2_status_code)
    if(NOT _catch2_status_code EQUAL 0)
        message(WARNING "Catch2 download failed: ${_catch2_status}. Catch2 tests will be skipped.")
        file(REMOVE "${CATCH2_HEADER}")
    else()
        # Sanity-check: a single-header Catch2 v2 is >100KB. If the file is
        # tiny we probably got an error page from GitHub.
        file(SIZE "${CATCH2_HEADER}" _catch2_size)
        if(_catch2_size LESS 100000)
            message(WARNING "Catch2: downloaded file is suspiciously small (${_catch2_size} bytes); removing.")
            file(REMOVE "${CATCH2_HEADER}")
        endif()
    endif()
endif()

if(EXISTS "${CATCH2_HEADER}")
    set(CATCH2_AVAILABLE TRUE)
    set(CATCH2_INCLUDE_DIR "${CATCH2_DIR}")
else()
    set(CATCH2_AVAILABLE FALSE)
endif()
