#pragma once

/* Version-neutral handoff between the ImGui 1.92.7 reader TU and the 1.80
 * applier TU. Deliberately plain C — must not include either imgui.h.
 *
 * The reader captures this from arcdps's 1.92.7 context; the applier maps
 * it onto our private 1.80 ImGuiStyle by field name, so additions/renames
 * in either direction degrade to "unknown dropped" rather than corruption. */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ARC_STYLE_MAX_COLORS 128
#define ARC_STYLE_NAME_MAX   48

typedef struct ArcStyleColor {
    char  name[ARC_STYLE_NAME_MAX];  /* e.g. "WindowBg", "FrameBgHovered" */
    float rgba[4];
} ArcStyleColor;

typedef struct ArcStyleSnapshot {
    /* Scalars / vecs — names mirror ImGuiStyle members that exist in both
     * 1.80 and 1.92. Fields the reader can't find are left at the sentinel
     * value below and skipped by the applier. */
    float Alpha;
    float WindowPadding[2];
    float WindowRounding;
    float WindowBorderSize;
    float WindowMinSize[2];
    float WindowTitleAlign[2];
    float ChildRounding;
    float ChildBorderSize;
    float PopupRounding;
    float PopupBorderSize;
    float FramePadding[2];
    float FrameRounding;
    float FrameBorderSize;
    float ItemSpacing[2];
    float ItemInnerSpacing[2];
    float CellPadding[2];
    float TouchExtraPadding[2];
    float IndentSpacing;
    float ColumnsMinSpacing;
    float ScrollbarSize;
    float ScrollbarRounding;
    float GrabMinSize;
    float GrabRounding;
    float LogSliderDeadzone;
    float TabRounding;
    float TabBorderSize;
    /* TabMinWidthForCloseButton was split into separate selected/unselected
     * fields in 1.92 — skip rather than guess. */
    float ButtonTextAlign[2];
    float SelectableTextAlign[2];
    float DisplayWindowPadding[2];
    float DisplaySafeAreaPadding[2];
    float MouseCursorScale;
    int   AntiAliasedLines;        /* bool as int to keep the struct plain-C */
    int   AntiAliasedLinesUseTex;
    int   AntiAliasedFill;
    float CurveTessellationTol;
    /* 1.80 calls this CircleSegmentMaxError; 1.92 renamed to
     * CircleTessellationMaxError. Keep the 1.92 name here; the reader
     * sources from the 1.92 field, the applier writes to the 1.80 one. */
    float CircleTessellationMaxError;

    int           color_count;
    ArcStyleColor colors[ARC_STYLE_MAX_COLORS];
} ArcStyleSnapshot;

/* Sentinel — any field left at this value is treated as "not captured". */
#define ARC_STYLE_UNSET (-1e30f)

/* Zeros the snapshot and sets every scalar to ARC_STYLE_UNSET so the
 * applier treats un-captured fields as "leave at default". Call before
 * Capture. */
void ArcStyleSnapshot_Init(ArcStyleSnapshot* s);

/* Returns 1 on success. On failure (null ctx, imgui ABI mismatch) *out is
 * left at the Init'd sentinel state and the caller should skip styling. */
int ArcStyleReader_Capture(void* arc_imgui_ctx, ArcStyleSnapshot* out);

/* Returns a pointer to a static diagnostic string describing the first
 * failed sanity check since process start (or empty if none). Useful for
 * one-shot logging — saves the caller from spamming per-frame failures. */
const char* ArcStyleReader_LastFailure(void);

/* After a successful Capture, describes where Style was found and the
 * offset delta between arcdps's layout and ours. Used by the windows
 * reader to correct its own field offset and by the context init to
 * surface the result in the log. */
const char* ArcStyleReader_LayoutDiagnostic(void);
long long   ArcStyleReader_LayoutDelta(void);  /* 0 when layouts agree */
int         ArcStyleReader_LayoutKnown(void);

#ifdef __cplusplus
}
#endif
