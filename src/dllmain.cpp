#include <windows.h>

HMODULE g_self_module = nullptr;

BOOL APIENTRY DllMain(HMODULE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_self_module = h;
        DisableThreadLibraryCalls(h);
    }
    return TRUE;
}
