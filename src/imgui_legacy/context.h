#pragma once

#include <windows.h>
#include <cstdint>

/* Owns a private imgui 1.80 ImGuiContext plus its dx11/win32 backends.
 * All legacy addons render through this context. */
namespace ImguiLegacy {
    /* Build context + dx11 backend from arcdps's swapchain pointer.
     * id3dptr is IDXGISwapChain* when d3dversion==11. Returns the
     * ImGuiContext* (opaque to callers — pass through to legacy mod_init). */
    void* Init(void* id3dptr, uint32_t d3dversion);
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
}
