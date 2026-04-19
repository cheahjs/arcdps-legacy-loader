#pragma once

#include <windows.h>
#include <cstdint>
#include <string>
#include <arcdps/arcdps_structs.h>

/* One legacy arcdps addon dll loaded by the loader. */
class LegacyAddon {
public:
    enum class Status {
        Loaded,
        LoadLibraryFailed,
        MissingGetInit,
        InitFailed,
        ImguiVersionMismatch,
        DuplicateSig,
    };

    LegacyAddon() = default;
    ~LegacyAddon() { Unload(); }

    LegacyAddon(const LegacyAddon&)            = delete;
    LegacyAddon& operator=(const LegacyAddon&) = delete;
    LegacyAddon(LegacyAddon&& o) noexcept;
    LegacyAddon& operator=(LegacyAddon&& o) noexcept;

    /* Returns true only on Status::Loaded. Rejected addons still populate
     * m_path/m_status (and name/build/sig/imguivers when available) so the UI
     * can report them. */
    bool Load(const std::wstring& path, void* legacy_imguictx);
    /* Tear down the dll but keep metadata + mark a failure reason. Used by
     * AddonManager when rejecting an addon after Load() succeeded. */
    void Reject(Status reason);
    void Unload();

    bool        Loaded()    const { return m_status == Status::Loaded && m_module; }
    Status      State()     const { return m_status; }
    const char* Name()      const { return m_name.empty()  ? "?" : m_name.c_str();  }
    const char* Build()     const { return m_build.c_str(); }
    uint32_t    Sig()       const { return m_sig; }
    uint32_t    ImguiVers() const { return m_imguivers; }
    const std::wstring& Path() const { return m_path; }

    /* Load address + SizeOfImage from the PE header, so callers can log the
     * same "0xBASE-0xEND" range arcdps prints for its own extensions. */
    uintptr_t   Base()      const { return reinterpret_cast<uintptr_t>(m_module); }
    uintptr_t   End()       const;

    void CallImgui(uint32_t not_charsel_or_loading, uint32_t hide_if_combat_or_ooc);
    void CallOptionsTab();
    uint32_t CallOptionsWindows(const char* window_name);
    void CallCombat(cbtevent*, ag*, ag*, const char*, uint64_t, uint64_t);
    void CallCombatLocal(cbtevent*, ag*, ag*, const char*, uint64_t, uint64_t);
    uint32_t CallWndFilter(HWND, UINT, WPARAM, LPARAM);
    uint32_t CallWndNoFilter(HWND, UINT, WPARAM, LPARAM);

private:
    HMODULE         m_module    = nullptr;
    arcdps_exports* m_exports   = nullptr;
    mod_release_t   m_release   = nullptr;
    std::wstring    m_path;
    std::string     m_name;
    std::string     m_build;
    uint32_t        m_sig       = 0;
    uint32_t        m_imguivers = 0;
    Status          m_status    = Status::LoadLibraryFailed;
};
