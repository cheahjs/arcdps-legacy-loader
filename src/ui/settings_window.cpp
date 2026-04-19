#include "settings_window.h"
#include "addon/addon_manager.h"
#include "addon/legacy_addon.h"
#include "config/config.h"
#include "imgui_legacy/context.h"

#include <imgui.h>

namespace {
    bool g_open             = false;
    bool g_awaiting_rebind  = false;

    bool IsModifierVk(int vk) {
        return vk == VK_SHIFT   || vk == VK_LSHIFT   || vk == VK_RSHIFT
            || vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL
            || vk == VK_MENU    || vk == VK_LMENU    || vk == VK_RMENU;
    }
}

namespace SettingsWindow {

void Toggle() { g_open = !g_open; }
bool IsOpen() { return g_open; }

void Draw() {
    if (!g_open) return;
    /* Without this, ImGui calls would dereference a null GImGui. */
    if (!ImguiLegacy::Context()) return;

    ImGui::SetNextWindowSize(ImVec2(520, 420), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Legacy Addon Settings", &g_open)) { ImGui::End(); return; }

    if (ImGui::BeginTabBar("legacy_loader")) {
        if (ImGui::BeginTabItem("Loader")) {
            auto& hk = Config::ToggleKey();
            char label[64];
            Config::FormatHotkey(hk, label, sizeof(label));
            ImGui::Text("Toggle hotkey: %s", label);

            if (g_awaiting_rebind) {
                ImGui::TextUnformatted("Press a key... (Esc to cancel)");
            } else if (ImGui::Button("Rebind")) {
                g_awaiting_rebind = true;
            }

            bool dirty = false;
            dirty |= ImGui::Checkbox("Ctrl",  &hk.ctrl);
            ImGui::SameLine();
            dirty |= ImGui::Checkbox("Shift", &hk.shift);
            ImGui::SameLine();
            dirty |= ImGui::Checkbox("Alt",   &hk.alt);
            if (dirty) Config::Save();

            ImGui::Separator();
            bool& follow = Config::StyleFollowsArcdps();
            if (ImGui::Checkbox("Match arcdps style", &follow)) {
                Config::Save();
                ImguiLegacy::RefreshStyle(follow);
            }
            ImGui::SameLine();
            if (ImGui::Button("Re-capture")) ImguiLegacy::RefreshStyle(follow);
            ImGui::TextDisabled("Copies arcdps's ImGui theme onto the legacy context.");

            bool& mirror = Config::MirrorArcdpsWindows();
            if (ImGui::Checkbox("Mirror arcdps windows", &mirror)) Config::Save();
            ImGui::TextDisabled("Lets legacy addons anchor their UI against arcdps windows.");

            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Windows")) {
            /* After arcdps's own window-name iteration, it calls
             * options_windows(nullptr) to let each addon render its own
             * checkboxes. We have no name list to iterate ourselves, so we
             * just run the nullptr pass. */
            for (size_t i = 0; i < AddonManager::Count(); ++i) {
                auto& a = AddonManager::At(i);
                if (!a.Loaded()) continue;
                ImGui::PushID(static_cast<int>(i));
                a.CallOptionsWindows(nullptr);
                ImGui::PopID();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Addons")) {
            if (ImGui::BeginTabBar("legacy_addons")) {
                for (size_t i = 0; i < AddonManager::Count(); ++i) {
                    auto& a = AddonManager::At(i);
                    if (!a.Loaded()) continue;
                    if (ImGui::BeginTabItem(a.Name())) {
                        a.CallOptionsTab();
                        ImGui::EndTabItem();
                    }
                }
                ImGui::EndTabBar();
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

bool HandleKey(HWND, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg != WM_KEYDOWN && msg != WM_SYSKEYDOWN) return false;
    /* lparam bit 30 is the previous key state — set means the key was already
     * down, i.e. this is an auto-repeat. Skip so holding the hotkey doesn't
     * toggle every frame. */
    if (lp & (1 << 30)) return false;

    /* Rebind capture: first non-modifier keydown becomes the new toggle. */
    if (g_awaiting_rebind) {
        if (wp == VK_ESCAPE) { g_awaiting_rebind = false; return true; }
        if (IsModifierVk(static_cast<int>(wp))) return false;
        auto& hk = Config::ToggleKey();
        hk.vk    = static_cast<int>(wp);
        hk.ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        hk.shift = (GetKeyState(VK_SHIFT)   & 0x8000) != 0;
        hk.alt   = (GetKeyState(VK_MENU)    & 0x8000) != 0;
        Config::Save();
        g_awaiting_rebind = false;
        return true;
    }

    const auto& hk = Config::ToggleKey();
    if (static_cast<int>(wp) != hk.vk) return false;
    if (hk.shift && !(GetKeyState(VK_SHIFT)   & 0x8000)) return false;
    if (hk.ctrl  && !(GetKeyState(VK_CONTROL) & 0x8000)) return false;
    if (hk.alt   && !(GetKeyState(VK_MENU)    & 0x8000)) return false;
    Toggle();
    return true;
}

}
