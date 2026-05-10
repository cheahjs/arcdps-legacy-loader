#include "style_persist.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>

namespace {
    bool       g_snapshot_init   = false;
    ImGuiStyle g_snapshot{};
    /* IO state we also persist (ShowStyleEditor's Fonts tab edits this, not
     * ImGuiStyle). Snapshot separately since it lives outside ImGuiStyle. */
    float      g_snapshot_font_global_scale = 1.0f;

    /* "User style": the live style as edited while match-arcdps was off. Kept
     * separate from the live ImGui style so a match-on capture (arcdps theme)
     * doesn't overwrite the user's saved customizations. Initialized from the
     * live style on first use, updated by ReadLineFn at load and by
     * DetectStyleEditsForAutoSave while match-arcdps is off, and written out
     * by WriteAllFn. ApplyUserStyleToLive copies it back over the live style
     * when match-arcdps toggles off. */
    bool       g_user_init       = false;
    ImGuiStyle g_user_style{};
    float      g_user_font_global_scale = 1.0f;

    void EnsureUserInit() {
        if (g_user_init) return;
        g_user_style             = ImGui::GetStyle();
        g_user_font_global_scale = ImGui::GetIO().FontGlobalScale;
        g_user_init              = true;
    }

    struct FloatField { const char* name; float ImGuiStyle::*ptr; };
    struct Vec2Field  { const char* name; ImVec2 ImGuiStyle::*ptr; };
    struct BoolField  { const char* name; bool   ImGuiStyle::*ptr; };
    struct DirField   { const char* name; ImGuiDir ImGuiStyle::*ptr; };

    const FloatField kFloats[] = {
        { "Alpha",                     &ImGuiStyle::Alpha },
        { "WindowRounding",            &ImGuiStyle::WindowRounding },
        { "WindowBorderSize",          &ImGuiStyle::WindowBorderSize },
        { "ChildRounding",             &ImGuiStyle::ChildRounding },
        { "ChildBorderSize",           &ImGuiStyle::ChildBorderSize },
        { "PopupRounding",             &ImGuiStyle::PopupRounding },
        { "PopupBorderSize",           &ImGuiStyle::PopupBorderSize },
        { "FrameRounding",             &ImGuiStyle::FrameRounding },
        { "FrameBorderSize",           &ImGuiStyle::FrameBorderSize },
        { "IndentSpacing",             &ImGuiStyle::IndentSpacing },
        { "ColumnsMinSpacing",         &ImGuiStyle::ColumnsMinSpacing },
        { "ScrollbarSize",             &ImGuiStyle::ScrollbarSize },
        { "ScrollbarRounding",         &ImGuiStyle::ScrollbarRounding },
        { "GrabMinSize",               &ImGuiStyle::GrabMinSize },
        { "GrabRounding",              &ImGuiStyle::GrabRounding },
        { "LogSliderDeadzone",         &ImGuiStyle::LogSliderDeadzone },
        { "TabRounding",               &ImGuiStyle::TabRounding },
        { "TabBorderSize",             &ImGuiStyle::TabBorderSize },
        { "TabMinWidthForCloseButton", &ImGuiStyle::TabMinWidthForCloseButton },
        { "MouseCursorScale",          &ImGuiStyle::MouseCursorScale },
        { "CurveTessellationTol",      &ImGuiStyle::CurveTessellationTol },
        { "CircleSegmentMaxError",     &ImGuiStyle::CircleSegmentMaxError },
    };
    const Vec2Field kVecs[] = {
        { "WindowPadding",          &ImGuiStyle::WindowPadding },
        { "WindowMinSize",          &ImGuiStyle::WindowMinSize },
        { "WindowTitleAlign",       &ImGuiStyle::WindowTitleAlign },
        { "FramePadding",           &ImGuiStyle::FramePadding },
        { "ItemSpacing",            &ImGuiStyle::ItemSpacing },
        { "ItemInnerSpacing",       &ImGuiStyle::ItemInnerSpacing },
        { "CellPadding",            &ImGuiStyle::CellPadding },
        { "TouchExtraPadding",      &ImGuiStyle::TouchExtraPadding },
        { "ButtonTextAlign",        &ImGuiStyle::ButtonTextAlign },
        { "SelectableTextAlign",    &ImGuiStyle::SelectableTextAlign },
        { "DisplayWindowPadding",   &ImGuiStyle::DisplayWindowPadding },
        { "DisplaySafeAreaPadding", &ImGuiStyle::DisplaySafeAreaPadding },
    };
    const BoolField kBools[] = {
        { "AntiAliasedLines",       &ImGuiStyle::AntiAliasedLines },
        { "AntiAliasedLinesUseTex", &ImGuiStyle::AntiAliasedLinesUseTex },
        { "AntiAliasedFill",        &ImGuiStyle::AntiAliasedFill },
    };
    const DirField kDirs[] = {
        { "WindowMenuButtonPosition", &ImGuiStyle::WindowMenuButtonPosition },
        { "ColorButtonPosition",      &ImGuiStyle::ColorButtonPosition },
    };

    /* 1.80 color name → ImGuiCol_ index. Reused across reads/writes. */
    const std::unordered_map<std::string, int>& ColorNameTable() {
        static auto table = [] {
            std::unordered_map<std::string, int> m;
            for (int i = 0; i < ImGuiCol_COUNT; ++i)
                if (const char* n = ImGui::GetStyleColorName(i)) m.emplace(n, i);
            return m;
        }();
        return table;
    }

    /* Matches "name=" prefix and returns the start of the value, or nullptr. */
    const char* MatchKey(const char* line, const char* key) {
        size_t n = std::strlen(key);
        if (std::strncmp(line, key, n) != 0) return nullptr;
        if (line[n] != '=') return nullptr;
        return line + n + 1;
    }

    void* ReadOpenFn(ImGuiContext*, ImGuiSettingsHandler*, const char* name) {
        /* Single-bucket section; ignore any other names that might appear. */
        return std::strcmp(name, "Default") == 0 ? (void*)1 : nullptr;
    }

    void ReadLineFn(ImGuiContext*, ImGuiSettingsHandler*, void*, const char* line) {
        /* Mirror every parsed field into the user-style snapshot too so it
         * starts each session populated with what's on disk. Any field not
         * present in the ini stays at whatever live had when EnsureUserInit
         * first ran (StyleColorsDark defaults). */
        EnsureUserInit();
        ImGuiStyle& s = ImGui::GetStyle();

        if (auto v = MatchKey(line, "FontGlobalScale")) {
            float x;
            if (std::sscanf(v, "%f", &x) == 1) {
                ImGui::GetIO().FontGlobalScale = x;
                g_user_font_global_scale       = x;
            }
            return;
        }

        for (auto& f : kFloats) {
            if (auto v = MatchKey(line, f.name)) {
                float x;
                if (std::sscanf(v, "%f", &x) == 1) {
                    s.*f.ptr           = x;
                    g_user_style.*f.ptr = x;
                }
                return;
            }
        }
        for (auto& f : kVecs) {
            if (auto v = MatchKey(line, f.name)) {
                float x, y;
                if (std::sscanf(v, "%f,%f", &x, &y) == 2) {
                    s.*f.ptr            = ImVec2(x, y);
                    g_user_style.*f.ptr = ImVec2(x, y);
                }
                return;
            }
        }
        for (auto& f : kBools) {
            if (auto v = MatchKey(line, f.name)) {
                int x;
                if (std::sscanf(v, "%d", &x) == 1) {
                    s.*f.ptr            = (x != 0);
                    g_user_style.*f.ptr = (x != 0);
                }
                return;
            }
        }
        for (auto& f : kDirs) {
            if (auto v = MatchKey(line, f.name)) {
                int x;
                if (std::sscanf(v, "%d", &x) == 1) {
                    s.*f.ptr            = (ImGuiDir)x;
                    g_user_style.*f.ptr = (ImGuiDir)x;
                }
                return;
            }
        }
        /* Color.<name>=R,G,B,A */
        if (std::strncmp(line, "Color.", 6) == 0) {
            char  cname[64];
            float r, g, b, a;
            if (std::sscanf(line, "Color.%63[^=]=%f,%f,%f,%f", cname, &r, &g, &b, &a) == 5) {
                const auto& tbl = ColorNameTable();
                auto it = tbl.find(cname);
                if (it != tbl.end()) {
                    s.Colors[it->second]            = ImVec4(r, g, b, a);
                    g_user_style.Colors[it->second] = ImVec4(r, g, b, a);
                }
            }
        }
    }

    void WriteAllFn(ImGuiContext*, ImGuiSettingsHandler*, ImGuiTextBuffer* buf) {
        /* Persist the user-style snapshot, not the live style. While match-
         * arcdps is on the live style is the arcdps capture (or transient
         * Appearance-tab edits that get discarded on the next capture) —
         * neither belongs in the user's saved style. */
        EnsureUserInit();
        const ImGuiStyle& s = g_user_style;
        buf->appendf("[Style][Default]\n");
        buf->appendf("FontGlobalScale=%g\n", g_user_font_global_scale);
        for (auto& f : kFloats) buf->appendf("%s=%g\n",       f.name, s.*f.ptr);
        for (auto& f : kVecs)   buf->appendf("%s=%g,%g\n",    f.name, (s.*f.ptr).x, (s.*f.ptr).y);
        for (auto& f : kBools)  buf->appendf("%s=%d\n",       f.name, (s.*f.ptr) ? 1 : 0);
        for (auto& f : kDirs)   buf->appendf("%s=%d\n",       f.name, (int)(s.*f.ptr));
        for (int i = 0; i < ImGuiCol_COUNT; ++i) {
            const char* n = ImGui::GetStyleColorName(i);
            const ImVec4& c = s.Colors[i];
            buf->appendf("Color.%s=%g,%g,%g,%g\n", n ? n : "?", c.x, c.y, c.z, c.w);
        }
        buf->append("\n");
    }
}

namespace ImguiLegacy {

void RegisterStyleSettingsHandler() {
    /* 1.80 doesn't expose ImGui::AddSettingsHandler — push into the context
     * vector directly, after a duplicate guard. Brace-init zeroes the
     * optional callbacks (ReadInitFn, ApplyAllFn) and UserData; imgui
     * iterates them with null guards but only after they exist as raw
     * memory in the ImVector slot, so leaving them indeterminate would
     * be a footgun. */
    if (ImGui::FindSettingsHandler("Style")) return;

    ImGuiSettingsHandler h{};
    h.TypeName   = "Style";
    h.TypeHash   = ImHashStr("Style");
    h.ReadOpenFn = ReadOpenFn;
    h.ReadLineFn = ReadLineFn;
    h.WriteAllFn = WriteAllFn;
    GImGui->SettingsHandlers.push_back(h);

    /* Seed the user-style snapshot from the current live style (typically
     * StyleColorsDark from Init) so any field absent from the ini stays at
     * a sensible default. ReadLineFn fills in the rest. */
    EnsureUserInit();
}

void DetectStyleEditsForAutoSave(bool match_arcdps) {
    const ImGuiStyle& s   = ImGui::GetStyle();
    const float       fgs = ImGui::GetIO().FontGlobalScale;
    if (!g_snapshot_init) {
        g_snapshot                   = s;
        g_snapshot_font_global_scale = fgs;
        g_snapshot_init              = true;
        return;
    }
    if (std::memcmp(&g_snapshot, &s, sizeof(ImGuiStyle)) != 0
        || g_snapshot_font_global_scale != fgs) {
        g_snapshot                   = s;
        g_snapshot_font_global_scale = fgs;
        /* While match-arcdps is on, the live style is the arcdps capture
         * (or transient Appearance-tab edits we explicitly don't persist).
         * Only sync edits into the user style — and mark the ini dirty —
         * when match is off, where the live style IS the user style. */
        if (!match_arcdps) {
            EnsureUserInit();
            g_user_style             = s;
            g_user_font_global_scale = fgs;
            ImGui::MarkIniSettingsDirty();
        }
    }
}

void ApplyUserStyleToLive() {
    EnsureUserInit();
    ImGui::GetStyle()              = g_user_style;
    ImGui::GetIO().FontGlobalScale = g_user_font_global_scale;
    /* Pre-seed the dirty-detection snapshot so the next call doesn't see
     * this restore as a fresh edit and uselessly mark the ini dirty. */
    g_snapshot                   = g_user_style;
    g_snapshot_font_global_scale = g_user_font_global_scale;
    g_snapshot_init              = true;
}

}
