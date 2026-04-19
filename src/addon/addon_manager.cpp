#include "addon_manager.h"
#include "legacy_addon.h"
#include "imgui_legacy/context.h"
#include "logging/log.h"
#include "ui/settings_window.h"

#include <imgui.h>
#include <filesystem>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace {
    /* Legacy addons are built against imgui 1.80 and report 18000 in their
     * exports.imguivers. We allow 0 for addons that don't touch imgui and
     * therefore don't bother filling the field. */
    constexpr uint32_t LEGACY_IMGUI_VERSION_NUM = 18000;

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

    /* Map first-loaded sig -> index into g_addons, so we can label the second
     * loader as the duplicate (keeping the first to run). */
    std::unordered_map<uint32_t, size_t> by_sig;

    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != L".dll") continue;

        LegacyAddon addon;
        const bool ok = addon.Load(entry.path().wstring(), legacy_imguictx);
        const auto name = entry.path().filename().string();

        if (ok) {
            const uint32_t vers = addon.ImguiVers();
            if (vers != 0 && vers != LEGACY_IMGUI_VERSION_NUM) {
                Log::Msg("AddonManager: %s rejected: imguivers %u != %u",
                         name.c_str(), vers, LEGACY_IMGUI_VERSION_NUM);
                addon.Reject(LegacyAddon::Status::ImguiVersionMismatch);
            } else if (auto it = by_sig.find(addon.Sig());
                       it != by_sig.end() && addon.Sig() != 0) {
                Log::Msg("AddonManager: %s rejected: duplicate sig %u (also in %s)",
                         name.c_str(), addon.Sig(), g_addons[it->second].Name());
                addon.Reject(LegacyAddon::Status::DuplicateSig);
            } else {
                by_sig[addon.Sig()] = g_addons.size();
            }
        } else {
            Log::Msg("AddonManager: failed to load %s", name.c_str());
        }

        g_addons.push_back(std::move(addon));
    }

    /* If any entry isn't in the Loaded state, prompt the user. */
    for (const auto& a : g_addons) {
        if (a.State() != LegacyAddon::Status::Loaded) {
            SettingsWindow::ShowLoadFailurePopup();
            break;
        }
    }
}

void UnloadAll() {
    g_addons.clear();  /* ~LegacyAddon runs mod_release + FreeLibrary */
}

void DispatchImgui(uint32_t flag) {
    ImguiLegacy::NewFrame();
    for (auto& a : g_addons) if (a.Loaded()) a.CallImgui(flag);
    SettingsWindow::Draw();
    ImguiLegacy::EndFrameAndRender();
}

void DispatchCombat(cbtevent* ev, ag* src, ag* dst, const char* sk, uint64_t id, uint64_t rev) {
    for (auto& a : g_addons) if (a.Loaded()) a.CallCombat(ev, src, dst, sk, id, rev);
}

void DispatchCombatLocal(cbtevent* ev, ag* src, ag* dst, const char* sk, uint64_t id, uint64_t rev) {
    for (auto& a : g_addons) if (a.Loaded()) a.CallCombatLocal(ev, src, dst, sk, id, rev);
}

uint32_t DispatchWndFilter(HWND h, UINT m, WPARAM w, LPARAM l) {
    uint32_t out = m;
    for (auto& a : g_addons) {
        if (!a.Loaded()) continue;
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
        if (!a.Loaded()) continue;
        out = a.CallWndNoFilter(h, out, w, l);
        if (out == 0) return 0;
    }
    return out;
}

}
