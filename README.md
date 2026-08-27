# Gazer

Gaze-driven AAC and system input for Windows. C++20 / Qt 6.

Contributors and coding agents: start at [AGENTS.md](AGENTS.md). Page XML schema: [docs/page-xml.md](docs/page-xml.md).

Gazer turns live gaze (Tobii Eye Tracker 5, or the mouse as a fallback) into on-screen pages you dwell to activate. Pages can send keys, click, move the pointer, speak, and run assist tools (look-to-scroll, magnifier, gaze reticle). The long-term aim is one stack in place of OptiKey + OpenTrack + UCR + AutoHotkey.

Version **0.4.0**. License: [GPL-3.0](LICENSE). Cell and zone icons are OptiKey geometries in `resources/icons/key_symbols.json` (also GPL-3.0; see `third_party/optikey/`). Set `icon` to a name such as `Tab`, `LeftClick`, `MinimizeDown`. Unknown names fall back to the label.

## What you get at launch

`resources/layouts/main.xml` is the process-lifetime root. It never closes. One host window is sized to visible chrome (not the whole desktop). Grids and Zones are regions on that surface, not extra HWNDs.

| Piece | XML | Role |
|-------|-----|------|
| Root page | `main` (`master="true"`) | Dock **Main** and **Sleep** zones, plus drawer and quit grids |
| Drawer grid | `drawer` | Keyboard, Mouse, Assist, Right, Settings, Editor, Close, Close All, Pause dwell, Quit |
| Quit grid | `quit` | Yes exits; No returns to the drawer |

Exclusive chrome: **Docked**, **Drawer**, or **Quit**. Only one of those is up at a time. Opening Keyboard / Mouse / Assist / Settings / Right attaches that page on the same host, then the drawer dismisses.

- **Main** is shown only while docked (`visibleWhen="!expanded"`). Dwell it to grow the drawer from the bottom.
- **Sleep** stays available while the drawer is open. Shell zones and grids paint and hit above other boards.
- Gaze on the drawer or the dock chips counts as using the shell, so the drawer idle timer does not fire while you look at Sleep.
- The host window stays above the Windows taskbar.

The tray owns process lifetime. Closing a page does not quit the app.

## Page editor

Tray → **Page editor**, command `openLayoutEditor`, or launch with `--editor`.

The designer edits **Page XML** (the same files the runtime loads). Three panes:

| Pane | Contents |
|------|----------|
| Left | **Add** (button, label, toggle, tab, slider, zone, grid, subgrid, named style, named dwell) and the element tree (page → styles / dwells / zones / grids → cells / subgrids). Right-click to duplicate, delete, convert cell ↔ zone, add a subgrid, or change paint order. |
| Center | **Fit grid** (default): the selected grid fills the canvas. Uncheck it to see true placement on a 1920×1080 virtual display. Click / Shift-click to select; drag to move; accent handles resize; arrows nudge (Shift = 16 px); Delete removes. **Esc** cancels click-to-place. Zones show the progress chip and the dwell region. |
| Right | Tabs follow the selection: **Page**; **Grid / Style / Placement**; **Cell** or **Zone / Style / Placement / Action**; named **Style** or **Dwell** alone. Placement holds anchor, offset, row/col/span, and zone progress/dwell regions. Cell and zone dwell inherit/overrides sit at the bottom of Action. |

File → New asks for id, name, and a template (blank, full keyboard, keyboard row, settings row, zone chip). File → Open lists shipped and user `*.xml` pages. **Save** of a shipped file writes a user copy to `%AppData%\Gazer\layouts` and leaves `resources/` unchanged. The toolbar layer combo appears for keyboard families that still ship a symbols layer (`id`, `id_sym`). Shift is a modifier on the same board (labels switch to the shifted glyph). **Test on canvas** (F6) plays a dwell ring. **Test on desktop** (F5) attaches the current page (and sibling layers) on the live host. Master roots cannot be live-tested. Empty actions warn before Save and F5.

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

Output: `dist\Gazer-<version>-beta.msi`. Installs to `Program Files\Gazer\` with Start Menu and desktop shortcuts. Pages ship under `resources\`. Settings stay in the user’s AppData.

Testers still need Tobii drivers for hardware gaze. Without a tracker, use the mouse backend (tray / Settings → tracker).

## Using Gazer

1. Start Gazer. Unless **start docked** is on, the drawer opens.
2. Dwell a cell until progress completes. Last dwell step repeats while gaze holds.
3. Open Keyboard, Mouse, Assist, or Settings as extra pages. They stay up after the drawer collapses.
4. **Close** hides the drawer. **Close All** closes other pages and then collapses.
5. **Pause dwell** / **Sleep** suspends dwell everywhere except `suspendExempt` unlock targets. A dim screen border leaves a gap at those targets.
6. Tray: show layout (raise host), show head-pose preview, quit.

When pages overlap, the topmost page’s grid is opaque: gaze and paint do not fall through to the page underneath. Shell zones (dock chips) still win over everything.

## Shipped pages

| Id | Kind |
|----|------|
| `main` | Root dock + drawer + quit |
| `main_settings_*` | Settings hub: button timing, pointer timing, styles, assist, LTS, theme |
| `example_keyboard` (+ shift / sym variants) | On-screen keyboard |
| `example_mouse` | Mouse pad |
| `example_assist` | Assist tools |
| `uw_qwerty` (+ shift) | QWERTY keyboard |
| `uw_right` | Right-hand board |

`Page` actions open/close/toggle a **Page**. Grids, zones, and cells are shown or hidden (`ShowGrid` / `HideGrid`, `ShowZone` / `HideZone`, `ShowCell` / `HideCell`). Closing a page removes all of its elements with it.

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

Boards are XML only (`resources/layouts/*.xml`). `gazer.openPage` / `loadPage` (aliases `openLayout` / `loadLayout`) open those pages on the live host. Editor F5 previews attach XML copies under `__editor_preview_*` ids so they do not replace the live page.

## Page XML

Boards are XML in `resources/layouts/*.xml` (catalog id = filename stem). Schema, runtime rules, and action names: [docs/page-xml.md](docs/page-xml.md).

## Commands

Builtins first; unknown names fall through to the mapping profile.

| Command | Role |
|---------|------|
| `closeOtherViews` | Close every attached (non-root) page |
| `quitApp` | Exit |
| `toggleDwellSuspend` / `suspendDwell` / `resumeDwell` | Global dwell pause |
| `toggleMagnifier` | Lens (exclusive with gaze reticle) |
| `toggleLookToScroll` | Gaze-driven scroll |
| `toggleGazeReticle` | Gaze marker (exclusive with magnifier) |
| `toggleGazeMouseFollow` | Cursor follows gaze |
| `mouseDwellMove` | Dwell to place the cursor |
| `mouseDwellClickLoop` | Sticky dwell-move then click |
| `mouseMoveToGaze` | Jump cursor to last valid gaze |
| `leftClick` / `rightClick` / `middleClick` | Click at cursor |
| `leftClickAtGaze` / `rightClickAtGaze` / `middleClickAtGaze` | Dwell-move then click |
| `stopAllActionLoops` | Stop sticky series and assist loops |
| `openPreview` | Head-pose preview |
| `openLayoutEditor` | XML page designer |
| `theme.light` / `theme.dark` / `theme.custom` | Theme mode |
| `settings.*` | Settings hub editors, nudges, presets |
