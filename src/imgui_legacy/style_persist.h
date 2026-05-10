#pragma once

/* Round-trips a "user style" snapshot through the imgui.ini file the legacy
 * 1.80 context already uses for window positions. Lets the user's tweaks in
 * the Appearance tab's ShowStyleEditor survive across launches.
 *
 * The user style is kept separate from the live ImGui style so toggling
 * "Match arcdps style" on doesn't overwrite the user's customizations: while
 * match is on, the live style holds the arcdps capture and is volatile; the
 * user style only changes when match is off (where live IS the user style).
 *
 * The mechanism is a standard ImGuiSettingsHandler ([Style][Default] section)
 * plus a per-frame memcmp that, when match-arcdps is off, syncs the live
 * style into the user style and fires MarkIniSettingsDirty so imgui's
 * auto-save flushes ~IniSavingRate seconds after the last edit. */

namespace ImguiLegacy {
    /* Adds the [Style][Default] handler to the current ImGui context. Must
     * run after CreateContext but before LoadIniSettingsFromDisk so the
     * handler picks up any saved style on the first load pass. */
    void RegisterStyleSettingsHandler();

    /* Diffs ImGui::GetStyle() against a cached snapshot. On change updates
     * the snapshot; when match_arcdps is false also copies the live style
     * into the persisted user style and calls MarkIniSettingsDirty. Cheap
     * (one memcmp); intended to run once per frame at the start of NewFrame
     * so edits from the previous frame's UI are picked up. */
    void DetectStyleEditsForAutoSave(bool match_arcdps);

    /* Copies the persisted user style onto the live ImGui style and font
     * scale. Called when match-arcdps toggles from on to off so the user's
     * saved customizations come back instead of the live arcdps capture
     * being reset to 1.80 dark. */
    void ApplyUserStyleToLive();
}
