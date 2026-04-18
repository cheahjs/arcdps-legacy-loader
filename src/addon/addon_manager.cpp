#include "addon_manager.h"
#include "legacy_addon.h"
#include "imgui_legacy/context.h"
#include "logging/log.h"
#include "ui/settings_window.h"

#include <imgui.h>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

extern HMODULE g_self_module;

namespace {
    std::vector<LegacyAddon> g_addons;

    /* <gw2-root>/addons/arcdps/legacy — resolve against the game exe's dir
     * (GetModuleHandle(NULL)) so the loader dll itself can live anywhere. */
    fs::path LegacyDir() {
        wchar_t buf[MAX_PATH];
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        return fs::path(buf).parent_path() / L"addons" / L"arcdps" / L"legacy";
    }
}

namespace AddonManager {

size_t Count() { return g_addons.size(); }
LegacyAddon& At(size_t i) { return g_addons[i]; }

void LoadAll(void* legacy_imguictx) {
    auto dir = LegacyDir();
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        Log::Msg("AddonManager: legacy dir missing: %s", dir.string().c_str());
        return;
    }

    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != L".dll") continue;

        LegacyAddon addon;
        const auto name = entry.path().filename().string();
        if (addon.Load(entry.path().wstring(), legacy_imguictx))
            g_addons.push_back(std::move(addon));
        else
            Log::Msg("AddonManager: failed to load %s", name.c_str());
    }
}

void UnloadAll() {
    for (auto& a : g_addons) a.Unload();
    g_addons.clear();
}

void DispatchImgui(uint32_t flag) {
    ImguiLegacy::NewFrame();
    for (auto& a : g_addons) a.CallImgui(flag);
    SettingsWindow::Draw();
    ImguiLegacy::EndFrameAndRender();
}

void DispatchCombat(cbtevent* ev, ag* src, ag* dst, const char* sk, uint64_t id, uint64_t rev) {
    for (auto& a : g_addons) a.CallCombat(ev, src, dst, sk, id, rev);
}

void DispatchCombatLocal(cbtevent* ev, ag* src, ag* dst, const char* sk, uint64_t id, uint64_t rev) {
    for (auto& a : g_addons) a.CallCombatLocal(ev, src, dst, sk, id, rev);
}

uint32_t DispatchWndFilter(HWND h, UINT m, WPARAM w, LPARAM l) {
    uint32_t out = m;
    for (auto& a : g_addons) {
        out = a.CallWndFilter(h, out, w, l);
        if (out == 0) return 0;
    }
    return out;
}

uint32_t DispatchWndNoFilter(HWND h, UINT m, WPARAM w, LPARAM l) {
    /* Route all messages through our 1.80 backend so input reaches legacy
     * windows regardless of whether arcdps's own imgui claims capture. */
    ImguiLegacy::WndProc(h, m, w, l);

    if (SettingsWindow::HandleKey(h, m, w, l)) return 0;

    /* Swallow only the messages imgui explicitly wants. MOUSEMOVE and
     * BUTTONUP always pass to the game so the camera keeps tracking and
     * mouse buttons never get stuck. imgui keeps WantCaptureMouse true for
     * the duration of a drag via its active id, so drags work without any
     * extra capture bookkeeping. */
    if (ImguiLegacy::Context()) {
        ImGui::SetCurrentContext(static_cast<ImGuiContext*>(ImguiLegacy::Context()));
        const auto& io = ImGui::GetIO();
        switch (m) {
        case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:
        case WM_MOUSEWHEEL:  case WM_MOUSEHWHEEL:
            if (io.WantCaptureMouse) return 0;
            break;
        case WM_KEYDOWN: case WM_SYSKEYDOWN: case WM_CHAR:
            if (io.WantTextInput) return 0;
            break;
        default: break;
        }
    }

    uint32_t out = m;
    for (auto& a : g_addons) {
        out = a.CallWndNoFilter(h, out, w, l);
        if (out == 0) return 0;
    }
    return out;
}

}
