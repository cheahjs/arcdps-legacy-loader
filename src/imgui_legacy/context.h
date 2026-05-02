#pragma once

#include <windows.h>
#include <cstdint>
#include <mutex>

/* Owns a private imgui 1.80 ImGuiContext plus its dx11/win32 backends.
 * All legacy addons render through this context. */
namespace ImguiLegacy {
    /* Build context + dx11 backend from arcdps's swapchain pointer.
     * id3dptr is IDXGISwapChain* when d3dversion==11. Returns the
     * ImGuiContext* (opaque to callers — pass through to legacy mod_init). */
    void* Init(void* id3dptr, uint32_t d3dversion);

    /* Persist window positions and sever any cross-module pointers from the
     * context into legacy addon DLLs (SettingsHandlers' WriteAllFn, Hooks'
     * Callback). MUST be called before addons are FreeLibrary'd — otherwise
     * ImGui::Shutdown's ini save / shutdown hooks will dereference unmapped
     * code pages and crash. Safe to call before Shutdown; Shutdown is a
     * no-op on these tables after this runs. */
    void  SaveAndDetachFromAddons();
    void  Shutdown();

    /* Pump NewFrame for the 1.80 context; call before dispatching imgui_cbs.
     * EndFrame+Render+draw submission happens in EndFrameAndRender. */
    void  NewFrame();
    void  EndFrameAndRender();

    /* Feeds the 1.80 backend's IO state from a WndProc message. Does not
     * consume the message — AddonManager decides whether to forward it to
     * legacy addons / the game based on io.WantCaptureMouse etc. */
    void WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    void* Context();  /* ImGuiContext* — for handing to legacy addons */

    /* Serializes all access to the shared 1.80 context. Legacy addon init can
     * run on a background thread, but ImGui itself is not thread-safe. */
    std::recursive_mutex& Mutex();

    /* Re-read arcdps's ImGuiStyle and apply it to our 1.80 context, or (if
     * `follow` is false) reset our context to stock 1.80 defaults.
     * Returns true if an arcdps capture was applied. */
    bool  RefreshStyle(bool follow);
}
