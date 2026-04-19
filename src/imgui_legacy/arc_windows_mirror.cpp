#include "arc_windows_mirror.h"
#include "arc_windows_snapshot.h"

#include <imgui.h>

namespace ImguiLegacy {

void MirrorArcdpsWindows(const ArcWindowList& list) {
    /* Every shadow is locked to arcdps's reported geometry, draws nothing,
     * eats no input, and is excluded from tab/focus navigation so it's
     * invisible to the user but still shows up in ImGui::FindWindowByID
     * / the context's Windows list. NoSavedSettings keeps shadows out of
     * imgui.ini — otherwise Begin() with a never-before-seen name would
     * write a [Window][Name] entry every time arcdps opens a new window. */
    const ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoInputs              |
        ImGuiWindowFlags_NoBackground          |
        ImGuiWindowFlags_NoTitleBar            |
        ImGuiWindowFlags_NoResize              |
        ImGuiWindowFlags_NoMove                |
        ImGuiWindowFlags_NoScrollbar           |
        ImGuiWindowFlags_NoScrollWithMouse     |
        ImGuiWindowFlags_NoCollapse            |
        ImGuiWindowFlags_NoNav                 |
        ImGuiWindowFlags_NoDecoration          |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoFocusOnAppearing    |
        ImGuiWindowFlags_NoSavedSettings;

    for (int i = 0; i < list.count; ++i) {
        const ArcWindow& w = list.items[i];
        ImGui::SetNextWindowPos (ImVec2(w.pos[0],  w.pos[1]));
        ImGui::SetNextWindowSize(ImVec2(w.size[0], w.size[1]));
        /* Begin + End regardless of return — imgui requires the pair. The
         * body stays empty so no draw commands are queued. */
        ImGui::Begin(w.name, nullptr, flags);
        ImGui::End();
    }
}

}
