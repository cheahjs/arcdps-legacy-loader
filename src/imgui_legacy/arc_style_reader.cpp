/* Reads ImGuiStyle out of arcdps's ImGui 1.92.7 context without linking
 * against 1.92's imgui implementation.
 *
 * We include only the 1.92 headers (public + imgui_internal.h for
 * ImGuiContext's layout), cast arc_imguictx to ImGuiContext*, and copy
 * fields straight out of ctx->Style. No functions are called on 1.92
 * imgui, so there are no symbols to collide with the 1.80 copy linked
 * into the same DLL.
 *
 * Safety: the Style offset inside ImGuiContext depends on the sizes of
 * ImGuiIO / ImGuiPlatformIO, which in turn depend on arcdps's imconfig.h.
 * If it doesn't match our vendored 1.92.7 stock build, we read garbage.
 * Capture() runs a plausibility check on the resulting values and bails
 * on anything out of range, so the worst case is "styling is skipped",
 * not corruption of a live context.
 *
 * This TU must NOT include the loader's 1.80 "imgui.h". Everything that
 * crosses the version boundary goes through the plain-C ArcStyleSnapshot. */

#include "arc_style_snapshot.h"
#include "logging/log.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <cstdio>
#include <cstring>

namespace {
    /* 1.92.7 color-enum → name. Order must mirror imgui.h's ImGuiCol_
     * enum exactly: reader fills Colors[i]'s name from this table, and
     * the applier keys off names to pick 1.80 targets. (We don't link
     * 1.92's GStyleColorNames table, so we reproduce it here.) */
    const char* const k1927ColorNames[] = {
        "Text",                   "TextDisabled",         "WindowBg",
        "ChildBg",                "PopupBg",              "Border",
        "BorderShadow",           "FrameBg",              "FrameBgHovered",
        "FrameBgActive",          "TitleBg",              "TitleBgActive",
        "TitleBgCollapsed",       "MenuBarBg",            "ScrollbarBg",
        "ScrollbarGrab",          "ScrollbarGrabHovered", "ScrollbarGrabActive",
        "CheckMark",              "SliderGrab",           "SliderGrabActive",
        "Button",                 "ButtonHovered",        "ButtonActive",
        "Header",                 "HeaderHovered",        "HeaderActive",
        "Separator",              "SeparatorHovered",     "SeparatorActive",
        "ResizeGrip",             "ResizeGripHovered",    "ResizeGripActive",
        "InputTextCursor",        "TabHovered",           "Tab",
        "TabSelected",            "TabSelectedOverline",  "TabDimmed",
        "TabDimmedSelected",      "TabDimmedSelectedOverline",
        "PlotLines",              "PlotLinesHovered",
        "PlotHistogram",          "PlotHistogramHovered",
        "TableHeaderBg",          "TableBorderStrong",    "TableBorderLight",
        "TableRowBg",             "TableRowBgAlt",
        "TextLink",               "TextSelectedBg",
        "TreeLines",
        "DragDropTarget",         "DragDropTargetBg",
        "UnsavedMarker",
        "NavCursor",              "NavWindowingHighlight","NavWindowingDimBg",
        "ModalWindowDimBg",
    };
    constexpr int k1927ColorNameCount = sizeof(k1927ColorNames) / sizeof(k1927ColorNames[0]);

    void CopyVec2(float dst[2], const ImVec2& v) { dst[0] = v.x; dst[1] = v.y; }

    void CopyName(char dst[ARC_STYLE_NAME_MAX], const char* src) {
        if (!src) { dst[0] = '\0'; return; }
        size_t n = std::strlen(src);
        if (n >= ARC_STYLE_NAME_MAX) n = ARC_STYLE_NAME_MAX - 1;
        std::memcpy(dst, src, n);
        dst[n] = '\0';
    }

    /* Plausibility gate: if any of these are outside known-sane ranges we
     * treat the read as ABI-mismatched and bail. Thresholds are loose on
     * purpose — we want to catch "ImGuiIO layout shifted so Style is at
     * the wrong offset and we got random bytes", not to police legitimate
     * arcdps tuning. */
    /* One-shot diagnostic dump for the caller. Populated on the first
     * failed LooksSane so we can see which field tripped it. Plain
     * buffer because the snapshot header is plain-C and we don't want
     * to widen the ABI for a debug payload. */
    char  g_last_fail[256] = {};
    bool  g_have_fail      = false;

    /* The layout-offset delta between arcdps's real ImGuiContext and the
     * one our vendored 1.92.7 headers describe. We locate the real
     * ImGuiStyle by pattern-scanning the first 6 floats of a populated
     * style (Alpha / DisabledAlpha / WindowPadding.x / WindowPadding.y /
     * WindowRounding / WindowBorderSize — stable across imconfig) and
     * remember how far off it was from where the compiler thinks Style
     * lives. The same delta is used to locate Windows for mirroring. */
    ptrdiff_t g_layout_delta = 0;
    bool      g_layout_known = false;
    char      g_layout_diag[160] = {};

    size_t ExpectedStyleOffset() {
        /* __builtin_offsetof tolerates ImGuiContext not being standard
         * layout — plain offsetof/null-deref trick would be UB and has
         * been seen folded to 0 under aggressive optimisation. */
        return __builtin_offsetof(ImGuiContext, Style);
    }

    ptrdiff_t ScanForStyle(const unsigned char* base, size_t scan_len) {
        /* 1.92.x ImGuiStyle layout anchor. The first 12 floats are:
         *   FontSizeBase, FontScaleMain, FontScaleDpi,   -- new in 1.92
         *   Alpha, DisabledAlpha,
         *   WindowPadding.{x,y},
         *   WindowRounding, WindowBorderSize, WindowBorderHoverPadding,
         *   WindowMinSize.{x,y}
         * We require all twelve to fall in plausible ranges; that's
         * enough signal to shake out false positives inside ImGuiIO. */
        /* Upper bound must leave room for the full ImGuiStyle read after
         * a match — otherwise the reinterpret_cast at the found offset
         * reaches past scan_len into unknown memory. */
        if (scan_len < sizeof(ImGuiStyle)) return -1;
        const size_t scan_end = scan_len - sizeof(ImGuiStyle);
        for (size_t off = 0; off <= scan_end; off += 4) {
            const float* f = reinterpret_cast<const float*>(base + off);
            if (!(f[0]  >= 4.0f  && f[0]  <= 96.0f))  continue; /* FontSizeBase */
            if (!(f[1]  >= 0.25f && f[1]  <= 8.0f))   continue; /* FontScaleMain */
            if (!(f[2]  >= 0.25f && f[2]  <= 8.0f))   continue; /* FontScaleDpi */
            if (!(f[3]  >= 0.3f  && f[3]  <= 1.01f))  continue; /* Alpha */
            if (!(f[4]  >= 0.2f  && f[4]  <= 1.01f))  continue; /* DisabledAlpha */
            if (!(f[5]  >= 0.0f  && f[5]  <= 24.0f))  continue; /* WindowPadding.x */
            if (!(f[6]  >= 0.0f  && f[6]  <= 24.0f))  continue; /* WindowPadding.y */
            if (!(f[7]  >= 0.0f  && f[7]  <= 16.0f))  continue; /* WindowRounding */
            if (!(f[8]  >= 0.0f  && f[8]  <= 2.0f))   continue; /* WindowBorderSize */
            if (!(f[9]  >= 0.0f  && f[9]  <= 8.0f))   continue; /* WindowBorderHoverPadding */
            if (!(f[10] >= 1.0f  && f[10] <= 256.0f)) continue; /* WindowMinSize.x */
            if (!(f[11] >= 1.0f  && f[11] <= 256.0f)) continue; /* WindowMinSize.y */
            return static_cast<ptrdiff_t>(off);
        }
        return -1;
    }

    void RecordFail(const char* why, const ImGuiStyle& s) {
        if (g_have_fail) return;
        snprintf(g_last_fail, sizeof(g_last_fail),
            "reason=%s alpha=%.3f wnd_rnd=%.2f frm_pad=(%.1f,%.1f) "
            "item_sp=%.2f indent=%.2f col[0].a=%.2f col[2].a=%.2f",
            why, s.Alpha, s.WindowRounding, s.FramePadding.x, s.FramePadding.y,
            s.ItemSpacing.x, s.IndentSpacing,
            s.Colors[0].w, s.Colors[2].w);
        g_have_fail = true;
    }

    bool LooksSane(const ImGuiStyle& s) {
        if (!(s.Alpha > 0.0f && s.Alpha <= 1.0f))                    { RecordFail("alpha",    s); return false; }
        if (!(s.WindowRounding >= 0.0f && s.WindowRounding <= 64.0f)){ RecordFail("wnd_rnd",  s); return false; }
        if (!(s.FrameRounding  >= 0.0f && s.FrameRounding  <= 64.0f)){ RecordFail("frm_rnd",  s); return false; }
        if (!(s.WindowPadding.x >= 0.0f && s.WindowPadding.x <= 256.0f)) { RecordFail("wnd_pad", s); return false; }
        if (!(s.FramePadding.x  >= 0.0f && s.FramePadding.x  <= 256.0f)) { RecordFail("frm_pad", s); return false; }
        if (s.ItemSpacing.x <= 0.0f || s.ItemSpacing.x > 256.0f)     { RecordFail("item_sp",  s); return false; }
        if (s.IndentSpacing <= 0.0f || s.IndentSpacing > 256.0f)     { RecordFail("indent",   s); return false; }
        bool any_opaque = false;
        for (int i = 0; i < ImGuiCol_COUNT; ++i)
            if (s.Colors[i].w > 0.0f) { any_opaque = true; break; }
        if (!any_opaque) { RecordFail("all_transparent", s); return false; }
        return true;
    }
}

extern "C" const char* ArcStyleReader_LastFailure(void)      { return g_last_fail; }
extern "C" const char* ArcStyleReader_LayoutDiagnostic(void) { return g_layout_diag; }
extern "C" long long   ArcStyleReader_LayoutDelta(void)      { return static_cast<long long>(g_layout_delta); }
extern "C" int         ArcStyleReader_LayoutKnown(void)      { return g_layout_known ? 1 : 0; }

extern "C" void ArcStyleSnapshot_Init(ArcStyleSnapshot* s) {
    if (!s) return;
    float* p     = &s->Alpha;
    float* p_end = &s->CircleTessellationMaxError + 1;
    for (float* it = p; it != p_end; ++it) *it = ARC_STYLE_UNSET;
    s->AntiAliasedLines       = -1;
    s->AntiAliasedLinesUseTex = -1;
    s->AntiAliasedFill        = -1;
    s->color_count = 0;
    std::memset(s->colors, 0, sizeof(s->colors));
}

extern "C" int ArcStyleReader_Capture(void* arc_imgui_ctx, ArcStyleSnapshot* out) {
    if (!arc_imgui_ctx || !out) return 0;

    const unsigned char* base = static_cast<const unsigned char*>(arc_imgui_ctx);

    /* First attempt: trust the compiled layout. If it passes sanity,
     * we're done and delta=0. Most builds will take this path. */
    if (!g_layout_known) {
        const ImGuiStyle& s0 = *reinterpret_cast<const ImGuiStyle*>(base + ExpectedStyleOffset());
        if (LooksSane(s0)) {
            g_layout_delta = 0;
            g_layout_known = true;
            snprintf(g_layout_diag, sizeof(g_layout_diag),
                     "style at compiled offset %zu (delta=0)", ExpectedStyleOffset());
        } else {
            /* Layout mismatch — scan a bounded window for the Style
             * signature. 64KB is well past where ImGuiContext lives. */
            ptrdiff_t found = ScanForStyle(base, 65536);
            if (found < 0) {
                /* Dump the first kilobyte as floats so we can find Style
                 * by eye — look for 1.0, 0.6, 8.0, 8.0 near each other. */
                char dump[2048];
                size_t cur = 0;
                const float* f = reinterpret_cast<const float*>(base);
                for (size_t i = 0; i < 256 && cur + 16 < sizeof(dump); ++i) {
                    int w = snprintf(dump + cur, sizeof(dump) - cur,
                                     "%zu:%.3g ", i * 4, f[i]);
                    if (w <= 0) break;
                    cur += static_cast<size_t>(w);
                }
                Log::Msg("ImguiLegacy: ctx dump (first 256 floats): %s", dump);
                snprintf(g_layout_diag, sizeof(g_layout_diag),
                         "scan_failed (expected=%zu)", ExpectedStyleOffset());
                return 0;
            }
            g_layout_delta = found - static_cast<ptrdiff_t>(ExpectedStyleOffset());
            g_layout_known = true;
            snprintf(g_layout_diag, sizeof(g_layout_diag),
                     "style scanned: expected=%zu found=%td delta=%+td",
                     ExpectedStyleOffset(), found, g_layout_delta);
        }
    }

    const ImGuiStyle& s = *reinterpret_cast<const ImGuiStyle*>(
        base + ExpectedStyleOffset() + g_layout_delta);
    if (!LooksSane(s)) return 0;

    out->Alpha                      = s.Alpha;
    CopyVec2(out->WindowPadding,       s.WindowPadding);
    out->WindowRounding             = s.WindowRounding;
    out->WindowBorderSize           = s.WindowBorderSize;
    CopyVec2(out->WindowMinSize,       s.WindowMinSize);
    CopyVec2(out->WindowTitleAlign,    s.WindowTitleAlign);
    out->ChildRounding              = s.ChildRounding;
    out->ChildBorderSize            = s.ChildBorderSize;
    out->PopupRounding              = s.PopupRounding;
    out->PopupBorderSize            = s.PopupBorderSize;
    CopyVec2(out->FramePadding,        s.FramePadding);
    out->FrameRounding              = s.FrameRounding;
    out->FrameBorderSize            = s.FrameBorderSize;
    CopyVec2(out->ItemSpacing,         s.ItemSpacing);
    CopyVec2(out->ItemInnerSpacing,    s.ItemInnerSpacing);
    CopyVec2(out->CellPadding,         s.CellPadding);
    CopyVec2(out->TouchExtraPadding,   s.TouchExtraPadding);
    out->IndentSpacing              = s.IndentSpacing;
    out->ColumnsMinSpacing          = s.ColumnsMinSpacing;
    out->ScrollbarSize              = s.ScrollbarSize;
    out->ScrollbarRounding          = s.ScrollbarRounding;
    out->GrabMinSize                = s.GrabMinSize;
    out->GrabRounding               = s.GrabRounding;
    out->LogSliderDeadzone          = s.LogSliderDeadzone;
    out->TabRounding                = s.TabRounding;
    out->TabBorderSize              = s.TabBorderSize;
    CopyVec2(out->ButtonTextAlign,     s.ButtonTextAlign);
    CopyVec2(out->SelectableTextAlign, s.SelectableTextAlign);
    CopyVec2(out->DisplayWindowPadding,   s.DisplayWindowPadding);
    CopyVec2(out->DisplaySafeAreaPadding, s.DisplaySafeAreaPadding);
    out->MouseCursorScale           = s.MouseCursorScale;
    out->AntiAliasedLines           = s.AntiAliasedLines       ? 1 : 0;
    out->AntiAliasedLinesUseTex     = s.AntiAliasedLinesUseTex ? 1 : 0;
    out->AntiAliasedFill            = s.AntiAliasedFill        ? 1 : 0;
    out->CurveTessellationTol       = s.CurveTessellationTol;
    out->CircleTessellationMaxError = s.CircleTessellationMaxError;

    int n = ImGuiCol_COUNT;
    if (n > k1927ColorNameCount) n = k1927ColorNameCount;
    if (n > ARC_STYLE_MAX_COLORS) n = ARC_STYLE_MAX_COLORS;
    out->color_count = n;
    for (int i = 0; i < n; ++i) {
        CopyName(out->colors[i].name, k1927ColorNames[i]);
        out->colors[i].rgba[0] = s.Colors[i].x;
        out->colors[i].rgba[1] = s.Colors[i].y;
        out->colors[i].rgba[2] = s.Colors[i].z;
        out->colors[i].rgba[3] = s.Colors[i].w;
    }
    return 1;
}
