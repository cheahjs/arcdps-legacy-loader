#include "config.h"
#include "logging/log.h"

#include <cstdio>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

namespace {
    Config::Hotkey g_toggle;
    bool           g_style_follows_arcdps = true;
    bool           g_mirror_arcdps_windows = true;

    fs::path ConfigPath() {
        wchar_t buf[MAX_PATH];
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        return fs::path(buf).parent_path() / L"addons" / L"arcdps" / L"legacy" / L"loader.ini";
    }
}

namespace Config {

void Load() {
    FILE* f = _wfopen(ConfigPath().wstring().c_str(), L"r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        int v;
        if (sscanf(line, "toggle_vk=%d", &v)    == 1) g_toggle.vk    = v;
        if (sscanf(line, "toggle_shift=%d", &v) == 1) g_toggle.shift = v != 0;
        if (sscanf(line, "toggle_ctrl=%d", &v)  == 1) g_toggle.ctrl  = v != 0;
        if (sscanf(line, "toggle_alt=%d", &v)   == 1) g_toggle.alt   = v != 0;
        if (sscanf(line, "style_follows_arcdps=%d", &v) == 1) g_style_follows_arcdps = v != 0;
        if (sscanf(line, "mirror_arcdps_windows=%d", &v) == 1) g_mirror_arcdps_windows = v != 0;
    }
    fclose(f);
}

void Save() {
    auto path = ConfigPath();
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    FILE* f = _wfopen(path.wstring().c_str(), L"w");
    if (!f) {
        Log::Msg("Config: save failed: %s", path.string().c_str());
        return;
    }
    fprintf(f, "toggle_vk=%d\n",    g_toggle.vk);
    fprintf(f, "toggle_shift=%d\n", g_toggle.shift ? 1 : 0);
    fprintf(f, "toggle_ctrl=%d\n",  g_toggle.ctrl  ? 1 : 0);
    fprintf(f, "toggle_alt=%d\n",   g_toggle.alt   ? 1 : 0);
    fprintf(f, "style_follows_arcdps=%d\n", g_style_follows_arcdps ? 1 : 0);
    fprintf(f, "mirror_arcdps_windows=%d\n", g_mirror_arcdps_windows ? 1 : 0);
    fclose(f);
}

Hotkey& ToggleKey()           { return g_toggle; }
bool&   StyleFollowsArcdps()  { return g_style_follows_arcdps; }
bool&   MirrorArcdpsWindows() { return g_mirror_arcdps_windows; }

void FormatHotkey(const Hotkey& h, char* buf, size_t len) {
    char key[32];
    /* Try to get a human-readable name for the key. */
    UINT sc = MapVirtualKeyA(h.vk, MAPVK_VK_TO_VSC) << 16;
    if (GetKeyNameTextA(sc, key, sizeof(key)) <= 0) {
        if (h.vk >= 32 && h.vk < 127) snprintf(key, sizeof(key), "%c", h.vk);
        else                          snprintf(key, sizeof(key), "VK_%02X", h.vk);
    }
    snprintf(buf, len, "%s%s%s%s",
             h.ctrl  ? "Ctrl+"  : "",
             h.shift ? "Shift+" : "",
             h.alt   ? "Alt+"   : "",
             key);
}

}
