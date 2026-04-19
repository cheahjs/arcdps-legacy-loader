/* Reads the top-level window list out of arcdps's 1.92.7 ImGuiContext.
 * Same headers-only strategy as arc_style_reader.cpp: include the 1.92
 * internal header for ImGuiContext/ImGuiWindow layout, cast the opaque
 * ctx pointer, and read fields by member name. Nothing links against
 * 1.92 imgui.
 *
 * Must NOT include the loader's 1.80 "imgui.h". */

#include "arc_windows_snapshot.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <cstring>

namespace {
    void CopyName(char dst[ARC_WINDOW_NAME_MAX], const char* src) {
        if (!src) { dst[0] = '\0'; return; }
        size_t n = std::strlen(src);
        if (n >= ARC_WINDOW_NAME_MAX) n = ARC_WINDOW_NAME_MAX - 1;
        std::memcpy(dst, src, n);
        dst[n] = '\0';
    }

    /* Same plausibility trick as the style reader — bail if the ctx's
     * Style field holds clearly-impossible values, which indicates our
     * assumed layout doesn't match arcdps's build. */
    bool LooksSane(const ImGuiContext* ctx) {
        const ImGuiStyle& s = ctx->Style;
        if (!(s.Alpha >= 0.0f && s.Alpha <= 1.0f)) return false;
        if (!(s.WindowRounding >= 0.0f && s.WindowRounding <= 64.0f)) return false;
        if (!(s.FramePadding.x >= 0.0f && s.FramePadding.x <= 256.0f)) return false;
        /* Windows vector plausibility: Size non-negative, under a huge cap,
         * Data non-null when non-empty. */
        if (ctx->Windows.Size < 0 || ctx->Windows.Size > 4096) return false;
        if (ctx->Windows.Size > 0 && ctx->Windows.Data == nullptr) return false;
        return true;
    }
}

extern "C" int ArcWindowsReader_Capture(void* arc_imgui_ctx, ArcWindowList* out) {
    if (!out) return 0;
    out->count = 0;
    if (!arc_imgui_ctx) return 0;

    const ImGuiContext* ctx = static_cast<const ImGuiContext*>(arc_imgui_ctx);
    if (!LooksSane(ctx)) return 0;

    int n = 0;
    for (int i = 0; i < ctx->Windows.Size && n < ARC_MAX_WINDOWS; ++i) {
        const ImGuiWindow* w = ctx->Windows[i];
        if (!w || !w->WasActive || w->Hidden) continue;
        /* Skip child windows — they're composed inside their parent's ID
         * space and aren't useful anchor targets. */
        if (w->Flags & ImGuiWindowFlags_ChildWindow) continue;
        /* Skip imgui-internal debug windows. Names starting with "##"
         * are hidden/internal by convention. */
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
