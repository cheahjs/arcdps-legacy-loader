#pragma once

#include <windows.h>
#include <string>
#include <arcdps/arcdps_structs.h>

/* One legacy arcdps addon dll loaded by the loader. */
class LegacyAddon {
public:
    LegacyAddon() = default;
    ~LegacyAddon() { Unload(); }

    LegacyAddon(const LegacyAddon&)            = delete;
    LegacyAddon& operator=(const LegacyAddon&) = delete;
    LegacyAddon(LegacyAddon&& o) noexcept;
    LegacyAddon& operator=(LegacyAddon&& o) noexcept;

    bool Load(const std::wstring& path, void* legacy_imguictx);
    void Unload();

    bool Loaded() const { return m_module != nullptr; }
    const char* Name() const { return m_exports && m_exports->out_name ? m_exports->out_name : "?"; }

    void CallImgui(uint32_t not_charsel_or_loading);
    /* Renders the addon's own options tab content (field: options_end). */
    void CallOptionsTab();
    /* Called once per arcdps-known window name with a trailing nullptr. Non-zero
     * return suppresses arcdps's default checkbox for that name. */
    uint32_t CallOptionsWindows(const char* window_name);
    void CallCombat(cbtevent*, ag*, ag*, const char*, uint64_t, uint64_t);
    void CallCombatLocal(cbtevent*, ag*, ag*, const char*, uint64_t, uint64_t);
    uint32_t CallWndFilter(HWND, UINT, WPARAM, LPARAM);
    uint32_t CallWndNoFilter(HWND, UINT, WPARAM, LPARAM);

private:
    HMODULE         m_module  = nullptr;
    arcdps_exports* m_exports = nullptr;
    mod_release_t   m_release = nullptr;
    std::wstring    m_path;
};
