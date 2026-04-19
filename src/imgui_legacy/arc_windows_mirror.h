#pragma once

struct ArcWindowList;

namespace ImguiLegacy {
    /* Emits invisible 1.80 shadow windows matching each entry in `list`.
     * Called between ImGui::NewFrame() and legacy addon imgui callbacks
     * so addons that do ImGui::FindWindowByID(hash("Squad", ...)) see a
     * window at arcdps's live pos/size. */
    void MirrorArcdpsWindows(const ArcWindowList& list);
}
