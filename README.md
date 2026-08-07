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

## Architecture

```
Tobii / Mouse ──► ITracker ──► GazePoint (+ HeadPose)
                                │
                    ┌───────────┼───────────┐
                    ▼           ▼           ▼
              LayoutManager  CurveMapping  Preview (gaze+head)
              + dwell/OSK    profiles
                    │
                    ▼
              ActionDispatcher / ActionLoopService
              CommandRegistry → InputService
```

---

## Layout JSON reference (complete)

Layouts live in `resources/layouts/*.json`. Loaded by `LayoutLoader`.

### Root object

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `schemaVersion` | int | no (default 1) | Schema version |
| `id` | string | **yes** | Catalog id (filename stem should match) |
| `name` | string | no | Title shown on Fluent boards |
| `description` | string | no | Subtitle |
| `uiStyle` | `"default"` \| `"fluent"` \| `"material"` | no | Board chrome style |
| `session` | object | no | Master-shell / dock role (see below) |
| `window` | object | no | Placement and size (see **Window**) |
| `grid` | object | **yes** | columns, rows, gap, margin |
| `dwell` | object | no | Layout-level dwell defaults |
| `items` | array | no | Cells and unbounded affordances |
| `onOpen` | action[] | no | Run when instance is first created |
| `onLoad` | action[] | no | Run when document is applied (open + loadInto) |
| `onClose` | action[] | no | Run before instance is closed |

### Session (`session`)

| Field | Description |
|-------|-------------|
| `role` | `"masterShell"` / `"master"` or secondary (default) |
| `masterGroup` | Shells with the same group share one live instance |
| `isHome` | Expanded home of the group |
| `collapseLayoutId` | Home → dock/collapsed id when secondaries open |
| `expandLayoutId` | Dock → home id |
| `hideUntilGazeReveal` | Dock chip hidden until bottom-edge reveal |

### Window (`window`)

| Field | Description |
|-------|-------------|
| `hidden` / `visible: false` | No board chrome (edge-only shells) |
| `anchor` | `default`, `topLeft`, `topCenter`, `topRight`, `center`, `leftCenter`, `rightCenter`, `bottomLeft`, `bottomCenter`, `bottomRight` |
| `width` / `height` | **Percent** of available desktop (0–100), or `"50%"` string |
| `widthPx` / `heightPx` | Absolute pixels (**wins** if both percent and px set) |
| `x` / `y` | Optional position as % of available desktop |
| `xPx` / `yPx` | Optional position in pixels |
| `marginPx` | Inset from screen edge when using anchors (legacy) |

### Grid (`grid`)

| Field | Description |
|-------|-------------|
| `columns`, `rows` | ≥ 1 |
| `gapPx`, `marginPx` | Spacing |
| `unitRows` | Voice OSK-style rows sized by item `u` / `widthUnits` |

### Dwell (`dwell` — layout or item)

| Field | Description |
|-------|-------------|
| `enabled` | bool |
| `ms` | int **or** array of progressive steps; last step repeats while holding |
| `repeatMs` | obsolete |
| `progressStyle` | `"radial"`, `"fill"`, `"border"` (comma-separated ok) |
| `graceMs` | Blink grace override |
| `progressColor`, `fillColor`, `borderColor` | Hex colors |
| `flashBorderColor`, `flashFillColor`, `flashMs` | Completion flash |

Priority: **item > layout > AppSettings**.

### Items (`items[]`)

| Field | Description |
|-------|-------------|
| `id` | **Required** unique id |
| `label`, `caption`, `tooltip` | Display text |
| `settingKey` | Live AppSettings value on label/value cells |
| `activeState` | Accent when resolver true (e.g. `magnifier`, `dwellSuspend`, `loop.gazeClick`) |
| `icon` | Built-in glyph key (`leftClick`, `moveTo`, …) |
| `interactive` | false = visual only (or `role`: `label` / `display` / `value`) |
| `dwellExempt` | Stays dwellable when global dwell is suspended |
| `row`, `col`, `rowSpan`, `colSpan` | Grid placement |
| `u` / `widthUnits` | Relative width in unit-row mode |
| `unbounded` | Not a grid cell; with `dwellRegion` for edge/off-screen hits |
| `dwellRegion` | Custom hit rect (see below) |
| `dwell` | Per-item dwell override object |
| `action` | Single action object (legacy; still supported) |
| `actions` | **Array** of actions run in series |
| `actionLoop` / `loop` | If true: first activate starts perpetual series; second stops (like magnifier) |
| `style.background` / `style.foreground` | Optional cell colors |

#### Dwell region (`dwellRegion`)

| Field | Description |
|-------|-------------|
| `screenAnchor` | `none`/omit = board-local; else `top`, `bottom`, `left`, `right`, corners, `*Center` |
| `x`, `y` | Position: **percent** of screen (or board if local), or `"50%"` |
| `xPx`, `yPx` | Position in pixels (wins over percent) |
| `width`, `height` | Size as percent of screen/board |
| `widthPx`, `heightPx` | Size in pixels |
| `marginPx` | **Deprecated**: outward gap when x/y unset for screen anchors |

**Screen anchor + x/y:** offsets are applied relative to the anchor origin (screen space, +Y down). Prefer explicit `x`/`y`/`xPx`/`yPx` over `marginPx`.

Board-local bare numeric `x`/`y` remain **pixels** for back-compat.

### Actions

**Types:** `speak` | `typeText` | `loadLayout` | `openLayout` | `closeLayout` | `command` | `script`

| Type | Fields |
|------|--------|
| `speak` | `text` |
| `typeText` | `text` |
| `loadLayout` / `openLayout` | `layoutId` |
| `closeLayout` | — |
| `command` | `name` (builtin or mapping profile) |
| `script` | `source` (JS via `gazer.*`) |

Optional per step: `delayMs` — delay before that step in a series.

#### Series and loops

```json
"actions": [
  { "type": "command", "name": "mouseMoveToGaze" },
  { "type": "command", "name": "mouseLeftClick", "delayMs": 80 }
],
"actionLoop": true,
"activeState": "loop.gazeClick"
```

- Without `actionLoop`: run series once on dwell activate.
- With `actionLoop`: sticky mode; re-activate to stop. Closing the board stops its loops.
- Commands: `mouseMoveToGaze`, `stopAllActionLoops`.

#### Lifecycle arrays

```json
"onOpen":  [ { "type": "command", "name": "…" } ],
"onLoad":  [ … ],
"onClose": [ … ]
```

One-shot only (no loops). `onOpen`+`onLoad` on create; `onLoad` on document replace; `onClose` before destroy.

### Notable command names

| Command | Role |
|---------|------|
| `toggleDwellSuspend` / `suspendDwell` / `resumeDwell` | Global dwell pause (auto-exempt) |
| `toggleMagnifier`, `toggleLookToScroll`, `toggleGazeReticle`, `toggleGazeMouseFollow` | Assist tools |
| `mouseDwellMove`, mouse click/move/scroll builtins | Mouse assist |
| `mouseMoveToGaze` | Move cursor to last valid gaze |
| `openMappingProperties`, `openPreview` | Curve editor / head+gaze preview |
| `theme.light` / `theme.dark` / `theme.custom` | Theme mode |
| `settings.*` | Settings hub editors |

### Mapping profile (`resources/mappings/*.json`)

Command name → ordered **input** outputs: `keyTap`, `keyCombo`, `text`, `mouseClick`, `mouseMove`, `mouseMoveTo`, `mouseScroll`, `gamepadButton`, `gamepadAxis`. Orthogonal to layout action series.

### Response curves (scaffolding)

OpenTrack-style piecewise-linear curve editor (`MappingPropertiesWindow`) can edit
in-memory control points for:

- `head.yaw` / `pitch` / `roll` / `x` / `y` / `z`
- `lookToScroll.response`
- `magnifier.stickiness`
- `gazeIndicator.stickiness`
- `gazeMouse.stickiness`

**Runtime note:** stickiness for magnifier / reticle / gaze→mouse currently uses
the discrete `magFollowProfile` (sticky/balanced/snappy), not these editable curves.
The graph UI is available via **Settings → Response curves** / `openMappingProperties`
for authoring; full runtime wiring + persistence is planned.

### Themes

`themeMode`: `light` | `dark` | `custom` with Voice-aligned tokens (`bgMain`, `accent`, `text`, …). Color picker uses Voice sample palette.

### Head pose

Tobii head-pose stream (optional soft-bind). Preview shows gaze + planar head + raw yaw/pitch/roll/x/y/z (OpenTrack units).

### Dwell suspend UI

When dwell is suspended: semi-transparent border around the screen with a **break** at unpause targets (Pause dwell cells).

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
