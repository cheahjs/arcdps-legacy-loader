#include <arcdps/arcdps_structs.h>

#include "addon/addon_manager.h"
#include "config/config.h"
#include "imgui_legacy/context.h"
#include "logging/log.h"
#include "proxy/arcdps_proxy.h"
#include "version.h"

#include <mutex>

/* arcdps 1.92.7 checks imguivers against this value. We statically link
 * imgui 1.80 for the legacy-addon context but report 19270 to arcdps.
 * (Verified empirically: 19270 loads, 19207/18000 both get rejected.) */
static constexpr uint32_t IMGUI_VERSION_NUM_1_92_7 = 19270;

namespace {
    arcdps_exports g_exports{};

    void cb_imgui(uint32_t not_charsel_or_loading, uint32_t hide_if_combat_or_ooc) {
        AddonManager::DispatchImgui(not_charsel_or_loading, hide_if_combat_or_ooc);
    }
    /* options_end/options_windows intentionally left null on our export table:
     * arcdps 1.92.7 would call them in its own imgui context, which we can't
     * hand to legacy addons. Legacy options_tab/options_windows run inside our
     * own SettingsWindow on the 1.80 context. */
    void cb_combat(cbtevent* ev, ag* s, ag* d, const char* sk, uint64_t i, uint64_t r) {
        AddonManager::DispatchCombat(ev, s, d, sk, i, r);
    }
    void cb_combat_local(cbtevent* ev, ag* s, ag* d, const char* sk, uint64_t i, uint64_t r) {
        AddonManager::DispatchCombatLocal(ev, s, d, sk, i, r);
    }
    uint32_t cb_wnd_filter(HWND h, UINT m, WPARAM w, LPARAM l) {
        return AddonManager::DispatchWndFilter(h, m, w, l);
    }
    uint32_t cb_wnd_nofilter(HWND h, UINT m, WPARAM w, LPARAM l) {
        return AddonManager::DispatchWndNoFilter(h, m, w, l);
    }

    arcdps_exports* mod_init() {
        Config::Load();
        const auto& h = ArcdpsProxy::Get();

        /* Populate exports BEFORE anything that can hang or crash. arcdps must
         * receive a valid exports table even if legacy addon init misbehaves. */
        g_exports.size       = sizeof(arcdps_exports);
        g_exports.sig        = 0x7A11EDAD;
        g_exports.imguivers  = IMGUI_VERSION_NUM_1_92_7;
        g_exports.out_name   = "legacy_loader";
        g_exports.out_build  = ARCDPS_LL_VERSION_STRING;
        Log::Msg("legacy_loader %s (built %s %s)",
                 ARCDPS_LL_VERSION_STRING, __DATE__, __TIME__);
        g_exports.imgui           = reinterpret_cast<void*>(&cb_imgui);
        g_exports.options_end     = nullptr;
        g_exports.options_windows = nullptr;
        g_exports.combat          = reinterpret_cast<void*>(&cb_combat);
        g_exports.combat_local    = reinterpret_cast<void*>(&cb_combat_local);
        g_exports.wnd_filter      = reinterpret_cast<void*>(&cb_wnd_filter);
        g_exports.wnd_nofilter    = reinterpret_cast<void*>(&cb_wnd_nofilter);

        void* legacy_ctx = ImguiLegacy::Init(h.id3dptr);
        if (!legacy_ctx) return &g_exports;
        AddonManager::LoadAllAsync(legacy_ctx);
        return &g_exports;
    }

    void mod_release() {
        AddonManager::WaitForLoadComplete();
        std::lock_guard<std::recursive_mutex> ctx_lk(ImguiLegacy::Mutex());
        /* Persist ini + sever cross-module pointers (settings handlers, hooks)
         * BEFORE FreeLibrary'ing legacy addons. If we unloaded first,
         * ImGui::Shutdown's save path would call WriteAllFn on a handler the
         * addon registered and crash in its now-unmapped code page. */
        ImguiLegacy::SaveAndDetachFromAddons();
        AddonManager::UnloadAll();
        ImguiLegacy::Shutdown();
    }
}

/* arcdps invokes get_init_addr with the full handoff; we capture it and
 * return a pointer to a parameterless mod_init per the arcdps API. */
extern "C" __declspec(dllexport) void* get_init_addr(
    char* arcversion, void* imguictx, void* id3dptr, HMODULE arcdll,
    void* mallocfn, void* freefn, uint32_t imguiversion) {
    ArcdpsProxy::Capture(arcversion, imguictx, id3dptr, arcdll, mallocfn, freefn, imguiversion);
    return reinterpret_cast<void*>(&mod_init);
}

extern "C" __declspec(dllexport) void* get_release_addr() {
    return reinterpret_cast<void*>(&mod_release);
}
