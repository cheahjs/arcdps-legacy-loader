#include "log.h"
#include "proxy/arcdps_proxy.h"

#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>

extern HMODULE g_self_module;

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

    /* Always-on file log next to the loader dll, so we get diagnostics even
     * when e3 can't be resolved. */
    void WriteFile(const char* msg) {
        static char path[MAX_PATH] = {0};
        if (!path[0]) {
            GetModuleFileNameA(g_self_module, path, sizeof(path));
            char* slash = strrchr(path, '\\');
            if (slash) { strcpy(slash + 1, "arcdps_legacy_loader.log"); }
            else { strcpy(path, "arcdps_legacy_loader.log"); }
        }
        FILE* f = fopen(path, "a");
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
