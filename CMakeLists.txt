cmake_minimum_required(VERSION 3.20)
project(arcdps_legacy_loader VERSION 0.1.0 LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(OUTPUT_NAME "arcdps_legacy_loader")
set(TARGET_NAME "arcdps_legacy_loader")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(GitVersion)

set(GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
configure_file(
    ${CMAKE_SOURCE_DIR}/src/version.h.in
    ${GENERATED_DIR}/version.h
    @ONLY)
configure_file(
    ${CMAKE_SOURCE_DIR}/src/version.rc.in
    ${GENERATED_DIR}/version.rc
    @ONLY)

include(FetchContent)
FetchContent_Declare(
    imgui_180
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.80
)
FetchContent_MakeAvailable(imgui_180)

# Vendored ImGui 1.92.7 headers — used only for struct definitions so the
# reader can cast arcdps's ImGuiContext* and read ->Style directly. We do
# NOT compile imgui's sources: attempting to link both 1.80 and 1.92
# implementations into one DLL produces ODR collisions on every global-
# namespace imgui class method (ImDrawList::AddText, ImFontAtlas::…, etc.)
# that the ImGui:: namespace rename can't touch. Headers-only sidesteps
# that entirely at the cost of losing the DebugCheckVersionAndDataLayout
# safety net — the reader compensates with value sanity checks.
FetchContent_Declare(
    imgui_1927
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.7
)
FetchContent_MakeAvailable(imgui_1927)

add_library(imgui_1927 OBJECT
    src/imgui_legacy/arc_style_reader.cpp
    src/imgui_legacy/arc_windows_reader.cpp
)
target_include_directories(imgui_1927 PRIVATE
    ${imgui_1927_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
# Keep the 1.80 backends' IO tweak in sync for struct layout compatibility
# with arcdps's build (stock imconfig, no gamepad enable).
target_compile_definitions(imgui_1927 PRIVATE IMGUI_DISABLE_OBSOLETE_FUNCTIONS=0)

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
    src/imgui_legacy/arc_style_apply.cpp
    src/imgui_legacy/arc_windows_mirror.cpp
    src/proxy/arcdps_proxy.cpp
    src/logging/log.cpp
    src/ui/settings_window.cpp
    def/arcdps_legacy_loader.def
    ${GENERATED_DIR}/version.rc
    $<TARGET_OBJECTS:imgui_1927>
)

set_target_properties(${TARGET_NAME} PROPERTIES
    OUTPUT_NAME "${OUTPUT_NAME}"
    PREFIX ""
    SUFFIX ".dll"
)

target_include_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${GENERATED_DIR}
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

# ----------------------------------------------------------------------------
# Optional debug harness: a separate arcdps addon DLL that drives the loader's
# init/release cycle via arcdps's addextension2/removeextension2 to reproduce
# teardown crashes. Not built by default; build with -DARCDPS_LL_HARNESS=ON.
# ----------------------------------------------------------------------------
option(ARCDPS_LL_HARNESS "Build the crash-reload harness addon" OFF)

if(ARCDPS_LL_HARNESS)
    # Full imgui 1.92.7 static lib for the harness. Distinct from the
    # imgui_1927 OBJECT lib above (which compiles two reader translation
    # units against the 1.92.7 headers without pulling in imgui's own
    # sources). The harness needs the real Begin/Text/etc. implementations,
    # but lives in its own DLL so there's no ODR collision with the
    # loader's imgui_180.
    add_library(imgui_1927_full STATIC
        ${imgui_1927_SOURCE_DIR}/imgui.cpp
        ${imgui_1927_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_1927_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_1927_SOURCE_DIR}/imgui_widgets.cpp
    )
    target_include_directories(imgui_1927_full PUBLIC ${imgui_1927_SOURCE_DIR})

    add_library(arcdps_legacy_loader_harness SHARED
        harness/src/main.cpp
        harness/def/arcdps_legacy_loader_harness.def
    )
    target_link_libraries(arcdps_legacy_loader_harness PRIVATE imgui_1927_full)
    target_include_directories(arcdps_legacy_loader_harness PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    set_target_properties(arcdps_legacy_loader_harness PROPERTIES
        OUTPUT_NAME "arcdps_legacy_loader_harness"
        PREFIX ""
        SUFFIX ".dll"
    )

    if(MSVC)
        target_compile_options(arcdps_legacy_loader_harness PRIVATE /W4 /permissive-)
        target_compile_options(imgui_1927_full              PRIVATE /W3)
    else()
        target_compile_options(arcdps_legacy_loader_harness PRIVATE
            -Wall -Wextra -Wno-unknown-pragmas)
    endif()
endif()
