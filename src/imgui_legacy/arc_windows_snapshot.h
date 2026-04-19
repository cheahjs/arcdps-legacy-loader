#pragma once

/* Version-neutral per-frame snapshot of arcdps's ImGui windows. The reader
 * walks arcdps's 1.92.7 ImGuiContext.Windows list and fills this; the
 * mirror pass creates invisible 1.80 shadow windows at matching names
 * so legacy addons (Boon Table et al.) can anchor against them via
 * ImGui::FindWindowByID on our 1.80 context.
 *
 * Plain C — must not include imgui.h so the 1.80 mirror and 1.92 reader
 * TUs can both consume it. */

#ifdef __cplusplus
extern "C" {
#endif

#define ARC_WINDOW_NAME_MAX 64
#define ARC_MAX_WINDOWS     128

typedef struct ArcWindow {
    char  name[ARC_WINDOW_NAME_MAX];
    float pos[2];
    float size[2];
} ArcWindow;

typedef struct ArcWindowList {
    int       count;
    ArcWindow items[ARC_MAX_WINDOWS];
} ArcWindowList;

/* Fills *out with arcdps's currently-visible top-level windows. Returns
 * 1 on success, 0 if the context fails a plausibility check (same ABI
 * risk as the style reader). On failure *out->count is set to 0. */
int ArcWindowsReader_Capture(void* arc_imgui_ctx, ArcWindowList* out);

#ifdef __cplusplus
}
#endif
