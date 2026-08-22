# Gazer

Gaze-driven AAC and system input for Windows. C++20 / Qt 6.

Gazer turns live gaze (Tobii Eye Tracker 5, or the mouse as a fallback) into on-screen boards you dwell to activate. Boards can speak, type, drive the mouse, and run assist tools (look-to-scroll, magnifier, gaze reticle). The long-term aim is one stack in place of OptiKey + OpenTrack + UCR + AutoHotkey.

Version **0.4.0**. License: [MIT](LICENSE). Board icons are OptiKey geometries in `resources/icons/key_symbols.json` (GPL-3.0; see `third_party/optikey/`). Set a layout item’s `icon` to a name such as `Tab`, `MouseLeftClick`, `MinimizeDown`. Unknown names fall back to the item’s label.

## What you get at launch

`main_master` is the process-lifetime root. It never closes and never swaps documents.

| Piece | Layout | Role |
|-------|--------|------|
| Root dock | `main_master` | Headless. Unbounded **Main** and **Sleep** chips at the bottom of the screen. |
| Home drawer | `main_drawer` | Keyboard, Mouse, Assist, Settings, Close, Close All, Pause dwell, Quit. |
| Quit confirm | `main_quit_confirm` | Yes exits; No returns to the drawer. |

Exclusive chrome: **Docked**, **Drawer**, or **Quit**. Only one of those is up at a time. Opening Keyboard / Mouse / Assist / Settings collapses the drawer first (same dismiss animation as Close).

- **Main** is shown only while docked. Dwell it to grow the drawer from the bottom (0 → 100%).
- **Sleep** stays available while the drawer is open. Master chips hit-test and paint above the drawer.
- Gaze on the drawer *or* the dock chips counts as using the shell, so the drawer idle timer does not fire while you look at Sleep.
- The drawer and quit boards sit above the Windows taskbar (`window.aboveTaskbar`).

The tray owns process lifetime. Closing a board does not quit the app.

## Layout editor

Tray → **Layout editor**, command `openLayoutEditor`, or launch with `--editor`.

Three-pane designer (follows the app light/dark theme):

| Pane | Contents |
|------|----------|
| Left | **Add** (button, label, toggle, tab, slider, free item) and the **element tree** (board → cells / free items). Right-click an item to duplicate, delete, convert cell ↔ free, or change paint order. |
| Center | **Fit board** (default): the board fills the canvas (aspect preserved); gaps, radii, and type scale with the view. Uncheck it to see true placement on the primary display, including free items outside the display. Click / Shift-click to select; drag to move; drag the accent handles to resize (cells and free items); arrows nudge (Shift = 16 px); Delete removes. **Esc** cancels click-to-place. |
| Right | Board selected: Board / Window / Grid / Gaze (default item chrome, children, lifecycle). Item selected: Item / Layout / Action. Appearance and dwell timing stay inherited until you override them. |

File → New asks for id, name, and a template (blank, QWERTY with Shift/Sym/Sym+Shift layers, keyboard row, settings row, free chip). File → Open lists shipped and user layouts. **Save** of a shipped file writes a user copy to `%AppData%\Gazer\layouts` and leaves `resources/` unchanged. The toolbar layer combo appears for keyboard families (Base / Shift / Symbols / Sym+Shift). **Test on canvas** (F6) plays a dwell ring using the item’s hold times and names the full action series; **Test on desktop** (F5) opens a live preview of the current layer and registers sibling layers so Shift/Sym still swap. Master roots cannot be live-tested. Overlaps and empty actions warn before Save and F5. Layouts are the same JSON as `resources/layouts/*.json`.

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

Output: `dist\Gazer-<version>-beta.msi`. Installs to `Program Files\Gazer\` with Start Menu and desktop shortcuts. Layouts ship under `resources\`. Settings stay in the user’s AppData.

Testers still need Tobii drivers for hardware gaze. Without a tracker, use the mouse backend (tray / Settings → tracker).

## Using Gazer

1. Start Gazer. Unless **start docked** is on, the main drawer opens.
2. Dwell a cell until progress completes. Last dwell step repeats while gaze holds.
3. Open Keyboard, Mouse, Assist, or Settings as extra boards. They stay up after the drawer collapses.
4. **Close** hides the drawer. **Close All** closes other boards and then collapses.
5. **Pause dwell** / **Sleep** suspends dwell everywhere except `dwellExempt` unlock cells. A dim screen border leaves a gap at those targets.
6. Tray: show layout (expand drawer), show head-pose preview, quit.

When boards overlap, only the topmost instance receives dwell — except master dock chips, which always win over the drawer.

## Shipped layouts

| Id | Kind |
|----|------|
| `main_master` | Root dock |
| `main_drawer` | Home bar |
| `main_quit_confirm` | Quit Yes / No |
| `main_settings*` | Settings hub: button timing, pointer timing, styles, assist, LTS, theme |
| `example_keyboard` (+ shift / sym variants) | On-screen keyboard |
| `example_mouse` | Mouse pad |
| `example_assist` | Assist tools |

Secondaries are independent windows. `loadLayout` on a secondary replaces that board in place.

## Architecture

```
Tobii / Mouse ──► ITracker ──► GazePoint (+ HeadPose)
                     │
                     ▼
              LayoutInstanceManager
              (root + children + secondaries)
                     │
        ┌────────────┼────────────┐
        ▼            ▼            ▼
  LayoutQuickWindow  Dwell SM   EdgeBubbleOverlay
  (Qt Quick board)               (unbounded chips)
        │
        ▼
  ActionDispatcher / ActionLoopService
  CommandRegistry ──► InputService
                  └──► ScriptHost (gazer.*)
```

- Boards are frameless topmost `QQuickWindow` + `QQuickPaintedItem` (software scene graph, alpha buffer).
- Root chrome is one setter: Docked / Drawer / Quit (`src/layout/RootChrome.cpp`).
- Mapping profiles (`resources/mappings/default.json`) turn leftover command names into key / mouse / gamepad output.

## Layout JSON

Layouts live in `resources/layouts/*.json`. Catalog id should match the filename stem. Parsed by `LayoutLoader` into `LayoutDocument`.

### Runtime rules

| Rule | Behavior |
|------|----------|
| Dims | Bare `width` / `x` = percent of the bounds reference. `widthPx` / `xPx` = pixels. Pixels win if both are set. |
| Overlap | Topmost board gets dwell, except `main_master` unbounded items (always in front of the drawer). |
| Auto-close | After idle: opacity snaps to 50% for `autoCloseFadeMs`, then a 500 ms suck to bottom-center, then close. The root never auto-closes. The drawer collapses instead of destroying itself. |
| Unbounded hit | Logical rect first; after a short engage, the on-screen edge band (“drift lip”) expands. While progress is visible, the progress strip is also hittable. |

### Root object

| Field | Type | Description |
|-------|------|-------------|
| `schemaVersion` | int | Default `1` |
| `id` | string | **Required** catalog id |
| `name`, `description` | string | Title / subtitle painted in the top grid margin when `grid.marginPx` is at least 42 |
| `master` | bool | Process-lifetime root. Only one. |
| `hideUntilGazeReveal` | bool | Hide dock chips until a bottom-edge reveal dwell completes |
| `children` | array | `{ "id", "layoutId", "visible", "visibleWhen" }` — owned instances, show/hide |
| `window` | object | Board placement and chrome. Omit for headless / chips-only shells |
| `grid` | object | Optional. Defaults to 1×1 then grows to fit items |
| `boundsMode` / `bounds` | string | `desktop` (work area, default) or `screen` (full monitor). Aliases: `available` / `work` / `workarea`, `full` / `geometry` |
| `style` | object | Default item chrome (per-field). Overrides the theme; item `style` wins. Use `window.style` for the board panel |
| `dwell` | object | Layout-level dwell / progress defaults |
| `autoClose` | bool | Secondaries default true; master defaults false when omitted |
| `autoCloseIdleMs`, `autoCloseFadeMs` | int | `-1` / omit → AppSettings (10000 / 3000) |
| `items` | array | Cells and free (screen-anchored) items |
| `onOpen`, `onLoad`, `onClose` | action[] | Lifecycle. One-shot, no loops |
| `session` | object | Legacy. Only `role` / `hideUntilGazeReveal` seed root `master` / `hideUntilGazeReveal` when those keys are omitted |

**Bounds precedence:** `window.boundsMode` → layout `boundsMode` → `desktop`. Unbounded item `boundsMode` → layout / window → `desktop`.

### Children and visibility

`main_master` declares:

```json
"children": [
  { "id": "home", "layoutId": "main_drawer", "visibleWhen": "expanded" },
  { "id": "quit", "layoutId": "main_quit_confirm", "visibleWhen": "quitConfirm" }
]
```

Slot ids `home` and `quit` drive exclusive chrome. `visibleWhen` on items is a tiny predicate, **not** JavaScript:

| Expression | Meaning |
|------------|---------|
| omitted / empty | Show |
| `ident` | Show when that root property is true |
| `!ident` | Show when it is false |

Known properties: `expanded`, `quitConfirm`, `dwellSuspend`. Unknown syntax fails open.

`expanded` is true while the drawer or quit is up, and while the drawer is still dismissing (so the Main chip cannot reopen mid-animation).

Item `visible` (bool, default true) is ANDed with `visibleWhen`.

### Window

Omit `window` for a headless shell (no board HWND). `"hidden": true` (or `"visible": false`) forces that.

| Field | Description |
|-------|-------------|
| `anchor` | `default`, `topLeft`, `topCenter`, `topRight`, `center`, `leftCenter` / `centerLeft`, `rightCenter` / `centerRight`, `bottomLeft`, `bottomCenter`, `bottomRight` |
| `width` / `height`, `widthPx` / `heightPx` | Size |
| `x` / `y`, `xPx` / `yPx` | Position from the top-left of the bounds reference |
| `marginPx` | Inset from the bounds edge when using anchors |
| `boundsMode` | Override reference for this window |
| `style` | Panel chrome. Style keys may also sit inline on `window` |
| `aboveTaskbar` | Keep this board in the topmost band above the taskbar while visible |
| `drawerMotion` | Bottom-anchored scale 0 → 1 on show, 1 → 0 on hide |

Show rules: `hidden` → never. Explicit `window` → show. No `window` block → show only if any grid cell exists.

### Chrome style (window or item)

Colors: `#RRGGBB` or `#AARRGGBB` (alpha `00` = transparent).

| Field | Aliases | Description |
|-------|---------|-------------|
| `background` | | Fill |
| `foreground` | | Label / icon |
| `borderColor` | `border` | Outline |
| `borderWidth` | `borderThickness`, `thickness` | Outline px |
| `radius` | `borderRadius` | Corner radius px |
| `blur` | `blurRadius`, `glass` | Frosted glass: blur the desktop behind this fill (px). `true` → 15. `0` / `false` turns it off. `background` is painted as-authored on top of the frost — use `#AARRGGBB` for a tint; opaque fills hide the blur |

Unset fields fall back to layout `style`, then the theme. Item styles may live under `"style"` or as the same keys on the item. A layout-level `"style": { "blur": 15 }` frosts every cell; `window.style.blur` frosts the board panel.

### Grid

| Field | Default | Description |
|-------|---------|-------------|
| `columns`, `rows` | `1` (then expanded) | Grown to fit max row/col + spans |
| `gapPx` | `8` | Gap between cells |
| `marginPx` | `0` | Uniform inner inset in px when `marginX` / `marginY` are unset |
| `marginX` / `marginY` | unset | Horizontal / vertical inset. Bare number or `"N%"` = percent of board size; `marginXPx` / `marginYPx` = pixels |
| `unitRows` | `false` | Size each row by item `u` / `widthUnits` (auto-on if any item has `widthUnits` > 0) |

### Dwell (layout or item)

Priority: item override → layout → AppSettings → built-in default. A layout `ms` / `scanGraceMs` / `graceMs` applies to every key that does not set that field.

| Field | Description |
|-------|-------------|
| `enabled` | Default true |
| `ms` | Single step (int) or progressive sequence (int[]). Last step repeats while gaze holds |
| `progressStyle` | Comma-separated: `radial`, `fill`, `border` |
| `scanGraceMs` | Time on-target before progress starts. `-1` = inherit (default 100) |
| `graceMs` | Blink / invalid-sample grace. `-1` = inherit |
| `progressColor`, `fillColor`, `borderColor` | Progress paint |
| `flashColor`, `flashMs` | Activation flash. Default is the item foreground at 60% opacity (`flashUseForeground`). A layout `flashColor` overrides that. `flashBorderColor` / `flashFillColor` still load as `flashColor` |

### Items

| Field | Description |
|-------|-------------|
| `id` | **Required** |
| `label`, `caption` | Fluent label title + caption (muted second line) |
| `settingKey` | Live `AppSettings` value on label cells |
| `activeState` | Accent on when the resolver is true (`dwellSuspend`, `magnifier`, `loop.gazeClick`, `setting.*`, …). A leading `!` (whitespace-trimmed after `!`) negates the rest of the key |
| `icon` | Built-in glyph (`leftClick`, `moveTo`, …) |
| `interactive` | `false` = visual only |
| `role` | Paint kind: omit/`button`, `label`, `tab`, `toggle`, `slider`, `preview`. `cluster` starting with `stepper.` / `segment.` wins. `card` is ignored. Placement is `screenAnchor`, not role. |
| `textStyle` | Label type ramp: `caption`, `body`, `bodyStrong` (default), `subtitle`, `title`, `section` (overline + rule). `settingKey` with no caption paints as an accent readout |
| `cluster` | `stepper.*` NumberBox or `segment.*` pill. Members share one chrome |
| `clusterSlot` | `dec` / `value` / `inc` / `edit` for steppers; omitted segments order by `col`. `value` is non-interactive unless JSON sets `interactive` |
| `dwellExempt` | Still dwellable while global dwell is suspended (auto for suspend-toggle commands) |
| `row`, `col`, `rowSpan`, `colSpan` | Cell |
| `u` / `widthUnits` | Relative width in unit-row mode |
| `screenAnchor` | Omit / empty = **cell** (`row`/`col`). Else a free item at that anchor point (`top`, `bottomCenter`, …) |
| `x`/`xPx`, `y`/`yPx`, `width`/`widthPx`, `height`/`heightPx` | Hit geometry when `screenAnchor` is set (also accepted nested under legacy `dwellRegion`) |
| `dwell` | Per-item override (omit to inherit the board, then AppSettings) |
| `action` | Single action (legacy) |
| `actions` | Ordered series (preferred when non-empty) |
| `actionLoop` / `loop` | Sticky series; re-activate to stop |
| `style` | Cell chrome |
| `visible`, `visibleWhen` | See visibility |

Cells: `screenAnchor` omitted. Free items: `screenAnchor` set to an anchor point.

#### Free items

`screenAnchor` is the layout switch (legacy: `unbounded: true` or `role: "unbounded"`, which load as `bottomCenter` if no anchor is set). Geometry is on the item:

| Field | Description |
|-------|-------------|
| `screenAnchor` | Empty = **cell**. Else `top`, `bottom`, `left`, `right`, `topLeft`, `topRight`, `bottomLeft`, `bottomRight`, `topCenter`, `bottomCenter`, `leftCenter`, `rightCenter` |
| `x` / `y`, `xPx` / `yPx` | Offset from the anchor (screen space, +Y down) or board origin |
| `width` / `height`, `widthPx` / `heightPx` | Size |
| `marginPx` | Deprecated outward gap when x/y unset |
| `boundsMode` | Reference for this region |

### Actions

Types: `speak` | `typeText` | `loadLayout` | `openLayout` | `closeLayout` | `command` | `script`

| Type | Fields | Notes |
|------|--------|-------|
| `speak` | `text` | TTS; also types if settings allow |
| `typeText` | `text` | Unicode inject |
| `loadLayout` | `layoutId` | Root → Docked. Declared child → that chrome. Else replace this secondary |
| `openLayout` | `layoutId` | Open a secondary (or show a declared child) |
| `closeLayout` | — | Close the activating instance (not the root) |
| `command` | `name` | Builtin or mapping-profile command |
| `script` | `source` | JS via `gazer.*` |

Optional on any step: `delayMs`.

**Series:** without `actionLoop`, run once on activate. With `actionLoop`, sticky until toggled off or the board closes. Gaze click loop (`mouseDwellClickLoop`) is a separate sticky assist mode that shares the same registry (`stopAllActionLoops`).

**Lifecycle:** `onOpen` + `onLoad` on create; `onLoad` on in-place document replace; `onClose` before destroy.

### Example

```json
{
  "id": "tools",
  "name": "Tools",
  "boundsMode": "desktop",
  "autoClose": true,
  "window": {
    "anchor": "topCenter",
    "widthPx": 800,
    "heightPx": 400,
    "aboveTaskbar": true,
    "style": {
      "background": "#3320232a",
      "borderColor": "#00dcff",
      "borderWidth": 2,
      "radius": 16,
      "blur": 15
    }
  },
  "grid": { "columns": 2, "rows": 1, "gapPx": 12, "marginPx": 16 },
  "items": [
    {
      "id": "hello",
      "label": "Speak",
      "row": 0, "col": 0,
      "action": { "type": "speak", "text": "Hello" }
    },
    {
      "id": "close",
      "label": "Close",
      "row": 0, "col": 1,
      "action": { "type": "closeLayout" }
    }
  ]
}
```

## Commands

Builtins first; unknown names fall through to the mapping profile.

| Command | Role |
|---------|------|
| `expandMaster` / `collapseMaster` | Show / hide the drawer |
| `closeOtherViews` | Close every non-root, non-child board |
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
| `toggleMouseMoveMagPick` | Mag-pick refine for move-to |
| `toggleMouseMoveMagPickCenter` | Mag-pick centered on dwell vs screen |
| `openPreview` | Head-pose preview |
| `openLayoutEditor` | Fluent layout designer |
| `theme.light` / `theme.dark` / `theme.custom` | Theme mode |
| `settings.*` | Settings hub editors, nudges, presets |

Mouse pad also registers move / scroll / hold helpers (`mouseMoveUp`, `mouseLeftDownUp`, `cycleMouseMoveAmount`, …). Mapping profile adds keys such as `backspace`, `enter`, `scrollUp`, `gamepadA`.

### Scripts (`gazer.*`)

Layout `script` actions run in QJSEngine with a global `gazer` object:

`log`, `speak`, `typeText`, `runCommand`, `openLayout`, `loadLayout`, `focusedLayoutId`.

Not a sandbox.

## Mapping profiles

`resources/mappings/default.json` maps command names to ordered `InputService` steps:

`keyTap`, `keyCombo`, `text`, `mouseClick`, `mouseDoubleClick`, `mouseDown`, `mouseUp`, `mouseMove`, `mouseMoveTo`, `mouseScroll`, `mouseScrollH`, `gamepadButton`, `gamepadAxis`.

Orthogonal to layout action series.

## Settings

Persisted at `%AppData%\Gazer\settings.json`. The in-app Settings boards edit the same file.

| Area | What |
|------|------|
| Timing | Dwell sequence, scan/blink grace, mouse-move dwell, mag-pick |
| Progress | Radial / fill / border colors for boards and mouse-move |
| Magnifier | Zoom, lens size, follow profile (sticky / balanced / snappy) |
| Look-to-scroll | Deadzone, falloff, rate, accel, indicator (fan / orb / pause-only), place-cursor-first |
| Session | Auto-collapse drawer when opening a secondary, start docked, auto-close timing, tracker pref (auto / mouse) |
| Speech | Speak also types |
| Theme | Light / dark / custom |

## License

[MIT](LICENSE). Copyright © 2026 Adam Roden.
