# arcdps-legacy-loader

An arcdps extension that loads legacy arcdps addons against an older ImGui
context after arcdps itself updates to ImGui 1.92.7+.

## Why

arcdps is about to update its ImGui version from 1.80 to 1.92.7+. Legacy
addons that are still compiled against ImGui 1.80 will fail the
`IMGUI_VERSION_NUM` check that arcdps performs on load and refuse to
initialise, bricking any addon whose author has not shipped a rebuild.

This loader registers itself as a modern arcdps extension (reporting the
new ImGui version number) and maintains its own ImGui 1.80 context. Legacy
addons are loaded in-process, handed the 1.80 context, and driven via the
same arcdps callback shape (`imgui`, `combat`, `combat_local`, `wnd_filter`,
`wnd_nofilter`). arcdps's own HMODULE is forwarded so addons can resolve
`e0`..`e10` exports directly against arcdps - those signatures are ImGui-
independent, so no shimming is required.

## How it works

- Loader is an arcdps extension built against ImGui 1.80 (statically linked).
  Reports `imguivers = 19270` so arcdps 1.92.7 accepts it.
- On `mod_init` the loader creates a private ImGui 1.80 `ImGuiContext` plus
  a DX11 backend bound to the game's swapchain.
- Legacy addons are scanned from `<game>/addons/arcdps/legacy/` and loaded
  with the 1.80 context and arcdps's real HMODULE.
- `imgui` callbacks are fanned out between an explicit `NewFrame` and
  `Render`; the backbuffer RTV is bound manually each frame since arcdps
  does not bind one before invoking our callback.
- Input routing uses a hand-rolled WndProc shim that sets `io.MouseDown` /
  `io.MousePos` / `io.KeysDown` directly. It deliberately does **not** call
  `SetCapture` / `ReleaseCapture` so the game's own Win32 mouse capture
  (used for right-click-to-rotate-camera) is not clobbered.
- A mock settings window is provided in the 1.80 context since arcdps's own
  options UI runs against the 1.92.7 context and cannot invoke legacy
  `options_tab` / `options_windows` callbacks. Toggle it with a
  configurable hotkey (default `Shift+Alt+L`).
- Settings persist to `<game>/addons/arcdps/legacy/loader.ini`. ImGui window
  positions persist to `<game>/addons/arcdps/legacy/imgui.ini`.

## Installation

1. Drop `arcdps_legacy_loader.dll` into `<gw2-root>/addons/arcdps/`.
2. Create `<gw2-root>/addons/arcdps/legacy/` and put the legacy ImGui 1.80
   addon DLLs in it.
3. Launch the game. The loader appears in arcdps's extension list; legacy
   addons are loaded behind it.

Failure diagnostics are written to `<gw2-root>/arcdps_legacy_loader.log`
and forwarded to arcdps's log window via `e3`.

## Building

### Cross-compile from macOS / Linux

Requires `mingw-w64` (`brew install mingw-w64`) and CMake 3.20+.

```
./scripts/build-and-deploy.sh
```

The script configures with `cmake/toolchain-mingw.cmake`, builds a
self-contained DLL (static libgcc / libstdc++), and deploys it into the
CrossOver bottle path hardcoded at the top of the script. Edit the path to
match your install.

### Native Windows (MSVC)

```
cmake -S . -B build
cmake --build build --config Release
```

Output: `build/Release/arcdps_legacy_loader.dll`.

## Layout

```
src/
  exports.cpp              arcdps extension entry + exports table
  dllmain.cpp              module handle capture
  proxy/arcdps_proxy.*     stashes the handoff from arcdps mod_init
  imgui_legacy/context.*   owns the 1.80 ImGuiContext + dx11/win32 backend
  addon/legacy_addon.*     one loaded legacy addon dll
  addon/addon_manager.*    scan/load and fan-out of arcdps callbacks
  ui/settings_window.*     mock options window rendered in the 1.80 context
  config/config.*          loader.ini parse/write
  logging/log.*            e3 + file log
```

## Known limitations

- The loader statically links ImGui 1.80 only. If arcdps's reported ImGui
  version changes again, bump `IMGUI_VERSION_NUM_1_92_7` in
  `src/exports.cpp`.
- `options_windows(name)` overrides against arcdps's built-in window list
  are not wired up - the loader does not know arcdps's internal window
  registry. Legacy addons' own checkboxes drawn from
  `options_windows(nullptr)` work inside the mock settings window.
- Under Wine / CrossOver, `ImGui_ImplWin32_NewFrame` will not update
  `io.MousePos` because its foreground-window check fails; the loader
  patches around this by re-reading `GetCursorPos` after `NewFrame`.
