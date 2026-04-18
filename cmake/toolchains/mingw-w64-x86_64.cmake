# Cross-compile for Windows x64 using mingw-w64.
# macOS:  brew install mingw-w64
# Debian: apt install g++-mingw-w64-x86-64 (prefer posix thread variant)
#
# Invoke with: -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake

set(CMAKE_SYSTEM_NAME      Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(MINGW_TRIPLET "x86_64-w64-mingw32" CACHE STRING "mingw-w64 triplet")

find_program(MINGW_C_COMPILER   NAMES ${MINGW_TRIPLET}-gcc   ${MINGW_TRIPLET}-clang)
find_program(MINGW_CXX_COMPILER NAMES ${MINGW_TRIPLET}-g++   ${MINGW_TRIPLET}-clang++)
find_program(MINGW_RC_COMPILER  NAMES ${MINGW_TRIPLET}-windres)

if(NOT MINGW_C_COMPILER OR NOT MINGW_CXX_COMPILER)
    message(FATAL_ERROR "mingw-w64 toolchain '${MINGW_TRIPLET}' not found on PATH")
endif()

set(CMAKE_C_COMPILER   "${MINGW_C_COMPILER}")
set(CMAKE_CXX_COMPILER "${MINGW_CXX_COMPILER}")
if(MINGW_RC_COMPILER)
    set(CMAKE_RC_COMPILER "${MINGW_RC_COMPILER}")
endif()

# Search for headers/libs only inside the mingw sysroot.
get_filename_component(_mingw_bin "${MINGW_CXX_COMPILER}" DIRECTORY)
get_filename_component(_mingw_root "${_mingw_bin}" DIRECTORY)
set(CMAKE_FIND_ROOT_PATH "${_mingw_root}/${MINGW_TRIPLET}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Static-link the mingw runtime so the resulting dll has no mingw dll deps.
set(_static_link "-static -static-libgcc -static-libstdc++")
set(CMAKE_EXE_LINKER_FLAGS_INIT    "${_static_link}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_static_link}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${_static_link}")
