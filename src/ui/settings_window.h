#pragma once

#include <windows.h>

/* Mock settings window drawn in the 1.80 context. arcdps 1.92.7's own Options
 * UI won't call our legacy addons (they're bound to a different imgui
 * context), so we render their options_end/options_windows ourselves. */
namespace SettingsWindow {
    void Toggle();
    bool IsOpen();

    /* Draw the settings window if open. Call inside a NewFrame on the 1.80 ctx. */
    void Draw();

    /* Request a modal popup on the next Draw listing any addons that failed
     * to load or were rejected (imgui version / duplicate sig). Called once
     * by AddonManager::LoadAll when it has at least one failure. */
    void ShowLoadFailurePopup();

    /* WndProc hook — consumes the toggle hotkey. Returns true if swallowed. */
    bool HandleKey(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
}
