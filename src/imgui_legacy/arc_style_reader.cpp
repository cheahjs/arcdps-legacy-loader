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

#include <imgui.h>
#include <imgui_internal.h>

#include <cstring>

namespace {
    /* 1.92.7 color-enum → name. Duplicates imgui.cpp's GStyleColorNames
     * because we don't link that TU; must track upstream if arcdps moves. */
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
        "DockingPreview",         "DockingEmptyBg",
        "PlotLines",              "PlotLinesHovered",
        "PlotHistogram",          "PlotHistogramHovered",
        "TableHeaderBg",          "TableBorderStrong",    "TableBorderLight",
        "TableRowBg",             "TableRowBgAlt",
        "TextLink",               "TextSelectedBg",
        "TreeLines",
        "DragDropTarget",
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
    bool LooksSane(const ImGuiStyle& s) {
        if (!(s.Alpha >= 0.0f && s.Alpha <= 1.0f)) return false;
        if (!(s.WindowRounding >= 0.0f && s.WindowRounding <= 64.0f)) return false;
        if (!(s.FrameRounding  >= 0.0f && s.FrameRounding  <= 64.0f)) return false;
        if (!(s.WindowPadding.x >= 0.0f && s.WindowPadding.x <= 256.0f)) return false;
        if (!(s.FramePadding.x  >= 0.0f && s.FramePadding.x  <= 256.0f)) return false;
        if (!(s.ItemSpacing.x   >= 0.0f && s.ItemSpacing.x   <= 256.0f)) return false;
        return true;
    }
}

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

    const ImGuiContext* ctx = static_cast<const ImGuiContext*>(arc_imgui_ctx);
    const ImGuiStyle& s = ctx->Style;
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
