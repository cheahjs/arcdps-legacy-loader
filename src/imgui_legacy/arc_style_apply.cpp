#include "arc_style_apply.h"
#include "arc_style_snapshot.h"

#include <imgui.h>

#include <cstring>
#include <unordered_map>
#include <string>

namespace {
    /* Builds once: 1.80 color-name → ImGuiCol_ index. Iterating imgui's own
     * GetStyleColorName keeps us honest if the 1.80 enum shifts in future. */
    const std::unordered_map<std::string, int>& LegacyColorTable() {
        static std::unordered_map<std::string, int> t = []{
            std::unordered_map<std::string, int> m;
            for (int i = 0; i < ImGuiCol_COUNT; ++i)
                if (const char* n = ImGui::GetStyleColorName(i)) m.emplace(n, i);
            return m;
        }();
        return t;
    }

    bool IsSet(float v) { return v != ARC_STYLE_UNSET; }
    void MaybeAssign(float& dst, float src) { if (IsSet(src)) dst = src; }
    void MaybeAssign(ImVec2& dst, const float src[2]) {
        if (IsSet(src[0]) && IsSet(src[1])) dst = ImVec2(src[0], src[1]);
    }
}

namespace ImguiLegacy {

void ApplyArcStyle(const ArcStyleSnapshot& s, ImGuiStyle& d) {
    MaybeAssign(d.Alpha,                s.Alpha);
    MaybeAssign(d.WindowPadding,        s.WindowPadding);
    MaybeAssign(d.WindowRounding,       s.WindowRounding);
    MaybeAssign(d.WindowBorderSize,     s.WindowBorderSize);
    MaybeAssign(d.WindowMinSize,        s.WindowMinSize);
    MaybeAssign(d.WindowTitleAlign,     s.WindowTitleAlign);
    MaybeAssign(d.ChildRounding,        s.ChildRounding);
    MaybeAssign(d.ChildBorderSize,      s.ChildBorderSize);
    MaybeAssign(d.PopupRounding,        s.PopupRounding);
    MaybeAssign(d.PopupBorderSize,      s.PopupBorderSize);
    MaybeAssign(d.FramePadding,         s.FramePadding);
    MaybeAssign(d.FrameRounding,        s.FrameRounding);
    MaybeAssign(d.FrameBorderSize,      s.FrameBorderSize);
    MaybeAssign(d.ItemSpacing,          s.ItemSpacing);
    MaybeAssign(d.ItemInnerSpacing,     s.ItemInnerSpacing);
    MaybeAssign(d.CellPadding,          s.CellPadding);
    MaybeAssign(d.TouchExtraPadding,    s.TouchExtraPadding);
    MaybeAssign(d.IndentSpacing,        s.IndentSpacing);
    MaybeAssign(d.ColumnsMinSpacing,    s.ColumnsMinSpacing);
    MaybeAssign(d.ScrollbarSize,        s.ScrollbarSize);
    MaybeAssign(d.ScrollbarRounding,    s.ScrollbarRounding);
    MaybeAssign(d.GrabMinSize,          s.GrabMinSize);
    MaybeAssign(d.GrabRounding,         s.GrabRounding);
    MaybeAssign(d.LogSliderDeadzone,    s.LogSliderDeadzone);
    MaybeAssign(d.TabRounding,          s.TabRounding);
    MaybeAssign(d.TabBorderSize,        s.TabBorderSize);
    MaybeAssign(d.ButtonTextAlign,      s.ButtonTextAlign);
    MaybeAssign(d.SelectableTextAlign,  s.SelectableTextAlign);
    MaybeAssign(d.DisplayWindowPadding, s.DisplayWindowPadding);
    MaybeAssign(d.DisplaySafeAreaPadding, s.DisplaySafeAreaPadding);
    MaybeAssign(d.MouseCursorScale,     s.MouseCursorScale);
    if (s.AntiAliasedLines       >= 0) d.AntiAliasedLines       = s.AntiAliasedLines       != 0;
    if (s.AntiAliasedLinesUseTex >= 0) d.AntiAliasedLinesUseTex = s.AntiAliasedLinesUseTex != 0;
    if (s.AntiAliasedFill        >= 0) d.AntiAliasedFill        = s.AntiAliasedFill        != 0;
    MaybeAssign(d.CurveTessellationTol,       s.CurveTessellationTol);
    MaybeAssign(d.CircleSegmentMaxError,      s.CircleTessellationMaxError);

    /* 1.92 renamed several colors that still exist in 1.80 under the
     * old names. Without this map, e.g. arcdps's TabSelected never
     * lands on 1.80's TabActive, so tabs stay bluish even though every
     * other color matches. Applied before the direct name lookup. */
    static const std::unordered_map<std::string, const char*> kRenames = {
        { "TabSelected",       "TabActive" },
        { "TabDimmed",         "TabUnfocused" },
        { "TabDimmedSelected", "TabUnfocusedActive" },
        { "NavCursor",         "NavHighlight" },
    };

    const auto& table = LegacyColorTable();
    for (int i = 0; i < s.color_count; ++i) {
        const char* name = s.colors[i].name;
        auto it = table.find(name);
        if (it == table.end()) {
            auto r = kRenames.find(name);
            if (r != kRenames.end()) it = table.find(r->second);
        }
        if (it == table.end()) continue;  /* genuinely new in 1.92 — drop */
        d.Colors[it->second] = ImVec4(
            s.colors[i].rgba[0], s.colors[i].rgba[1],
            s.colors[i].rgba[2], s.colors[i].rgba[3]);
    }
}

}
