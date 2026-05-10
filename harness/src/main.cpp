/* arcdps_legacy_loader_harness.dll
 *
 * Debug-only sibling addon that drives the legacy loader's init/release cycle
 * via arcdps's addextension2 / removeextension2 exports, to reproduce
 * teardown/re-init crashes on demand without restarting the game.
 *
 * Lives in its own DLL (not pinned into the loader) so the loader is free
 * to be FreeLibrary'd by arcdps each cycle, exactly the way a real reload
 * would happen.
 *
 * Skip-addon-load on the loader side is signaled via the
 * ARCDPS_LL_HARNESS_SKIP_ADDONS=1 environment variable, which the loader's
 * mod_init reads before deciding whether to scan the legacy addons dir.
 *
 * Build with -DARCDPS_LL_HARNESS=ON.
 */

#include <arcdps/arcdps_structs.h>

#include <windows.h>
#include <imgui.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

namespace fs = std::filesystem;

namespace {

constexpr uint32_t HARNESS_SIG        = 0xAA12EDAD;
constexpr uint32_t LEGACY_LOADER_SIG  = 0x7A11EDAD;  /* mirrors src/exports.cpp */
constexpr const char*  LOADER_DLL_A   = "arcdps_legacy_loader.dll";
constexpr const wchar_t* LOADER_DLL_W = L"arcdps_legacy_loader.dll";
constexpr const wchar_t* SKIP_ENV     = L"ARCDPS_LL_HARNESS_SKIP_ADDONS";

/* arcdps API typedefs. addextension2 returns 0 on success, otherwise an
 * error code 1..7 (see README at https://www.deltaconnected.com/arcdps/api/).
 * removeextension2 returns 0 if the sig wasn't loaded, or the previous
 * HINSTANCE otherwise. */
using e3_fn_t   = void (*)(char*);
using add_ext_t = uint32_t (*)(HINSTANCE);
using rm_ext_t  = HINSTANCE (*)(uint32_t);

HMODULE        g_arcdll           = nullptr;
e3_fn_t        g_e3               = nullptr;
add_ext_t      g_addextension2    = nullptr;
rm_ext_t       g_removeextension2 = nullptr;
void*          g_arc_imguictx     = nullptr;
arcdps_exports g_exports{};

/* Harness state. All cross-thread reads/writes go through atomics; the
 * loader-path string is the one piece that needs a mutex because we resolve
 * it lazily and pass through DoOneCycle. */
std::atomic_bool g_continuous{false};
std::atomic_bool g_skip_addons{true};
std::atomic_int  g_reload_request{0};
std::atomic_uint g_cycle_count{0};
std::atomic_int  g_last_add_error{0};   /* 0=ok, >0 = arcdps code, <0 = local */
std::atomic_int  g_min_interval_ms{100};
std::atomic_bool g_worker_quit{false};
std::atomic_bool g_path_resolved{false};

std::thread  g_worker;
std::mutex   g_path_mutex;
std::wstring g_loader_path;

void Log(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (g_e3) {
        char prefixed[sizeof(buf) + 16];
        snprintf(prefixed, sizeof(prefixed), "ll_harness: %s", buf);
        g_e3(prefixed);
    }
}

bool ResolveLoaderPath() {
    if (g_path_resolved.load()) return true;

    /* Prefer the already-loaded module so we get the exact path arcdps used.
     * If the loader isn't currently loaded (user removed it manually, or
     * first cycle hasn't run yet from a fresh load), fall back to the game
     * exe directory where arcdps looks for its native addons. */
    HMODULE m = GetModuleHandleA(LOADER_DLL_A);
    if (m) {
        wchar_t buf[MAX_PATH];
        DWORD n = GetModuleFileNameW(m, buf, MAX_PATH);
        if (n > 0 && n < MAX_PATH) {
            std::lock_guard<std::mutex> lk(g_path_mutex);
            g_loader_path = buf;
            g_path_resolved.store(true);
            return true;
        }
    }

    wchar_t exe[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        fs::path p = fs::path(exe).parent_path() / LOADER_DLL_W;
        std::error_code ec;
        if (fs::exists(p, ec)) {
            std::lock_guard<std::mutex> lk(g_path_mutex);
            g_loader_path = p.wstring();
            g_path_resolved.store(true);
            return true;
        }
    }
    return false;
}

void DoOneCycle() {
    /* Set the env var before either step so the loader's next mod_init
     * picks it up. Clear if the user toggled it off. */
    if (g_skip_addons.load())
        SetEnvironmentVariableW(SKIP_ENV, L"1");
    else
        SetEnvironmentVariableW(SKIP_ENV, nullptr);

    if (g_removeextension2) {
        HINSTANCE prev = g_removeextension2(LEGACY_LOADER_SIG);
        (void)prev;  /* 0 just means it wasn't loaded — fine, we proceed to add */
    }

    if (!ResolveLoaderPath()) {
        g_last_add_error.store(-1);  /* path not found */
        return;
    }

    std::wstring path;
    {
        std::lock_guard<std::mutex> lk(g_path_mutex);
        path = g_loader_path;
    }
    if (!g_addextension2) {
        g_last_add_error.store(-2);  /* arcdps addextension2 not resolved */
        return;
    }

    /* addextension2 contract: arcdps LoadLibrary's the HINSTANCE to bump its
     * own refcount, then calls get_init_addr + mod_init. After we hand off
     * we FreeLibrary our local handle so the only outstanding ref is
     * arcdps's, leaving the steady state identical to a fresh-from-disk
     * load. */
    HMODULE local = LoadLibraryW(path.c_str());
    if (!local) {
        g_last_add_error.store(-3);
        return;
    }
    uint32_t err = g_addextension2(local);
    FreeLibrary(local);
    g_last_add_error.store(static_cast<int>(err));
    if (err == 0) g_cycle_count.fetch_add(1);
    else Log("addextension2 returned %u", err);
}

void WorkerLoop() {
    /* Resolve the loader's on-disk path before the first reload — at this
     * point the loader is (usually) already loaded by arcdps. If we wait
     * until after the first removeextension2, GetModuleHandle would return
     * NULL and we'd have to rely on the exe-dir fallback. */
    ResolveLoaderPath();

    while (!g_worker_quit.load()) {
        const bool requested  = g_reload_request.exchange(0) != 0;
        const bool continuous = g_continuous.load();
        if (requested || continuous) {
            DoOneCycle();
            int ms = g_min_interval_ms.load();
            if (ms < 0) ms = 0;
            /* Break the sleep early on quit so we don't block shutdown. */
            const int slice = 25;
            while (ms > 0 && !g_worker_quit.load()) {
                int chunk = ms < slice ? ms : slice;
                Sleep(static_cast<DWORD>(chunk));
                ms -= chunk;
            }
        } else {
            Sleep(25);
        }
    }
}

void DrawUi() {
    ImGui::SetNextWindowSize(ImVec2(380, 0), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("legacy_loader harness")) { ImGui::End(); return; }

    {
        bool v = g_skip_addons.load();
        if (ImGui::Checkbox("Skip addon load on reload", &v))
            g_skip_addons.store(v);
        ImGui::TextDisabled("Sets %ls=1 before each reload.", SKIP_ENV);
    }

    ImGui::Separator();

    if (ImGui::Button("Reload now")) g_reload_request.store(1);
    ImGui::SameLine();
    {
        bool v = g_continuous.load();
        if (ImGui::Checkbox("Continuous", &v)) g_continuous.store(v);
    }

    {
        int v = g_min_interval_ms.load();
        if (ImGui::SliderInt("Min interval (ms)", &v, 0, 2000))
            g_min_interval_ms.store(v);
    }

    ImGui::Separator();

    {
        std::lock_guard<std::mutex> lk(g_path_mutex);
        if (g_loader_path.empty()) {
            ImGui::TextUnformatted("Loader path: <not resolved>");
        } else {
            std::string utf8 = fs::path(g_loader_path).string();
            ImGui::Text("Loader path: %s", utf8.c_str());
        }
    }
    ImGui::Text("Cycles: %u", g_cycle_count.load());
    const int err = g_last_add_error.load();
    if (err == 0) {
        ImGui::TextUnformatted("Last result: ok");
    } else if (err > 0) {
        const char* what = "?";
        switch (err) {
            case 1: what = "extension-specific error"; break;
            case 2: what = "imgui version mismatch"; break;
            case 3: what = "obsolete arcdps module"; break;
            case 4: what = "extension with sig already exists"; break;
            case 5: what = "extension did not provide function table"; break;
            case 6: what = "extension does not have an init function"; break;
            case 7: what = "loadlibrary error"; break;
        }
        ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1),
                           "Last result: addextension2 error %d (%s)", err, what);
    } else {
        const char* what = "?";
        switch (err) {
            case -1: what = "loader dll path not found"; break;
            case -2: what = "arcdps addextension2 export not resolved"; break;
            case -3: what = "LoadLibraryW failed"; break;
        }
        ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1),
                           "Last result: harness error %d (%s)", err, what);
    }

    ImGui::End();
}

void cb_imgui(uint32_t /*not_charsel_or_loading*/, uint32_t /*hide_if_combat_or_ooc*/) {
    /* Drawing happens on arcdps's own 1.92.7 ImGui context, which our static
     * imgui copy was pointed at in mod_init. SetCurrentContext is idempotent
     * and cheap, but we re-set it here in case some other addon switched
     * context on our copy of imgui (each statically-linked copy has its own
     * GImGui). */
    if (g_arc_imguictx)
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(g_arc_imguictx));
    DrawUi();
}

arcdps_exports* mod_init() {
    g_exports.size       = sizeof(arcdps_exports);
    g_exports.sig        = HARNESS_SIG;
    g_exports.imguivers  = 19270;  /* arcdps 1.92.7 */
    g_exports.out_name   = "legacy_loader_harness";
    g_exports.out_build  = "0.1";
    g_exports.imgui      = reinterpret_cast<void*>(&cb_imgui);

    g_worker_quit.store(false);
    g_worker = std::thread(WorkerLoop);
    Log("init (sig=0x%08X), targeting loader sig 0x%08X", HARNESS_SIG, LEGACY_LOADER_SIG);
    return &g_exports;
}

void mod_release() {
    g_worker_quit.store(true);
    if (g_worker.joinable()) g_worker.join();
    /* Clear the env var on shutdown so the next fresh game start (which the
     * same process could in theory survive into via launcher reuse) doesn't
     * inherit a stale skip flag. */
    SetEnvironmentVariableW(SKIP_ENV, nullptr);
    Log("release");
}

}  /* namespace */

extern "C" __declspec(dllexport) void* get_init_addr(
    char* /*arcversion*/, void* imguictx, void* /*id3dptr*/, HMODULE arcdll,
    void* mallocfn, void* freefn, uint32_t /*imguiversion*/) {
    g_arcdll       = arcdll;
    g_arc_imguictx = imguictx;
    if (arcdll) {
        g_e3               = reinterpret_cast<e3_fn_t>(GetProcAddress(arcdll, "e3"));
        g_addextension2    = reinterpret_cast<add_ext_t>(GetProcAddress(arcdll, "addextension2"));
        g_removeextension2 = reinterpret_cast<rm_ext_t>(GetProcAddress(arcdll, "removeextension2"));
    }
    /* Route our static imgui's allocations through arcdps's allocator so
     * any Begin/Text/... we issue on arcdps's context allocates from the
     * same heap arcdps's own imgui copy uses. Same reasoning as the
     * loader's main DLL (see src/imgui_legacy/context.cpp:Init). */
    if (mallocfn && freefn) {
        ImGui::SetAllocatorFunctions(
            reinterpret_cast<void*(*)(size_t, void*)>(mallocfn),
            reinterpret_cast<void(*)(void*, void*)>(freefn));
    }
    if (imguictx)
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(imguictx));
    return reinterpret_cast<void*>(&mod_init);
}

extern "C" __declspec(dllexport) void* get_release_addr() {
    return reinterpret_cast<void*>(&mod_release);
}

BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(h);
    return TRUE;
}
