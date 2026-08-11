# Gazer

**Gaze input mapper** for **Tobii Eye Tracker 5** — C++20 / Qt 6.

Maps live gaze into on-screen AAC layouts **and** system / virtual-controller
input. Long-term target: replace **OptiKey + OpenTrack + UCR + AutoHotkey** in
one stack.

## Phases

| Phase | Scope | Status |
|-------|--------|--------|
| **0** | Skeleton: tray, preview, tracker backends | Done |
| **1** | JSON layouts, layout window, dwell activate | Done |
| **2** | Multi-instance layouts + off-screen indicators | Done |
| **3** | Mapping profiles + keyboard/mouse/gamepad injection | Done |
| **4** | QJSEngine, TTS, Look-to-Scroll, magnification | Done |
| **5** | Action series/loops, lifecycle, geometry %, themes, head pose, curves | In progress |

## Beta MSI (Windows)

Build a machine-wide installer for testers (WiX Toolset CLI v7):

```powershell
# Prerequisites: Qt 6 MinGW, CMake, Ninja (as for normal builds)
winget install WiXToolset.WiXCLI   # once

.\scripts\build-msi.ps1            # build + stage + MSI
.\scripts\build-msi.ps1 -SkipBuild # reuse existing build\Gazer.exe
```

Output: `dist\Gazer-<version>-beta.msi` (≈25–30 MB).

| Install | Command |
|---------|---------|
| UI | double-click the MSI, or `msiexec /i dist\Gazer-0.4.0-beta.msi` |
| Quiet | `msiexec /i dist\Gazer-0.4.0-beta.msi /qn` |
| Remove | `msiexec /x dist\Gazer-0.4.0-beta.msi` |

Installs to `Program Files\Gazer\` with Start Menu + desktop shortcuts. Layouts/models ship under `resources\`. Settings still live in the user’s AppData.

**Tobii:** the MSI bundles `tobii_stream_engine.dll` when found on the build machine. Testers still need Tobii Eye Tracker drivers/runtime for hardware gaze; without hardware, use the mouse tracker.

## Architecture

```
Tobii / Mouse ──► ITracker ──► GazePoint (+ HeadPose)
                                │
                    ┌───────────┼───────────┐
                    ▼           ▼           ▼
              LayoutManager  Stickiness   Preview (head OBJ)
              + dwell/OSK    profiles
                    │
                    ▼
              ActionDispatcher / ActionLoopService
              CommandRegistry → InputService
```

---

## Layout JSON reference (complete)

Layouts live in `resources/layouts/*.json`. Parsed by `LayoutLoader` into a `LayoutDocument`.

**Runtime behaviors worth knowing:**

| Behavior | Rule |
|----------|------|
| **Overlap dwell** | When boards overlap, only the **topmost** instance (hit-stack / raised HWND) receives gaze dwell. |
| **Auto-close** | Secondaries can fade then close after idle with no dwell; master shells default off. |
| **Unbounded progress hit** | While dwelling an unbounded item, the visible edge **progress band/strip** is included in the hit rect. |
| **Dims** | Bare `width` / `x` = **percent** of reference; `widthPx` / `xPx` = pixels (**px wins** if both set). |

### Root object

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `schemaVersion` | int | no (default `1`) | Schema version |
| `id` | string | **yes** | Catalog id (filename stem should match) |
| `name` | string | no | Title on Fluent boards |
| `description` | string | no | Subtitle under title |
| `uiStyle` | string | no | `"default"`, `"fluent"`, or `"material"` (material → fluent) |
| `session` | object | no | Master-shell / dock role (see **Session**) |
| `window` | object | no | Board placement, size, chrome (see **Window**) |
| `grid` | object | no | Cell grid (see **Grid**). **Optional** — defaults if omitted |
| `boundsMode` | string | no | Layout default for % placement: `"desktop"` or `"screen"` (see **Bounds**) |
| `bounds` | string | no | Alias for `boundsMode` |
| `style` | object | no | Window chrome if `window.style` omitted (see **Chrome style**) |
| `dwell` | object | no | Layout-level dwell / progress defaults |
| `autoClose` | bool | no | Secondary: fade→close when not dwelt on. Master shells default **false** if omitted |
| `autoCloseIdleMs` | int | no | Idle ms before fade starts (`-1` / omit → AppSettings, default 8000) |
| `autoCloseFadeMs` | int | no | Fade duration ms (`-1` / omit → AppSettings, default 500) |
| `items` | array | no | Grid cells and unbounded affordances |
| `onOpen` | action[] | no | When instance is first created |
| `onLoad` | action[] | no | When document is applied (open + in-place load) |
| `onClose` | action[] | no | Before instance is closed |

### Bounds (`boundsMode`)

Controls the **reference rectangle** for percent-based size/position of windows and (when set) dwell regions.

| Value | Also accepted | Meaning |
|-------|---------------|---------|
| `"desktop"` | `"available"`, `"work"`, `"workarea"` | `QScreen::availableGeometry()` — typically excludes taskbar (**default**) |
| `"screen"` | `"full"`, `"geometry"` | `QScreen::geometry()` — full monitor |

**Precedence:** `window.boundsMode` → layout `boundsMode` → `desktop`.  
**Dwell regions:** `dwellRegion.boundsMode` → layout `boundsMode` / window → `desktop`.

### Session (`session`)

Product policy for the unique master instance (main bar / dock / quit). Not related to assist “session” modes.

| Field | Type | Description |
|-------|------|-------------|
| `role` | string | `"masterShell"` / `"master"` → unique shell; anything else → secondary (default) |
| `masterGroup` | string | Shells with the same group share one live instance (defaults to `"default"` for masters) |
| `isHome` | bool | Expanded home of the group (`raiseMaster` / restore target) |
| `collapseLayoutId` | string | Home → collapsed/dock layout id (auto-collapse, window close with secondaries) |
| `expandLayoutId` | string | Non-home shell → home layout id |
| `hideUntilGazeReveal` | bool | Dock chip hidden until bottom-edge gaze reveal completes |

### Window (`window`)

Optional board HWND. **Omit** for pure edge/unbounded shells (no chrome). Explicit `"hidden": true` forces headless.

| Field | Type | Description |
|-------|------|-------------|
| `hidden` | bool | No board chrome |
| `visible` | bool | `false` same as `hidden: true` |
| `anchor` | string | Placement anchor (case-insensitive): `default`, `topLeft`, `topCenter`, `topRight`, `center`, `leftCenter` / `centerLeft`, `rightCenter` / `centerRight`, `bottomLeft`, `bottomCenter`, `bottomRight` |
| `width` / `height` | number or `"N%"` | Size as **percent** of bounds reference |
| `widthPx` / `heightPx` | number | Size in pixels (**wins** over percent) |
| `x` / `y` | number or `"N%"` | Position as percent of bounds (from top-left of reference) |
| `xPx` / `yPx` | number | Position in pixels |
| `marginPx` | int | Inset from bounds edge when using anchors (default `8`) |
| `boundsMode` / `bounds` | string | Override reference for this window (`desktop` / `screen`) |
| `style` | object | Board panel chrome (see **Chrome style**). Style keys may also sit **inline** on `window` |

If `anchor` is `default` and `x`/`y` unset → cascaded default placement.

**Show rules:** `hidden` → never show board. Explicit `window` → show. No `window` block → show only if any **grid** item exists (unbounded-only layouts stay headless).

### Chrome style (`style` — window or item)

Colors accept `#RRGGBB` or `#AARRGGBB` (alpha `00` = fully transparent). Boards use a translucent window so transparent fills work.

| Field | Type | Aliases | Description |
|-------|------|---------|-------------|
| `background` | color | | Fill. Alpha 0 → no fill |
| `foreground` | color | | Label / icon color |
| `borderColor` | color | `border` | Outline color |
| `borderWidth` | number | `borderThickness`, `thickness` | Outline thickness (px) |
| `radius` | number | `borderRadius` | Corner radius (px) |

Unset fields fall back to theme / paint defaults (Fluent cards use slightly different defaults than `default` style).

**Item styles** may live under `"style": { … }` or (for convenience) as the same keys on the item object.

**Window styles:** prefer `"window": { "style": { … } }`. Root-level `"style"` applies to the board when `window.style` is empty.

```json
"window": {
  "anchor": "bottomCenter",
  "widthPx": 1080,
  "heightPx": 150,
  "boundsMode": "desktop",
  "style": {
    "background": "#e8181c24",
    "borderColor": "#6600d4ff",
    "borderWidth": 2,
    "radius": 18
  }
}
```

### Grid (`grid`)

**Optional.** If omitted: starts as 1×1, then expands to fit the max `row`/`col` (+ spans) of items. If provided but undersized for items, columns/rows grow to fit.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `columns` | int | `1` (then expanded) | Column count |
| `rows` | int | `1` (then expanded) | Row count |
| `gapPx` | int | `8` | Gap between cells |
| `marginPx` | int | `16` | Inner board margin |
| `unitRows` | bool | `false` | Voice OSK-style: each row sized by item `u` / `widthUnits` (also auto-on if any item has `widthUnits` > 0) |

### Dwell (`dwell` — layout or item)

Progress / timing overrides. **Priority:** item → layout → AppSettings.

| Field | Type | Description |
|-------|------|-------------|
| `enabled` | bool | Dwell active (default true) |
| `ms` | int **or** int[] | Single step or progressive sequence; **last step repeats** while gaze holds |
| `repeatMs` | int | Obsolete (ignored when sequence is used) |
| `progressStyle` | string | Comma-separated: `radial`, `fill`, `border` |
| `scanGraceMs` | int | Time on-target before dwell progress / sequence starts; `-1` = inherit AppSettings (default 100) |
| `graceMs` | int | Blink / invalid-sample grace; `-1` = inherit |
| `progressColor`, `fillColor`, `borderColor` | color | Progress paint (hex, optional alpha) |
| `flashBorderColor`, `flashFillColor` | color | Activation flash |
| `flashMs` | int | Flash duration; `-1` = inherit |

### Items (`items[]`)

| Field | Type | Description |
|-------|------|-------------|
| `id` | string | **Required** unique id |
| `label` | string | Primary text |
| `caption` | string | Secondary line (Fluent labels / default multiline) |
| `tooltip` | string | Legacy help text (prefer a non-interactive label cell) |
| `settingKey` | string | Live `AppSettings` value on value/label cells |
| `activeState` | string | Accent “on” when resolver true (e.g. `magnifier`, `dwellSuspend`, `loop.gazeClick`, `setting.*`) |
| `icon` | string | Built-in glyph key (`leftClick`, `moveTo`, …) |
| `interactive` | bool | `false` = visual only, not dwell/click hit-tested |
| `role` | string | Shorthand: `label` / `display` / `value` → non-interactive; `unbounded` / `dwellExempt` as role names also recognized |
| `dwellExempt` | bool | Still dwellable while global dwell suspend is on (auto for suspend toggle commands) |
| `row`, `col` | int | Grid cell (0-based) |
| `rowSpan`, `colSpan` | int | Span (default 1) |
| `u` / `widthUnits` | number | Relative width in unit-row mode (letter unit = 1) |
| `unbounded` | bool | Not a board grid cell; use with `dwellRegion` for edge / off-screen hits |
| `dwellRegion` | object | Custom hit geometry (see below) |
| `dwell` | object | Per-item dwell override |
| `action` | object | Single action (legacy; still supported) |
| `actions` | action[] | Ordered series (preferred when non-empty) |
| `actionLoop` / `loop` | bool | Sticky series: first activate starts loop; re-activate stops |
| `style` | object | Cell chrome (see **Chrome style**) |

Grid cells participate in board layout only if **not** `unbounded` and **not** carrying a `dwellRegion`.

#### Dwell region (`dwellRegion`)

Used for unbounded / edge affordances. When `screenAnchor` is set, the item is treated as unbounded.

| Field | Type | Description |
|-------|------|-------------|
| `screenAnchor` | string | Omit / empty = board-local. Else: `top`, `bottom`, `left`, `right`, `topLeft`, `topRight`, `bottomLeft`, `bottomRight`, `topCenter`, `bottomCenter`, `leftCenter`, `rightCenter` (case-insensitive) |
| `x`, `y` | DimSpec | Offset: **percent** of bounds reference (or board if local), or `"N%"` |
| `xPx`, `yPx` | number | Offset in pixels (**wins**) |
| `width`, `height` | DimSpec | Size as percent of reference |
| `widthPx`, `heightPx` | number | Size in pixels (**wins**) |
| `marginPx` | int | **Deprecated** outward gap when x/y unset for screen anchors |
| `boundsMode` / `bounds` | string | Reference for this region (`desktop` / `screen`) |

**Screen anchor + x/y:** offsets apply in screen space (+Y down) from the anchor origin. Prefer explicit coords over `marginPx`.

**Board-local legacy:** bare numeric `x`/`y`/`width`/`height` (without `%` string) are forced to **pixels** for back-compat.

**Hit testing:** logical rect first; after a short engage dwell, the on-screen edge band (“drift lip”) expands the hit. While progress is visible, the **progress strip / band** is also hit-tested so gaze can stay on the chrome.

### Actions

**Types:** `speak` | `typeText` | `loadLayout` | `openLayout` | `closeLayout` | `command` | `script`

| Type | Required fields | Notes |
|------|-----------------|--------|
| `speak` | `text` | TTS (and optional type if settings allow) |
| `typeText` | `text` | Inject unicode (empty text = no-op) |
| `loadLayout` | `layoutId` | Navigate / replace per session policy |
| `openLayout` | `layoutId` | Open another secondary (or master path via `openInstance`) |
| `closeLayout` | — | Close activating instance (not last / not master alone) |
| `command` | `name` | Builtin or mapping-profile command |
| `script` | `source` | JS via `gazer.*` (may be empty in stubs) |

Optional on any step: `delayMs` — delay before that step in a series.

#### Series and sticky loops

One sticky-mode policy (`ActionLoopService`):

| Kind | How | Active state |
|------|-----|--------------|
| Layout `actionLoop` | Timed re-dispatch of `actions[]` until toggled off | `activeState` or auto `loop.<itemId>` |
| Assist sticky (gaze click loop) | `mouseDwellClickLoop` — dwell move (+ mag-pick) then click, re-arm | `loop.gazeClick` |

```json
"actions": [
  { "type": "command", "name": "mouseMoveToGaze" },
  { "type": "command", "name": "mouseLeftClick", "delayMs": 80 }
],
"actionLoop": true,
"activeState": "loop.myClick"
```

- Without `actionLoop`: run series once on dwell activate.
- With `actionLoop`: sticky series; re-activate to stop. Closing the board stops its series loops.
- Gaze click loop uses dwell UX (not timed series) but shares the sticky registry and `stopAllActionLoops`.

#### Lifecycle arrays

```json
"onOpen":  [ { "type": "command", "name": "…" } ],
"onLoad":  [ … ],
"onClose": [ … ]
```

One-shot only (no loops). `onOpen`+`onLoad` on create; `onLoad` on document replace; `onClose` before destroy.

### Auto-close

| Level | Fields |
|-------|--------|
| AppSettings | `layoutAutoClose` (bool), `layoutAutoCloseIdleMs`, `layoutAutoCloseFadeMs` |
| Layout | `autoClose`, `autoCloseIdleMs`, `autoCloseFadeMs` |

- Master shells (`session.role` master) default **`autoClose: false`** when the field is omitted.
- Secondaries default **true** (subject to global setting).
- While the user dwells the board (or any item), idle resets and opacity returns to full.
- After idle: opacity fades over `autoCloseFadeMs`, then the instance closes.

### Minimal examples

**Headless edge shell (no grid, no window):**

```json
{
  "id": "edge_home",
  "items": [
    {
      "id": "open_main",
      "label": "Main",
      "unbounded": true,
      "dwellRegion": {
        "screenAnchor": "bottomCenter",
        "widthPx": 300,
        "heightPx": 120,
        "marginPx": 40,
        "boundsMode": "desktop"
      },
      "action": { "type": "command", "name": "expandMaster" }
    }
  ]
}
```

**Board with chrome and auto-close:**

```json
{
  "id": "my_board",
  "name": "Tools",
  "uiStyle": "fluent",
  "boundsMode": "desktop",
  "autoClose": true,
  "autoCloseIdleMs": 12000,
  "window": {
    "anchor": "topCenter",
    "widthPx": 800,
    "heightPx": 400,
    "style": {
      "background": "#cc20232a",
      "borderColor": "#00dcff",
      "borderWidth": 2,
      "radius": 16
    }
  },
  "grid": { "columns": 2, "rows": 1, "gapPx": 12, "marginPx": 16 },
  "items": [
    {
      "id": "a",
      "label": "Speak",
      "row": 0, "col": 0,
      "style": {
        "background": "#772d6a4f",
        "foreground": "#ffffff",
        "borderColor": "#88ffffff",
        "borderWidth": 1.5,
        "radius": 12
      },
      "action": { "type": "speak", "text": "Hello" }
    },
    {
      "id": "b",
      "label": "Close",
      "row": 0, "col": 1,
      "action": { "type": "closeLayout" }
    }
  ]
}
```

### Notable command names

| Command | Role |
|---------|------|
| `toggleDwellSuspend` / `suspendDwell` / `resumeDwell` | Global dwell pause (items auto-exempt) |
| `expandMaster` | Navigate master to home shell |
| `closeOtherViews` | Close all non-master boards |
| `toggleMagnifier`, `toggleLookToScroll`, `toggleGazeReticle`, `toggleGazeMouseFollow` | Assist tools |
| `mouseDwellMove`, mouse click/move/scroll builtins | Mouse assist |
| `mouseMoveToGaze` | Move cursor to last valid gaze |
| `mouseDwellClickLoop`, `stopAllActionLoops` | Sticky loops |
| `openPreview` | Head pose OBJ preview |
| `theme.light` / `theme.dark` / `theme.custom` | Theme mode |
| `quitApp` | Exit application |
| `settings.*` | Settings hub editors / nudges |

### Mapping profile (`resources/mappings/*.json`)

Command name → ordered **input** outputs: `keyTap`, `keyCombo`, `text`, `mouseClick`, `mouseMove`, `mouseMoveTo`, `mouseScroll`, `gamepadButton`, `gamepadAxis`. Orthogonal to layout action series.

### Stickiness

Magnifier, gaze reticle, and gaze→mouse share `GazeFollowStickiness` profiles
driven by settings `magFollowProfile` (sticky / balanced / snappy).

### Themes

`themeMode`: `light` | `dark` | `custom` with Voice-aligned tokens (`bgMain`, `accent`, `text`, …). Color picker uses Voice sample palette. Layout `style` colors override cell/window chrome per layout.

### Head pose

Tobii head-pose stream (optional soft-bind). Preview shows gaze + planar head + raw yaw/pitch/roll/x/y/z (OpenTrack units).

### Dwell suspend UI

When dwell is suspended: semi-transparent border around the screen with a **break** at unpause targets (Pause dwell / `dwellExempt` unpause cells, including unbounded edge bands).

---

## Build & run

See prior docs: CMake ≥ 3.21, Qt 6, optional Tobii Stream Engine.

```powershell
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64" -DCMAKE_BUILD_TYPE=Release
cmake --build build
.\build\Gazer.exe
```

## License

TBD.
