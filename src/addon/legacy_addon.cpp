#include "legacy_addon.h"
#include "proxy/arcdps_proxy.h"

#include <utility>

LegacyAddon::LegacyAddon(LegacyAddon&& o) noexcept
    : m_module(o.m_module), m_exports(o.m_exports), m_release(o.m_release),
      m_path(std::move(o.m_path)) {
    o.m_module  = nullptr;
    o.m_exports = nullptr;
    o.m_release = nullptr;
}

LegacyAddon& LegacyAddon::operator=(LegacyAddon&& o) noexcept {
    if (this != &o) {
        Unload();
        m_module   = o.m_module;
        m_exports  = o.m_exports;
        m_release  = o.m_release;
        m_path     = std::move(o.m_path);
        o.m_module  = nullptr;
        o.m_exports = nullptr;
        o.m_release = nullptr;
    }
    return *this;
}

bool LegacyAddon::Load(const std::wstring& path, void* legacy_imguictx) {
    m_path   = path;
    m_module = LoadLibraryW(path.c_str());
    if (!m_module) return false;

    auto get_init    = reinterpret_cast<get_init_addr_fn_t>(GetProcAddress(m_module, "get_init_addr"));
    auto get_release = reinterpret_cast<get_release_addr_fn_t>(GetProcAddress(m_module, "get_release_addr"));
    if (!get_init) { Unload(); return false; }

    const auto& h = ArcdpsProxy::Get();
    /* Pass the legacy addon its 1.80 context while forwarding arcdps's real
     * HMODULE as arcdll so its GetProcAddress("e3") resolves directly. */
    auto mod_init = get_init(
        const_cast<char*>(h.arcversion), legacy_imguictx, h.id3dptr,
        h.arcdll, h.mallocfn, h.freefn, h.d3dversion);
    if (!mod_init) { Unload(); return false; }

    m_exports = mod_init();
    if (!m_exports) { Unload(); return false; }

    m_release = get_release ? get_release() : nullptr;
    return true;
}

void LegacyAddon::Unload() {
    if (m_release) { m_release(); m_release = nullptr; }
    if (m_module)  { FreeLibrary(m_module); m_module = nullptr; }
    m_exports = nullptr;
}

void LegacyAddon::CallImgui(uint32_t flag) {
    if (m_exports && m_exports->imgui)
        reinterpret_cast<imgui_cb_t>(m_exports->imgui)(flag);
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
