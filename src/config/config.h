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
}
