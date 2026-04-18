#pragma once

#include <arcdps/arcdps_structs.h>
#include <windows.h>

class LegacyAddon;

namespace AddonManager {
    /* Iterate loaded addons (for UI rendering). */
    size_t Count();
    LegacyAddon& At(size_t i);

    /* Scan a directory for legacy addon dlls and load them all against our
     * 1.80 imgui context. Call once, after ImguiLegacy::Init. */
    void LoadAll(void* legacy_imguictx);
    void UnloadAll();

    /* Fanout of arcdps callbacks. The loader's own mod_init wires its
     * exports table entries to these. */
    void DispatchImgui(uint32_t not_charsel_or_loading);
    void DispatchCombat(cbtevent*, ag*, ag*, const char*, uint64_t, uint64_t);
    void DispatchCombatLocal(cbtevent*, ag*, ag*, const char*, uint64_t, uint64_t);
    uint32_t DispatchWndFilter(HWND, UINT, WPARAM, LPARAM);
    uint32_t DispatchWndNoFilter(HWND, UINT, WPARAM, LPARAM);
}
