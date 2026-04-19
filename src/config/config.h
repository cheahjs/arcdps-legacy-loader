#pragma once

#include <windows.h>

/* User-configurable loader settings, persisted to a small ini file next to
 * the legacy-addon directory. */
namespace Config {
    struct Hotkey {
        int  vk    = 'L';    /* Win32 VK_* code; ASCII letter/digit VKs coincide with their char */
        bool shift = true;
        bool ctrl  = false;
        bool alt   = true;
    };

    void Load();
    void Save();

    Hotkey& ToggleKey();             /* mutable — edits save on demand */
    void    FormatHotkey(const Hotkey&, char* buf, size_t len);

    /* When true (default), the loader copies arcdps's ImGuiStyle onto the
     * legacy 1.80 context at init so legacy addons visually match arcdps.
     * Users can opt out via `style_follows_arcdps=0` in loader.ini — some
     * legacy addons ship their own styling that would otherwise fight
     * ours, or the user may prefer the stock 1.80 look. */
    bool& StyleFollowsArcdps();

    /* When true (default), each frame the loader mirrors arcdps's
     * top-level windows into the legacy 1.80 context as invisible
     * shadows with matching names, so legacy addons that anchor their
     * own windows against arcdps windows (e.g. Boon Table's relative
     * positioning) can still resolve them via ImGui::FindWindowByID. */
    bool& MirrorArcdpsWindows();
}
