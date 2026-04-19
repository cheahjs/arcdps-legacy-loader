# Derives version strings from git at configure time.
#
# Sets in the parent scope:
#   ARCDPS_LL_VERSION_STRING  — human-readable (e.g. "v0.2.1", "v0.2.1-5-gabcdef-dirty", "unknown")
#   ARCDPS_LL_VERSION_COMMIT  — short commit sha (or "unknown")
#   ARCDPS_LL_VERSION_MAJOR/MINOR/PATCH/TWEAK — integers suitable for Win32 VERSIONINFO.
#     TWEAK is the commit count since the nearest v* tag (0 on a clean tag).
#
# Re-runs when HEAD moves by attaching .git/HEAD and the resolved ref file as
# CMAKE_CONFIGURE_DEPENDS. Tag-only changes still require a manual reconfigure.

find_package(Git QUIET)

set(ARCDPS_LL_VERSION_STRING "unknown")
set(ARCDPS_LL_VERSION_COMMIT "unknown")
set(ARCDPS_LL_VERSION_MAJOR  0)
set(ARCDPS_LL_VERSION_MINOR  0)
set(ARCDPS_LL_VERSION_PATCH  0)
set(ARCDPS_LL_VERSION_TWEAK  0)

if(Git_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --long --always --dirty --match "v*"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE _git_describe
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _git_rc)

    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE _git_sha
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)

    if(_git_rc EQUAL 0 AND _git_describe)
        # --long form: vMAJOR.MINOR.PATCH-COUNT-gSHA[-dirty]. If no v* tag exists
        # git falls back to just the short sha (optionally "-dirty").
        if(_git_describe MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)-([0-9]+)-g[0-9a-f]+(-dirty)?$")
            set(ARCDPS_LL_VERSION_MAJOR ${CMAKE_MATCH_1})
            set(ARCDPS_LL_VERSION_MINOR ${CMAKE_MATCH_2})
            set(ARCDPS_LL_VERSION_PATCH ${CMAKE_MATCH_3})
            set(ARCDPS_LL_VERSION_TWEAK ${CMAKE_MATCH_4})
            if(CMAKE_MATCH_4 STREQUAL "0" AND NOT CMAKE_MATCH_5)
                # Clean build exactly on a tag — collapse to "vX.Y.Z".
                set(ARCDPS_LL_VERSION_STRING "v${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}")
            else()
                set(ARCDPS_LL_VERSION_STRING "${_git_describe}")
            endif()
        else()
            set(ARCDPS_LL_VERSION_STRING "${_git_describe}")
        endif()
    endif()

    if(_git_sha)
        set(ARCDPS_LL_VERSION_COMMIT "${_git_sha}")
    endif()

    # Re-run CMake configure whenever HEAD changes.
    set(_head_file "${CMAKE_SOURCE_DIR}/.git/HEAD")
    if(EXISTS "${_head_file}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_head_file}")
        file(READ "${_head_file}" _head_contents)
        string(STRIP "${_head_contents}" _head_contents)
        if(_head_contents MATCHES "^ref: (.+)$")
            set(_ref_file "${CMAKE_SOURCE_DIR}/.git/${CMAKE_MATCH_1}")
            if(EXISTS "${_ref_file}")
                set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_ref_file}")
            endif()
        endif()
    endif()
endif()

message(STATUS "arcdps_legacy_loader version: ${ARCDPS_LL_VERSION_STRING}")
