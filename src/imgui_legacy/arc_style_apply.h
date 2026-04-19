#pragma once

struct ImGuiStyle;
struct ArcStyleSnapshot;

namespace ImguiLegacy {
    /* Maps the captured 1.92.7 style onto a 1.80 ImGuiStyle in place.
     * Unknown color names are dropped; scalars that weren't captured (left
     * at ARC_STYLE_UNSET) are preserved. */
    void ApplyArcStyle(const ArcStyleSnapshot& snap, ImGuiStyle& dst);
}
