# arcdps-legacy-loader

An arcdps extension that keeps old ImGui 1.80 arcdps addons working after arcdps updates to ImGui 1.92.7.

## What it does

This loader registers as a 1.92.7 arcdps extension and maintains its own private ImGui 1.80 context.
Legacy addons are loaded against the 1.80 context and driven via the same arcdps callback shape (`imgui`, `combat`, `combat_local`, `wnd_filter`, `wnd_nofilter`).
arcdps's own HMODULE is forwarded so addons' `GetProcAddress("e3")` etc. resolve directly - those exports are ImGui-independent, so no shimming is needed.

A small options window (`Shift+Alt+L` by default) exposes each legacy addon's `options_tab` and `options_windows(nullptr)` callbacks, since arcdps's own options UI runs against the 1.92.7 context and cannot invoke them.

## Installation

1. Download the latest release from the [releases page](https://github.com/cheahjs/arcdps-legacy-loader/releases).
1. Drop `arcdps_legacy_loader.dll` into `<gw2-root>/` next to arcdps's dll, either `d3d11.dll` / `arcdps.dll`.
1. Put your legacy ImGui 1.80 addon DLLs into `<gw2-root>/addons/arcdps/legacy/`.
1. Launch. The loader appears in arcdps's extension list and loads the
   legacy addons behind it.

> [!WARNING]
> Be aware that the loader does not attempt to stop you from loading duplicate addons across arcdps and the loader and can cause issues if you have duplicates!
> When addons have updated to a newer version of imgui, make sure to remove the old version from the legacy folder.


Settings are written to `<gw2-root>/addons/arcdps/legacy/loader.ini` and ImGui window state to `imgui.ini` next to it.
Failure diagnostics go to `<gw2-root>/addons/arcdps/arcdps_legacy_loader.log` and arcdps's log file.

By default the loader copies arcdps's live ImGuiStyle onto its private 1.80 context so legacy addons match arcdps's theme.
You can disable this to have a distinct style from the loader settings UI.

Also by default, arcdps's top-level windows are mirrored into the 1.80 context each frame as invisible shadows at matching names.
Legacy addons that anchor their own windows against arcdps windows (e.g. Boon Table's relative positioning, which resolves the anchor via `ImGui::FindWindowByID`) work transparently as a result.
This only works for legacy addons anchoring against arcdps windows, and will not work for newer addons to anchor against legacy windows.
Disable in the loader settings UI to reduce performance overhead.

## Building

### Native Windows (MSVC)

```
cmake --preset msvc-x64
cmake --build --preset msvc-x64
```

Output: `build/msvc-x64/RelWithDebInfo/arcdps_legacy_loader.dll`.

### Cross-compile from macOS / Linux

Requires `mingw-w64` (`brew install mingw-w64` / `apt install g++-mingw-w64`)
and CMake 3.21+.

```
cmake --preset mingw-x64
cmake --build --preset mingw-x64
```

Output: `build/mingw-x64/arcdps_legacy_loader.dll` (self-contained, static-linked mingw runtime).

## Known limitations

- `options_windows(name)` overrides against arcdps's built-in window list are not wired up - the loader does not know arcdps's window registry.
  Legacy addons' own checkboxes drawn from `options_windows(nullptr)` work inside the options window.
- Input routing uses a hand-rolled WndProc shim instead of ImGui's upstream `ImGui_ImplWin32_WndProcHandler` so that `SetCapture` /`ReleaseCapture` inside ImGui do not clobber the game's own Win32 mouse capture (GW2's right-click-to-rotate-camera relies on it).
- Input handling with respect to arcdps modifier settings is still lacking.
- Input handling is not a 1-to-1 match for arcdps's own input handling and may not behave the same way.
- Mouse input when multiple windows across multiple Imgui contexts are overlapping may behave weirdly.
