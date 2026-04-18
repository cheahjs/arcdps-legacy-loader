#pragma once

#include <windows.h>
#include <stdint.h>

/* State captured from arcdps's call to our mod_init.
 * Legacy addons receive their own versions of these fields (with imguictx
 * swapped for our 1.80 context) during their own mod_init. */
struct ArcdpsHandoff {
    const char* arcversion   = nullptr;
    void*       arc_imguictx = nullptr;  /* 1.92.7 ctx — stashed, we don't use it */
    void*       id3dptr      = nullptr;  /* IDXGISwapChain* (d3dversion==11) */
    HMODULE     arcdll       = nullptr;  /* arcdps HMODULE — e0..e10 live here */
    void*       mallocfn     = nullptr;
    void*       freefn       = nullptr;
    uint32_t    d3dversion   = 0;
};

namespace ArcdpsProxy {
    void Capture(const char* arcversion, void* imguictx, void* id3dptr,
                 HMODULE arcdll, void* mallocfn, void* freefn, uint32_t d3dversion);
    const ArcdpsHandoff& Get();
}
