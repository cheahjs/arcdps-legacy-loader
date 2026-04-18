# arcdps-legacy-loader

An arcdps extension that keeps old ImGui 1.80 arcdps addons working after
arcdps updates to ImGui 1.92.7+.

## What it does

arcdps rejects addons whose reported `IMGUI_VERSION_NUM` does not match its
own. When arcdps updates to 1.92.7, every addon still compiled against 1.80
stops loading until its author ships a rebuild.

This loader registers as a 1.92.7 arcdps extension and maintains its own
private ImGui 1.80 context. Legacy addons are loaded against the 1.80
context and driven via the same arcdps callback shape (`imgui`, `combat`,
`combat_local`, `wnd_filter`, `wnd_nofilter`). arcdps's own HMODULE is
forwarded so addons' `GetProcAddress("e3")` etc. resolve directly - those
exports are ImGui-independent, so no shimming is needed.

A small options window (`Shift+Alt+L` by default) exposes each legacy
addon's `options_tab` and `options_windows(nullptr)` callbacks, since
arcdps's own options UI runs against the 1.92.7 context and cannot invoke
them.

## Installation

1. Drop `arcdps_legacy_loader.dll` into `<gw2-root>/addons/arcdps/`.
2. Put your legacy ImGui 1.80 addon DLLs into
   `<gw2-root>/addons/arcdps/legacy/`.
3. Launch. The loader appears in arcdps's extension list and loads the
   legacy addons behind it.

Settings are written to `<gw2-root>/addons/arcdps/legacy/loader.ini` and
ImGui window state to `imgui.ini` next to it. Failure diagnostics go to
`<gw2-root>/addons/arcdps/arcdps_legacy_loader.log` and arcdps's log window.

## Building

### Cross-compile from macOS / Linux

Requires `mingw-w64` (`brew install mingw-w64` / `apt install g++-mingw-w64`)
and CMake 3.21+.

```
cmake --preset mingw-x64
cmake --build --preset mingw-x64
```

Output: `build/mingw-x64/arcdps_legacy_loader.dll` (self-contained,
static-linked mingw runtime).

### Native Windows (MSVC)

```
cmake --preset msvc-x64
cmake --build --preset msvc-x64
```

Output: `build/msvc-x64/RelWithDebInfo/arcdps_legacy_loader.dll`.

## Known limitations

- The reported `imguivers` is hard-coded to `19270` in `src/exports.cpp`.
  Bump it if arcdps moves to a newer ImGui.
- `arcdps_exports::sig` is hard-coded (`0x7A11EDAD`). arcdps de-duplicates
  addons by this value, so another addon picking the same constant would
  collide. If you fork the loader, pick your own.
- `options_windows(name)` overrides against arcdps's built-in window list
  are not wired up - the loader does not know arcdps's window registry.
  Legacy addons' own checkboxes drawn from `options_windows(nullptr)` work
  inside the options window.
- Input routing uses a hand-rolled WndProc shim instead of ImGui's upstream
  `ImGui_ImplWin32_WndProcHandler` so that `SetCapture` /`ReleaseCapture`
  inside ImGui do not clobber the game's own Win32 mouse capture (GW2's
  right-click-to-rotate-camera relies on it).
