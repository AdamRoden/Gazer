# Gazer

Gaze-driven AAC and system input for Windows. C++20 / Qt 6.

Gazer turns live gaze (Tobii Eye Tracker 5, or the mouse as a fallback) into on-screen pages you dwell to activate. Pages can send keys, click, move the pointer, speak, and run assist tools (look-to-scroll, magnifier, gaze reticle). The long-term aim is one stack in place of OptiKey + OpenTrack + UCR + AutoHotkey.

Version **0.4.0**. License: [MIT](LICENSE). Cell and zone icons are OptiKey geometries in `resources/icons/key_symbols.json` (GPL-3.0; see `third_party/optikey/`). Set `icon` to a name such as `Tab`, `MouseLeftClick`, `MinimizeDown`. Unknown names fall back to the label.

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

File → New asks for id, name, and a template (blank, QWERTY + Shift, keyboard row, settings row, zone chip). File → Open lists shipped and user `*.xml` pages. **Save** of a shipped file writes a user copy to `%AppData%\Gazer\layouts` and leaves `resources/` unchanged. The toolbar layer combo appears for keyboard families (`id`, `id_shift`, `id_sym`, `id_sym_shift`). **Test on canvas** (F6) plays a dwell ring. **Test on desktop** (F5) attaches the current page (and sibling layers) on the live host. Master roots cannot be live-tested. Empty actions warn before Save and F5.

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

Pages live in `resources/layouts/*.xml`. Catalog id should match the filename stem. Parsed by `PageLoader` into `PageDocument`.

### Runtime rules

| Rule | Behavior |
|------|----------|
| Dims | Integer token = pixels (`150`). Token with `.` or `/` = proportion of the bounds (`0.5`, `1/2`). Arithmetic with `A_ScreenWidth` / `A_ScreenHeight` is pixels (`A_ScreenHeight/9*16`), evaluated against the placement surface passed at resolve time (work area when `desktopMode`). |
| Style / dwell | Page inherits from settings, then overrides per field. Grids, cells, and zones inherit from the **page** (never from a grid). Named `style` / `dwell` plus inline attrs override individual members. |
| Overlap | Topmost attached page’s grid is opaque. Shell grids/zones paint and hit above the rest. |
| Auto-close | Idle on an `autoClose` grid collapses the drawer (root never destroys itself). |
| Zones | Chrome is hidden until dwell progress or activation flash. Engaged dwell includes the on-screen progress strip. |

### `<Page>`

| Attribute | Description |
|-----------|-------------|
| `id` | **Required** catalog id |
| `name` | Title |
| `master` | Process-lifetime root. Only one. |
| `autoClose`, `autoCloseIdleMs` | Page-level idle close |
| chrome / dwell attrs | Override settings per field (`background`, `scanGrace`, `activation`, …). Grids, cells, and zones inherit these. |

Child elements: `<Style>`, `<Dwell>`, `<Zone>`, `<Grid>`.

### `<Style>`

Named or anonymous chrome. An unnamed `<Style>` (no `id`) sets the page default. Grids, cells, and zones reference a named style with `style="id"` and may override the same attributes inline. They inherit from the page, never from a parent grid.

| Attribute | Description |
|-----------|-------------|
| `id` | Omit to set the page default style |
| `background`, `foreground`, `border` | Colors (`#RRGGBB` or `#AARRGGBB`) |
| `thickness` | Border widths: one value, or `t,r,b,l` |
| `radius` | Corner radii: one value, or `tl,tr,br,bl` |
| `progressStyle` | How dwell progress is drawn. Comma-separated: `radial`, `border`, `fill` (center), `fillup`, `filldown`, `fillleft`, `fillright` |
| `progressColor` | Dwell-progress accent (`#RRGGBB` or `#AARRGGBB`). Empty inherits settings. |
| `blur` | Frosted-glass blur radius |

### `<Dwell>`

Named or anonymous timing. An unnamed `<Dwell>` (no `id`) sets the page default (`scanGrace`, `dwellGrace`, `activation`). Grids, cells, and zones inherit from the page, never from a parent grid. They may reference a named dwell with `dwell="id"` and override individual members inline.

`visibleWhen` on cells and zones is a tiny predicate, **not** JavaScript: omitted = show; `ident` = show when that property is true; `!ident` = show when false. Known properties: `expanded`, `dwellSuspend`.

### `<Grid>` / `<SubGrid>` / `<Cell>`

A Grid is a placed rectangle of rows and columns. `desktopMode="true"` uses the virtual desktop as the bounds reference; otherwise the current screen.

| Attribute | Description |
|-----------|-------------|
| `anchor` | `TopLeft`, `Top`, `Center`, `Bottom`, … |
| `offset`, `size` | `x,y` dim pairs: pixels, axis proportion (`0.25`), height proportion (`0.25h`), or screen expressions (`A_ScreenHeight/9*16, A_ScreenHeight`) |
| `rows`, `columns`, `gap`, `margin` | Cell mesh |
| `rowWeights` | Relative row heights (`1,2,2` = header half as tall as each content row). Missing tracks are 1 |
| `drawerMotion`, `shell` | Drawer scale / always-on-top layer |
| `chrome` | `drawer` or `quit` — exclusive root-shell slot |
| `show` | `true` (default) or `false` — omit from the live session when false. Chrome-slot grids follow root chrome instead. |
| `style`, `dwell` | Named style/dwell ids, plus inline chrome/dwell attrs |

Cells use `row`, `col`, `rowSpan`, `colSpan`, `label`, `icon`, `caption`, `role` (`label`, `tab`, `slider`, `preview`, …), `show` (default true), `visibleWhen`, `interactive`, `suspendExempt`. Nested `<SubGrid>` occupies a cell span. Zones take the same `show` attribute.

### `<Zone>`

Screen-anchored chip (dock Main/Sleep, keyboard edge keys). Same leaf fields as a cell, plus `anchor` / `offset` / `size`, optional `desktopMode`, and optional `dwellOffset` / `dwellSize` for off-screen dwell.

### Actions

Action is generic: the specific thing to do is named as an attribute (on `<Action>`, or on the cell/zone when there is only one) or as a child element.

```xml
<Cell row="0" col="9" colSpan="10" label="1" send="1"/>
<Cell command="toggleLookToScroll"/>
<Cell openPage="uw_qwerty, true"/>

<Send value="a"/>
<Click value="left"/>
<Move value="gaze"/>
<Move value="gaze,0"/>
<Move value="gaze,4"/>
<Move value="up"/>
<Move value="down,40"/>
<Move value="100,200"/>
<MoveAndClick value="left"/>
<MoveAndClick value="left,4"/>
<Command value="toggleLookToScroll"/>
<OpenPage value="uw_qwerty, true"/>
<ShowGrid value="board, true"/>
<ShowZone value="more, true"/>
<ShowCell value="k_q"/>
<ClosePage value="-self"/>
<HideGrid value="-all"/>
<HideZone value="-!self"/>
<HideCell value="k_q"/>
<GoBack/>
<Speak value="Hello"/>
```

A cell or zone may have **one** action attribute. Multiple actions use child elements.

| Name | `value` |
|------|---------|
| `Send` | key[, Down\|Up[, durationMs]] |
| `Click` | left\|right\|middle |
| `Move` | `gaze` (settings zoom), `gaze,0` (no magnify), `gaze,N`; or direction (`up`/`down`/anchor)[, amount px]; or `x,y` screen coords. Amount omitted uses the mouse-assist step. |
| `MoveAndClick` | button[, zoom] — always move to gaze, then click. Zoom omitted means no magnify. |
| `Command` | builtin or mapping-profile name |
| `OpenPage` | targetId[, true] — `true` saves a breadcrumb of the current page state |
| `ShowGrid` / `ShowZone` / `ShowCell` | targetId[, true] — show a grid, zone, or cell (`openGrid` / `openZone` still load) |
| `ClosePage` | targetId[, true] — `-all`, `-self`, `-!self` (all except current) |
| `HideGrid` / `HideZone` / `HideCell` | targetId[, true] — hide a grid, zone, or cell (`closeGrid` / `closeZone` still load). `-all` hides ordinary targets of that kind (drawer/quit stay on their chrome slot). |
| `GoBack` | (none) — restore the last breadcrumb |
| `Speak` | TTS text |
| `AHK` | element body (not executed yet) |

`<Action send="a"/>` is the same as `<Send value="a"/>`. Legacy `<Action id="Send" value="a"/>` still loads.

### Example

```xml
<Page id="tools" name="Tools">
  <Grid id="board" desktopMode="true" rows="1" columns="2"
        anchor="Top" offset="0,0" size="800,400" gap="12" margin="16">
    <Cell id="hello" row="0" col="0" label="Speak" speak="Hello"/>
    <Cell id="close" row="0" col="1" label="Close" closePage="-self"/>
  </Grid>
</Page>
```

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
| `mouseLeftClick` | Left click at cursor |
| `stopAllActionLoops` | Stop sticky series and assist loops |
| `openPreview` | Head-pose preview |
| `openLayoutEditor` | XML page designer |
| `theme.light` / `theme.dark` / `theme.custom` | Theme mode |
| `settings.*` | Settings hub editors, nudges, presets |
