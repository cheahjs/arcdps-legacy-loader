/* Reads the top-level window list out of arcdps's 1.92.7 ImGuiContext.
 * Same headers-only strategy as arc_style_reader.cpp: include the 1.92
 * internal header for ImGuiContext/ImGuiWindow layout, cast the opaque
 * ctx pointer, and read fields by member name. Nothing links against
 * 1.92 imgui.
 *
 * Must NOT include the loader's 1.80 "imgui.h". */

#include "arc_style_snapshot.h"
#include "arc_windows_snapshot.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <cstddef>
#include <cstring>

namespace {
    void CopyName(char dst[ARC_WINDOW_NAME_MAX], const char* src) {
        if (!src) { dst[0] = '\0'; return; }
        size_t n = std::strlen(src);
        if (n >= ARC_WINDOW_NAME_MAX) n = ARC_WINDOW_NAME_MAX - 1;
        std::memcpy(dst, src, n);
        dst[n] = '\0';
    }

    size_t ExpectedWindowsOffset() {
        return __builtin_offsetof(ImGuiContext, Windows);
    }
}

extern "C" int ArcWindowsReader_Capture(void* arc_imgui_ctx, ArcWindowList* out) {
    if (!out) return 0;
    out->count = 0;
    if (!arc_imgui_ctx) return 0;

    /* Must not read Windows until the style reader has located the real
     * layout — we use its delta to correct our own offset. If layout is
     * still unknown, bail silently; the style reader's retry/give-up
     * loop owns the one-shot diagnostic log. */
    if (!ArcStyleReader_LayoutKnown()) return 0;

    const unsigned char* base = static_cast<const unsigned char*>(arc_imgui_ctx);
    const auto delta = static_cast<ptrdiff_t>(ArcStyleReader_LayoutDelta());
    const auto& windows = *reinterpret_cast<const ImVector<ImGuiWindow*>*>(
        base + ExpectedWindowsOffset() + delta);

    /* Post-correction plausibility check — a bad Windows vector crashes
     * as soon as we dereference an element. */
    if (windows.Size < 0 || windows.Size > 4096) return 0;
    if (windows.Size > 0 && windows.Data == nullptr) return 0;

    int n = 0;
    for (int i = 0; i < windows.Size && n < ARC_MAX_WINDOWS; ++i) {
        const ImGuiWindow* w = windows[i];
        if (!w || !w->WasActive || w->Hidden) continue;
        if (w->Flags & ImGuiWindowFlags_ChildWindow) continue;
        /* Skip imgui-internal debug windows. */
        if (!w->Name || (w->Name[0] == '#' && w->Name[1] == '#')) continue;

        CopyName(out->items[n].name, w->Name);
        out->items[n].pos[0]  = w->Pos.x;
        out->items[n].pos[1]  = w->Pos.y;
        out->items[n].size[0] = w->Size.x;
        out->items[n].size[1] = w->Size.y;
        ++n;
    }
    out->count = n;
    return 1;
}
