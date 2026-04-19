#include "log.h"
#include "proxy/arcdps_proxy.h"

#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <string>

namespace {
    using e3_fn_t = uintptr_t(*)(char*);
    e3_fn_t g_e3 = nullptr;
    bool    g_resolved = false;

    void Resolve() {
        if (g_resolved) return;
        HMODULE arc = ArcdpsProxy::Get().arcdll;
        if (arc) g_e3 = reinterpret_cast<e3_fn_t>(GetProcAddress(arc, "e3"));
        g_resolved = true;
    }

    /* Rooted on the game exe so the log lives at a predictable location
     * (<gw2-root>/addons/arcdps/) regardless of where the loader dll sits.
     * Kept as a wide string so _wfopen sees the real Windows path under
     * Wine — narrow fopen + fs::path::string() was silently failing when
     * the path contained an ANSI-unfriendly component, leaving us with
     * no local log even though e3 forwarding still worked. */
    const std::wstring& LogPath() {
        static std::wstring path = [] {
            wchar_t buf[MAX_PATH];
            DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
            namespace fs = std::filesystem;
            fs::path dir = (n > 0 && n < MAX_PATH) ? fs::path(buf).parent_path() : fs::path();
            return (dir / L"addons" / L"arcdps" / L"arcdps_legacy_loader.log").wstring();
        }();
        return path;
    }

    /* Always-on file log next to arcdps.dll, so we get diagnostics even when
     * e3 can't be resolved. */
    void WriteFile(const char* msg) {
        /* Truncate on the first write of each process run so the log reflects
         * only the current session. */
        static bool truncated = false;
        const wchar_t* mode = truncated ? L"a" : L"w";
        truncated = true;
        FILE* f = _wfopen(LogPath().c_str(), mode);
        if (!f) return;
        SYSTEMTIME st; GetLocalTime(&st);
        fprintf(f, "%04d-%02d-%02d %02d:%02d:%02d.%03d %s\n",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                msg);
        fclose(f);
    }
}

namespace Log {

void Msg(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (n < 0) return;

    WriteFile(buf);

    Resolve();
    if (g_e3) g_e3(buf);
}

}
