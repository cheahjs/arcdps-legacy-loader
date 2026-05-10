#pragma once

/* Round-trips ImGui::GetStyle() through the imgui.ini file the legacy 1.80
 * context already uses for window positions. Lets the user's tweaks in the
 * Appearance tab's ShowStyleEditor survive across launches.
 *
 * The mechanism is a standard ImGuiSettingsHandler ([Style][Default]
 * section) plus a per-frame memcmp that fires MarkIniSettingsDirty when
 * the style mutates, so imgui's auto-save flushes ~IniSavingRate seconds
 * after the last edit. */

namespace ImguiLegacy {
    /* Adds the [Style][Default] handler to the current ImGui context. Must
     * run after CreateContext but before LoadIniSettingsFromDisk so the
     * handler picks up any saved style on the first load pass. */
    void RegisterStyleSettingsHandler();

    /* True if the most recent LoadIniSettingsFromDisk pass populated at
     * least one style field via the handler. Caller uses this to skip the
     * deferred arcdps style capture so the persisted style wins on launch. */
    bool StyleWasLoadedFromIni();

    /* Diffs ImGui::GetStyle() against a cached snapshot. On change updates
     * the snapshot and calls MarkIniSettingsDirty. Cheap (one memcmp);
     * intended to run once per frame at the start of NewFrame so edits
     * from the previous frame's UI are picked up. */
    void DetectStyleEditsForAutoSave();
}
