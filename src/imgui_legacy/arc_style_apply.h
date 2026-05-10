#pragma once

struct ImGuiStyle;
struct ArcStyleSnapshot;

namespace ImguiLegacy {
    /* Maps the captured 1.92.7 style onto a 1.80 ImGuiStyle in place.
     * Unknown color names are dropped; scalars that weren't captured (left
     * at ARC_STYLE_UNSET) are preserved.
     *
     * Side-effect: also writes ImGui::GetIO().FontGlobalScale from the
     * 1.92 font triplet (FontSizeBase * FontScaleMain * FontScaleDpi),
     * since 1.80's ImGuiStyle has no equivalent. The current ImGui
     * context must be the 1.80 one. */
    void ApplyArcStyle(const ArcStyleSnapshot& snap, ImGuiStyle& dst);
}
