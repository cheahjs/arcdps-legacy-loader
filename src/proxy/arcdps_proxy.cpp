#include "arcdps_proxy.h"

namespace {
    ArcdpsHandoff g_handoff;
}

namespace ArcdpsProxy {

void Capture(const char* arcversion, void* imguictx, void* id3dptr,
             HMODULE arcdll, void* mallocfn, void* freefn, uint32_t d3dversion) {
    g_handoff.arcversion   = arcversion;
    g_handoff.arc_imguictx = imguictx;
    g_handoff.id3dptr      = id3dptr;
    g_handoff.arcdll       = arcdll;
    g_handoff.mallocfn     = mallocfn;
    g_handoff.freefn       = freefn;
    g_handoff.d3dversion   = d3dversion;
}

const ArcdpsHandoff& Get() { return g_handoff; }

}
