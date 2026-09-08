# Gazer

Gaze-driven AAC and system input for Windows. C++20 / Qt 6.

Contributors and coding agents: start at [AGENTS.md](AGENTS.md). Page XML schema: [docs/page-xml.md](docs/page-xml.md).

Gazer turns live gaze (Tobii Eye Tracker 5, or the mouse as a fallback) into on-screen pages you dwell to activate. Pages can send keys, click, move the pointer, speak, and run assist tools (look-to-scroll, magnifier, gaze reticle). The long-term aim is one stack for accessible gaming in place of OptiKey + OpenTrack + UCR + AutoHotkey.

Version **0.5.3**. License: [GPL-3.0](LICENSE). Cell and zone icons are SVG files in `resources/icons/svg/`. Set `icon` to the filename stem (`menu`, `mouseLeftClick`, `keyTab`). Matching is case-insensitive; a trailing `Icon` is ignored. Unknown names fall back to the label. Most glyphs are [Material Symbols Rounded](https://fonts.google.com/icons?icon.set=Material+Symbols&icon.style=Rounded); see [resources/icons/README.md](resources/icons/README.md).

## What you get at launch

`resources/layouts/main.xml` is the process-lifetime root. It never closes. One host window is sized to visible chrome (not the whole desktop). Grids and Zones are regions on that surface, not extra HWNDs.

| Piece | XML | Role |
|-------|-----|------|
| Root page | `main` (`master="true"`) | Dock **Main** and **Sleep** zones, plus drawer and quit grids |
| Drawer grid | `drawer` | Keyboard, Mouse, Assist, Right, Settings, Editor, Close, Close All, Pause dwell, Quit |
| Quit grid | `quit` | Yes exits; No returns to the drawer |

Drawer (layer 2) and quit (layer 3) start hidden; the page opens on layer 1. The Main chip `ShowLayers`s `1,2`; Dismiss returns to `1`; Quit switches to `1,3`. Opening Keyboard / Mouse / Assist / Settings / Right attaches that page on the same host, then the drawer auto-collapses.

- **Main** is shown only while no master grid is up (`visibleWhen="!expanded"`). Dwell it to grow the drawer from the bottom.
- **Sleep** stays available while the drawer is open. Shell zones and grids paint and hit above other boards.
- Gaze on the drawer or the dock chips counts as using the shell, so the drawer idle timer does not fire while you look at Sleep.
- The host window stays above the Windows taskbar.

The tray owns process lifetime. Closing a page does not quit the app.

## Page editor

Tray → **Page editor**, command `openPageEditor`, or launch with `--editor`.

The designer edits **Page XML** (the same files the runtime loads). Three panes:

| Pane | Contents |
|------|----------|
| Left | **Add** (button, label, toggle, tab, slider, zone, grid, subgrid, named style, named dwell) and the element tree (page → styles / dwells / zones / grids → cells / subgrids). Right-click to duplicate, delete, convert cell ↔ zone, add a subgrid, or change paint order. |
| Center | **Fit grid** (default): the selected grid fills the canvas. Uncheck it to see true placement on a 1920×1080 virtual display. Click / Shift-click to select; drag to move; accent handles resize; arrows nudge (Shift = 16 px); Delete removes. **Esc** cancels click-to-place. Zones show the progress chip and the dwell region. The toolbar **layer** combo (next to Code view) filters which grid/zone layers paint on the canvas. |
| Right | Tabs follow the selection: **Page**; **Grid / Style / Placement**; **Cell** or **Zone / Style / Placement / Action**; named **Style** or **Dwell** alone. Placement holds anchor, offset, row/col/span, and zone progress/dwell regions. Cell and zone dwell inherit/overrides sit at the bottom of Action. |

File → New asks for id, name, and a template (blank, full keyboard, keyboard row, settings row, zone chip). File → Open lists shipped and user `*.xml` pages. **Save** of a shipped file writes a user copy to `%AppData%\Gazer\layouts` and leaves `resources/` unchanged. The toolbar **layer** combo filters which grid/zone layers paint. Shift is a modifier on QWERTY boards (labels switch to the shifted glyph). **Test on canvas** (F6) plays a dwell ring. **Test on desktop** (F5) attaches the current page on the live host. Master roots cannot be live-tested. Empty actions warn before Save and F5.

## Requirements

- Windows
- CMake ≥ 3.21
- Qt 6 (Core, Gui, Widgets, Qml, Quick) — tested with 6.11.1 MinGW
- Optional: [Tobii Stream Engine](https://developer.tobii.com/) headers under `third_party/tobii/include` and `tobii_stream_engine.dll` on the machine. Without hardware, Gazer uses the mouse tracker.

## Build and run

```powershell
cmake -S . -B build -G Ninja `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64" `
  -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Gazer
.\build\Gazer.exe
```

Use one Ninja binary for this tree. Qt Tools 1.12.1 cannot read the `.ninja_log` from Visual Studio 18’s 1.13.2 (`build log version is too new; starting over`). `scripts/build-msi.ps1` prefers the 1.13 copy when it is installed.

MinGW `bin` must be on `PATH` when configuring (otherwise AUTOMOC / g++ predefs fail silently).

The post-build step copies `resources/` next to `Gazer.exe` and runs `windeployqt`. If Tobii’s DLL is installed in the usual EyeX folder, it is copied beside the exe as well.

Log: `gazer.log` in the working directory. Settings: `%AppData%\Gazer\settings.json`.

### Beta MSI

```powershell
winget install WiXToolset.WiXCLI   # once
.\scripts\build-msi.ps1            # configure, build, stage, MSI
.\scripts\build-msi.ps1 -SkipBuild # reuse existing build\Gazer.exe
```

Output: `dist\Gazer-<version>-beta.msi`. Installs to `Program Files\Gazer\` with Start Menu and desktop shortcuts. Pages ship under `resources\`. Each install deletes `%AppData%\Gazer\settings.json` (and a leftover `%AppData%\Gazer\Gazer\settings.json` from older builds); the next launch writes factory defaults. Speech secrets, clips, and user layouts are left in place.

The MSI stamps `uiAccess=true` on the staged exe and Authenticode-signs it so the **Program Files** copy can sit above Task Manager and type/click into elevated windows. `.\build\Gazer.exe` is left without UIAccess so it still launches from the build directory. A real code-signing PFX: `$env:GAZER_SIGN_PFX` and optional `$env:GAZER_SIGN_PFX_PASSWORD`. Without those, the script uses a local self-signed cert (`%LOCALAPPDATA%\Gazer\signing\`) and the MSI trusts it at install time.

Testers still need Tobii drivers for hardware gaze. Without a tracker, use the mouse backend (tray / Settings → tracker).

## Using Gazer

1. Start Gazer. Unless **start docked** is on, the drawer opens.
2. Dwell a cell until progress completes. Last dwell step repeats while gaze holds. Typing, mouse, composer, modifiers, and AHK use **daily driver dwell**; settings and navigation use **designer dwell**. Both are under Settings → Speed.
3. Open Keyboard, Mouse, Assist, or Settings as extra pages. They stay up after the drawer collapses.
4. **Close** hides the drawer. **Close All** closes other pages and then collapses.
5. **Pause dwell** / **Sleep** suspends dwell everywhere except `suspendExempt` unlock targets. A dim screen border leaves a gap at those targets.
6. Tray: show layout (raise host), show head-pose preview, quit.

When pages overlap, the topmost page’s grid is opaque: gaze and paint do not fall through to the page underneath. Shell zones (dock chips) still win over everything.

## Shipped pages

| Id | Kind |
|----|------|
| `main` | Root dock + drawer + quit |
| `main_settings` / `main_settings_*` | Settings hub, then Speed / Magnify / Indicators / Assist / Tools / Theme |
| `example_keyboard` | Compact keyboard (layers: letters, shift, symbols, symbols+shift) |
| `example_mouse` | Mouse pad |
| `example_assist` | Assist tools |
| `uw_qwerty` (+ shift) | QWERTY keyboard |
| `uw_right` | Right-hand board |

`Page` actions open, close, or toggle a **Page** (`OpenPage`, `ClosePage`, `CloseAllPages`, `CloseOtherPages`). `ShowLayers` sets which grid/zone layers are visible. Closing a page removes all of its elements with it.

## Architecture

```
Tobii / Mouse ──► ITracker ──► GazePoint (+ HeadPose)
                     │
                     ▼
                 GazeRouter
                     │
        ┌────────────┼────────────┐
        ▼            ▼
   PageSession    Assist tools
   PageHostWindow LookToScroll
   Dwell SM       Magnifier
        │
        ▼
  ActionDispatcher
  CommandRegistry ──► InputService
                  └──► ScriptHost (gazer.*)
```

- Live UI is one frameless topmost `QQuickWindow` + `QQuickPaintedItem` (software scene graph, alpha buffer) sized to painted chrome.
- Root chrome is Docked / Drawer / Quit (`PageSession`).
- Mapping profiles (`resources/mappings/default.json`) turn leftover command names into key / mouse / gamepad output.

Boards are XML only (`resources/layouts/*.xml`). `gazer.openPage` / `loadPage` open those pages on the live host. Editor F5 previews attach XML copies under `__editor_preview_*` ids so they do not replace the live page.

## Page XML

Boards are XML in `resources/layouts/*.xml` (catalog id = filename stem). Schema, runtime rules, and action names: [docs/page-xml.md](docs/page-xml.md).

## Commands

Builtins first; unknown names fall through to the mapping profile. Full catalog (settings patterns, mapping-only names): [src/app/Commands.md](src/app/Commands.md).

| Command | Role |
|---------|------|
| `quitApp` | Exit |
| `toggleDwellSuspend` / `suspendDwell` / `resumeDwell` | Global dwell pause |
| `toggleMagnifier` | Lens (exclusive with gaze reticle) |
| `toggleLookToScroll` | Gaze-driven scroll |
| `toggleGazeReticle` | Gaze marker (exclusive with magnifier) |
| `toggleGazeMouseFollow` | Cursor follows gaze |
| `mouseMoveToGaze` | Dwell to place the cursor |
| `mouseMoveToGazeClickLoop` | Sticky dwell-move then click |
| `mouseLeftClick` / `mouseRightClick` / `mouseMiddleClick` | Click at cursor |
| `mouseLeftClickAtGaze` / `mouseRightClickAtGaze` / `mouseMiddleClickAtGaze` | Dwell-move then click |
| `stopAllActionLoops` | Stop sticky series and assist loops |
| `openPreview` | Head-pose preview |
| `openPageEditor` | XML page designer |
| `theme.light` / `.dark` | Light or dark surfaces |
| `theme.brightness.0`…`.4` | Background shade |
| `theme.tint.none` / `.primary` / `.complementary` / `.analogous1` / `.analogous2` / `.tertiary1` / `.tertiary2` | Surface tint hue |
| `theme.primary.0`…`.8` | Accent (Apple system: Red … Pink) |
| `theme.secondary.0`…`.8` | Progress (same 9) |
| `settings.*` | Settings hub editors, nudges, presets |
