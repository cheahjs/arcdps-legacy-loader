#include "context.h"

#include "logging/log.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <filesystem>
#include <string>

using Microsoft::WRL::ComPtr;

namespace {
    ImGuiContext*         g_ctx        = nullptr;
    ComPtr<ID3D11Device>  g_device;
    ComPtr<ID3D11DeviceContext> g_immediate;
    ComPtr<IDXGISwapChain>      g_swap;
    HWND                  g_hwnd       = nullptr;
    bool                  g_backend_up = false;
    /* Backing storage for ImGuiIO::IniFilename — imgui just stores the
     * pointer, so this must outlive the context. */
    std::string g_ini_path;
}

namespace ImguiLegacy {

void* Init(void* id3dptr, uint32_t d3dversion) {
    if (g_ctx) return g_ctx;
    if (d3dversion != 11 || !id3dptr) {
        Log::Msg("ImguiLegacy: unsupported d3dversion=%u", d3dversion);
        return nullptr;
    }

    g_swap = static_cast<IDXGISwapChain*>(id3dptr);
    if (FAILED(g_swap->GetDevice(__uuidof(ID3D11Device), &g_device))) {
        Log::Msg("ImguiLegacy: swap->GetDevice failed");
        return nullptr;
    }
    g_device->GetImmediateContext(&g_immediate);

    DXGI_SWAP_CHAIN_DESC desc{};
    g_swap->GetDesc(&desc);
    g_hwnd = desc.OutputWindow;

    IMGUI_CHECKVERSION();
    g_ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(g_ctx);

    /* Persist window positions/sizes next to the loader config. */
    wchar_t exe[MAX_PATH];
    DWORD exe_n = GetModuleFileNameW(nullptr, exe, MAX_PATH);
    namespace fs = std::filesystem;
    fs::path exe_dir = (exe_n > 0 && exe_n < MAX_PATH) ? fs::path(exe).parent_path() : fs::path();
    g_ini_path = (exe_dir / "addons" / "arcdps" / "legacy" / "imgui.ini").string();
    ImGui::GetIO().IniFilename = g_ini_path.c_str();

    if (!ImGui_ImplWin32_Init(g_hwnd) ||
        !ImGui_ImplDX11_Init(g_device.Get(), g_immediate.Get())) {
        Log::Msg("ImguiLegacy: backend init failed");
        Shutdown(); return nullptr;
    }
    /* Force shader/font creation now so a D3DCompile failure surfaces here
     * rather than silently no-op'ing every RenderDrawData. */
    if (!ImGui_ImplDX11_CreateDeviceObjects()) {
        Log::Msg("ImguiLegacy: ImGui_ImplDX11_CreateDeviceObjects failed — "
                 "legacy render path will be dark");
    }
    g_backend_up = true;
    return g_ctx;
}

void Shutdown() {
    if (g_backend_up) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        g_backend_up = false;
    }
    if (g_ctx) { ImGui::DestroyContext(g_ctx); g_ctx = nullptr; }
    g_swap.Reset();
    g_immediate.Reset();
    g_device.Reset();
    g_hwnd = nullptr;
}

void NewFrame() {
    if (!g_ctx) return;
    ImGui::SetCurrentContext(g_ctx);
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();

    /* imgui 1.80's impl_win32 skips the GetCursorPos update unless the game
     * window is the foreground window. Under Wine/CrossOver the foreground
     * check can fail, leaving io.MousePos at (-FLT_MAX,-FLT_MAX) which
     * breaks drags. Set it ourselves, unconditionally. */
    POINT pt;
    if (g_hwnd && GetCursorPos(&pt) && ScreenToClient(g_hwnd, &pt))
        ImGui::GetIO().MousePos = ImVec2((float)pt.x, (float)pt.y);

    ImGui::NewFrame();
}

void EndFrameAndRender() {
    if (!g_ctx) return;
    ImGui::SetCurrentContext(g_ctx);
    ImGui::Render();
    auto* dd = ImGui::GetDrawData();

    /* arcdps doesn't bind the backbuffer RTV before calling our imgui cb,
     * so we bind it ourselves, render, and restore. */
    ComPtr<ID3D11RenderTargetView> prev_rtv;
    ComPtr<ID3D11DepthStencilView> prev_dsv;
    g_immediate->OMGetRenderTargets(1, &prev_rtv, &prev_dsv);

    ComPtr<ID3D11Texture2D>        back;
    ComPtr<ID3D11RenderTargetView> rtv;
    if (g_swap && SUCCEEDED(g_swap->GetBuffer(0, __uuidof(ID3D11Texture2D), &back))
               && SUCCEEDED(g_device->CreateRenderTargetView(back.Get(), nullptr, &rtv))) {
        ID3D11RenderTargetView* bind = rtv.Get();
        g_immediate->OMSetRenderTargets(1, &bind, nullptr);
    }

    ImGui_ImplDX11_RenderDrawData(dd);

    /* Restore whatever arcdps had bound (typically nothing). */
    ID3D11RenderTargetView* restore = prev_rtv.Get();
    g_immediate->OMSetRenderTargets(1, &restore, prev_dsv.Get());
}

/* Hand-rolled input routing — we deliberately avoid imgui's upstream
 * WndProcHandler because it calls ::SetCapture/::ReleaseCapture on mouse
 * events, which clobbers the game's own capture (GW2's right-click-to-
 * rotate-camera relies on persistent Win32 capture to keep tracking mouse
 * movement once the cursor leaves the window). */
void WndProc(HWND, UINT msg, WPARAM wp, LPARAM lp) {
    if (!g_ctx) return;
    ImGui::SetCurrentContext(g_ctx);
    auto& io = ImGui::GetIO();

    switch (msg) {
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: io.MouseDown[0] = true;  break;
    case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK: io.MouseDown[1] = true;  break;
    case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK: io.MouseDown[2] = true;  break;
    case WM_LBUTTONUP: io.MouseDown[0] = false; break;
    case WM_RBUTTONUP: io.MouseDown[1] = false; break;
    case WM_MBUTTONUP: io.MouseDown[2] = false; break;

    case WM_MOUSEWHEEL:
        io.MouseWheel  += (float)GET_WHEEL_DELTA_WPARAM(wp) / (float)WHEEL_DELTA;
        break;
    case WM_MOUSEHWHEEL:
        io.MouseWheelH += (float)GET_WHEEL_DELTA_WPARAM(wp) / (float)WHEEL_DELTA;
        break;

    case WM_KEYDOWN: case WM_SYSKEYDOWN:
        if (wp < 256) io.KeysDown[wp] = true;
        break;
    case WM_KEYUP: case WM_SYSKEYUP:
        if (wp < 256) io.KeysDown[wp] = false;
        break;
    case WM_CHAR:
        if (wp > 0 && wp < 0x10000) io.AddInputCharacterUTF16((unsigned short)wp);
        break;
    default: break;
    }
    (void)lp;
}

void* Context() { return g_ctx; }

}
