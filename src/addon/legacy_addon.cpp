#include "legacy_addon.h"
#include "proxy/arcdps_proxy.h"

#include <utility>

LegacyAddon::LegacyAddon(LegacyAddon&& o) noexcept
    : m_module(o.m_module), m_exports(o.m_exports), m_release(o.m_release),
      m_path(std::move(o.m_path)), m_name(std::move(o.m_name)),
      m_build(std::move(o.m_build)), m_sig(o.m_sig),
      m_imguivers(o.m_imguivers), m_status(o.m_status) {
    o.m_module  = nullptr;
    o.m_exports = nullptr;
    o.m_release = nullptr;
}

LegacyAddon& LegacyAddon::operator=(LegacyAddon&& o) noexcept {
    if (this != &o) {
        Unload();
        m_module    = o.m_module;
        m_exports   = o.m_exports;
        m_release   = o.m_release;
        m_path      = std::move(o.m_path);
        m_name      = std::move(o.m_name);
        m_build     = std::move(o.m_build);
        m_sig       = o.m_sig;
        m_imguivers = o.m_imguivers;
        m_status    = o.m_status;
        o.m_module  = nullptr;
        o.m_exports = nullptr;
        o.m_release = nullptr;
    }
    return *this;
}

bool LegacyAddon::Load(const std::wstring& path, void* legacy_imguictx) {
    m_path   = path;
    m_module = LoadLibraryW(path.c_str());
    if (!m_module) { m_status = Status::LoadLibraryFailed; return false; }

    auto get_init    = reinterpret_cast<get_init_addr_fn_t>(GetProcAddress(m_module, "get_init_addr"));
    auto get_release = reinterpret_cast<get_release_addr_fn_t>(GetProcAddress(m_module, "get_release_addr"));
    if (!get_init) { Reject(Status::MissingGetInit); return false; }

    const auto& h = ArcdpsProxy::Get();
    /* Pass the legacy addon its 1.80 context while forwarding arcdps's real
     * HMODULE as arcdll so its GetProcAddress("e3") resolves directly. */
    auto mod_init = get_init(
        const_cast<char*>(h.arcversion), legacy_imguictx, h.id3dptr,
        h.arcdll, h.mallocfn, h.freefn, h.d3dversion);
    if (!mod_init) { Reject(Status::InitFailed); return false; }

    m_exports = mod_init();
    if (!m_exports) { Reject(Status::InitFailed); return false; }

    m_release   = get_release ? get_release() : nullptr;
    m_sig       = m_exports->sig;
    m_imguivers = m_exports->imguivers;
    if (m_exports->out_name)  m_name  = m_exports->out_name;
    if (m_exports->out_build) m_build = m_exports->out_build;
    m_status    = Status::Loaded;
    return true;
}

uintptr_t LegacyAddon::End() const {
    if (!m_module) return 0;
    const auto* base = reinterpret_cast<const uint8_t*>(m_module);
    const auto* dos  = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    return reinterpret_cast<uintptr_t>(base) + nt->OptionalHeader.SizeOfImage;
}

void LegacyAddon::Reject(Status reason) {
    Unload();
    m_status = reason;
}

void LegacyAddon::Unload() {
    if (m_release) { m_release(); m_release = nullptr; }
    if (m_module)  { FreeLibrary(m_module); m_module = nullptr; }
    m_exports = nullptr;
}

void LegacyAddon::CallImgui(uint32_t not_charsel_or_loading, uint32_t hide_if_combat_or_ooc) {
    if (m_exports && m_exports->imgui)
        reinterpret_cast<imgui_cb_t>(m_exports->imgui)(not_charsel_or_loading, hide_if_combat_or_ooc);
}

void LegacyAddon::CallOptionsTab() {
    if (m_exports && m_exports->options_end)
        reinterpret_cast<options_end_cb_t>(m_exports->options_end)();
}

uint32_t LegacyAddon::CallOptionsWindows(const char* name) {
    if (m_exports && m_exports->options_windows)
        return reinterpret_cast<options_windows_cb_t>(m_exports->options_windows)(name);
    return 0;
}

void LegacyAddon::CallCombat(cbtevent* ev, ag* src, ag* dst, const char* sk, uint64_t id, uint64_t rev) {
    if (m_exports && m_exports->combat)
        reinterpret_cast<combat_cb_t>(m_exports->combat)(ev, src, dst, sk, id, rev);
}

void LegacyAddon::CallCombatLocal(cbtevent* ev, ag* src, ag* dst, const char* sk, uint64_t id, uint64_t rev) {
    if (m_exports && m_exports->combat_local)
        reinterpret_cast<combat_cb_t>(m_exports->combat_local)(ev, src, dst, sk, id, rev);
}

uint32_t LegacyAddon::CallWndFilter(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (m_exports && m_exports->wnd_filter)
        return reinterpret_cast<wndproc_cb_t>(m_exports->wnd_filter)(hwnd, msg, wp, lp);
    return msg;
}

uint32_t LegacyAddon::CallWndNoFilter(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (m_exports && m_exports->wnd_nofilter)
        return reinterpret_cast<wndproc_cb_t>(m_exports->wnd_nofilter)(hwnd, msg, wp, lp);
    return msg;
}
