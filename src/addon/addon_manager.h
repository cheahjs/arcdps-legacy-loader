#pragma once

#include <arcdps/arcdps_structs.h>
#include <windows.h>

#include <functional>

class LegacyAddon;

namespace AddonManager {
    /* Iterate addons under the shared mutex — safe to call while the
     * background loader is still populating g_addons. */
    void ForEach(const std::function<void(size_t, LegacyAddon&)>& fn);
    bool Empty();

    /* Snapshot accessors. Only safe to call from the render thread, which
     * is also where the background loader's appends become visible under
     * the shared mutex — callers should not hold indices across frames. */
    size_t Count();
    LegacyAddon& At(size_t i);

    /* Kick off a background thread that scans the legacy addon directory
     * and loads each dll against our 1.80 imgui context. Returns
     * immediately so arcdps's own init isn't blocked behind third-party
     * addon load times. Call once, after ImguiLegacy::Init. */
    void LoadAllAsync(void* legacy_imguictx);
    /* Joins the load thread before tearing down any loaded addons. */
    void UnloadAll();

    /* Fanout of arcdps callbacks. The loader's own mod_init wires its
     * exports table entries to these. */
    void DispatchImgui(uint32_t not_charsel_or_loading, uint32_t hide_if_combat_or_ooc);
    void DispatchCombat(cbtevent*, ag*, ag*, const char*, uint64_t, uint64_t);
    void DispatchCombatLocal(cbtevent*, ag*, ag*, const char*, uint64_t, uint64_t);
    uint32_t DispatchWndFilter(HWND, UINT, WPARAM, LPARAM);
    uint32_t DispatchWndNoFilter(HWND, UINT, WPARAM, LPARAM);
}
