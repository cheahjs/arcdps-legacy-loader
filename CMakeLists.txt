cmake_minimum_required(VERSION 3.20)
project(arcdps_legacy_loader VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OUTPUT_NAME "arcdps_legacy_loader")
set(TARGET_NAME "arcdps_legacy_loader")

include(FetchContent)
FetchContent_Declare(
    imgui_180
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.80
)
FetchContent_MakeAvailable(imgui_180)

add_library(imgui_180 STATIC
    ${imgui_180_SOURCE_DIR}/imgui.cpp
    ${imgui_180_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_180_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_180_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_180_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_180_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_180_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui_180 PUBLIC
    ${imgui_180_SOURCE_DIR}
    ${imgui_180_SOURCE_DIR}/backends
)
target_compile_definitions(imgui_180 PRIVATE IMGUI_IMPL_WIN32_DISABLE_GAMEPAD)

add_library(${TARGET_NAME} SHARED
    src/dllmain.cpp
    src/exports.cpp
    src/addon/legacy_addon.cpp
    src/addon/addon_manager.cpp
    src/config/config.cpp
    src/imgui_legacy/context.cpp
    src/proxy/arcdps_proxy.cpp
    src/logging/log.cpp
    src/ui/settings_window.cpp
    def/arcdps_legacy_loader.def
)

set_target_properties(${TARGET_NAME} PROPERTIES
    OUTPUT_NAME "${OUTPUT_NAME}"
    PREFIX ""
    SUFFIX ".dll"
)

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(${TARGET_NAME} PRIVATE imgui_180 d3d11 dxgi d3dcompiler)

if(MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE /W4 /permissive-)
    target_compile_options(imgui_180      PRIVATE /W3)
else()
    # mingw-w64 / clang-mingw path.
    target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Wno-unknown-pragmas)
    # mingw needs uuid for __uuidof on D3D/DXGI interfaces and dxguid for
    # GUIDs referenced without __uuidof usage fallbacks.
    target_link_libraries(${TARGET_NAME} PRIVATE uuid dxguid)
    # Strip the "lib" prefix mingw adds to shared libraries by default.
    set_target_properties(${TARGET_NAME} PROPERTIES PREFIX "")
endif()
