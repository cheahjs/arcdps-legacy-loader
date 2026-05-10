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
        ImGuiStyle& s = ImGui::GetStyle();

        if (auto v = MatchKey(line, "FontGlobalScale")) {
            float x;
            if (std::sscanf(v, "%f", &x) == 1) ImGui::GetIO().FontGlobalScale = x;
            return;
        }

        for (auto& f : kFloats) {
            if (auto v = MatchKey(line, f.name)) {
                float x;
                if (std::sscanf(v, "%f", &x) == 1) s.*f.ptr = x;
                return;
            }
        }
        for (auto& f : kVecs) {
            if (auto v = MatchKey(line, f.name)) {
                float x, y;
                if (std::sscanf(v, "%f,%f", &x, &y) == 2) s.*f.ptr = ImVec2(x, y);
                return;
            }
        }
        for (auto& f : kBools) {
            if (auto v = MatchKey(line, f.name)) {
                int x;
                if (std::sscanf(v, "%d", &x) == 1) s.*f.ptr = (x != 0);
                return;
            }
        }
        for (auto& f : kDirs) {
            if (auto v = MatchKey(line, f.name)) {
                int x;
                if (std::sscanf(v, "%d", &x) == 1) s.*f.ptr = (ImGuiDir)x;
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
                if (it != tbl.end())
                    s.Colors[it->second] = ImVec4(r, g, b, a);
            }
        }
    }

    void WriteAllFn(ImGuiContext*, ImGuiSettingsHandler*, ImGuiTextBuffer* buf) {
        const ImGuiStyle& s = ImGui::GetStyle();
        buf->appendf("[Style][Default]\n");
        buf->appendf("FontGlobalScale=%g\n", ImGui::GetIO().FontGlobalScale);
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
}

void DetectStyleEditsForAutoSave() {
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
        ImGui::MarkIniSettingsDirty();
    }
}

}
