# Gazer

Gaze-driven AAC and system input for Windows. C++20 / Qt 6.

Gazer turns live gaze (Tobii Eye Tracker 5, or the mouse as a fallback) into on-screen pages you **dwell** to activate. Pages can type, click, move the pointer, speak, and run assist tools. The long-term aim is one stack for accessible gaming in place of OptiKey + OpenTrack + UCR + AutoHotkey.

Version **0.6.1**. License: [GPL-3.0](LICENSE).

Contributors and coding agents: start at [AGENTS.md](AGENTS.md). Page XML schema: [docs/page-xml.md](docs/page-xml.md). Command catalog: [src/app/Commands.md](src/app/Commands.md).

---

## Features

| Area | What it does |
|------|----------------|
| **Dwell pages** | One frameless overlay. Boards are XML grids and zones. Look at a cell until progress completes; the last dwell step repeats while gaze holds. |
| **Shell** | Process-lifetime dock: **Main** (drawer), **Sleep** (pause dwell), Keyboard / Speak / Mouse / Assist / Settings, Close All, Quit. |
| **Keyboards** | Compact and QWERTY boards. `Send` injects keys. Shift and symbol layers use `ShowLayers`. Modifiers cycle Up → Down → LockedDown. |
| **Mouse pad** | Nudge, click, hold, scroll, dwell-move, click-at-gaze, ComboMouse. |
| **Assist** | Live magnifier, gaze reticle, gaze-follows-cursor, look-to-scroll, mag-pick zoom, foresight. |
| **Speech** | Canned `<Speak>` (Windows SAPI). Gaze composer: internal phrase, word chips, ElevenLabs or SAPI, soundboard clips, history. |
| **Settings** | Gaze-operated boards for Speed, Magnify, Indicators, Assist, Tools, Theme, Speech, Head. No desktop dialogs for live prefs. |
| **Page editor** | Qt Widgets designer for the same XML the runtime loads. Save of a shipped page writes a user copy. |
| **Always on top** | Overlay stays above the taskbar and other apps. The installed MSI can sit above Task Manager (`uiAccess`). |
| **Scripts / AHK** | Cells can run AutoHotkey from a local install, or `gazer.*` from scripts. |

---

## How dwell works

1. Gaze lands on a cell or zone.
2. **Scan grace** waits until you are stably on-target (default 100 ms; Settings → Speed → Advanced).
3. **Activation** is a sequence of step times in milliseconds. Progress fills through each step. The **last** step repeats while gaze holds.
4. The cell fires when dwell **ends** and blink grace expires (look away, or look at another cell). Blink grace **pauses** progress; look-away does not start another fill until grace expires.

Two sequences share scan grace:

| Sequence | Used for |
|----------|----------|
| **Rapid** | `Send`, composer typing, modifiers, mapping keys (`backspace`, `space`, `enter`, …) |
| **Standard** | Settings, navigation, mouse, AHK, assist toggles, composer word chips |

Both live under Settings → **Speed** (Slow / Normal / Fast / Custom). Per-cell XML can override `scanGrace`, `dwellGrace`, and `activation`.

**Pause dwell** / **Sleep** suspends dwell everywhere except `suspendExempt` unlock targets (the Main chip also resumes). A dim screen border leaves a gap at those targets.

When pages overlap, the topmost page’s grid is opaque: gaze and paint do not fall through. Shell zones (dock chips) still win over everything.

---

## What you get at launch

`resources/layouts/main.xml` is the process-lifetime root. It never closes. One host window is sized to visible chrome (not the whole desktop). Grids and zones are regions on that surface, not extra windows.

| Piece | XML | Role |
|-------|-----|------|
| Root page | `main` (`master="true"`) | Dock **Main** and **Sleep** zones, plus drawer and quit grids |
| Drawer grid | `drawer` | Keyboard, Speak, Mouse, Assist, More, Settings, Dismiss, Close All, Pause dwell, Quit |
| Quit grid | `quit` | Yes exits; No returns to the drawer |

Drawer (layer 2) and quit (layer 3) start hidden; the page opens on layer 1. The Main chip `ShowLayers`s `1,2`; Dismiss returns to `1`; Quit switches to `1,3`. Opening Keyboard / Speak / Mouse / Assist / Settings / More attaches that page on the same host, then the drawer auto-collapses (unless you turned that off).

- **Main** is shown only while no master grid is up (`visibleWhen="!expanded"`). Dwell it to grow the drawer from the bottom.
- **Sleep** stays available while the drawer is open. Shell zones and grids paint and hit above other boards.
- Gaze on the drawer or the dock chips counts as using the shell, so the drawer idle timer does not fire while you look at Sleep.
- The host window stays above the Windows taskbar.

The **tray** owns process lifetime. Closing a page does not quit the app.

Typical first session:

1. Start Gazer. Unless **start docked** is on, the drawer opens.
2. Dwell a cell until progress completes.
3. Open Keyboard, Speak, Mouse, Assist, or Settings as extra pages. They stay up after the drawer collapses.
4. **Dismiss** hides the drawer. **Close All** closes other pages and then collapses.
5. Tray: show layout (raise host), show head-pose preview, quit.

---

## Keyboards and typing

Shipped boards:

| Page | Role |
|------|------|
| `qwerty_main` | Full QWERTY plus edge mouse / LTS / window AHK strips |
| `example_keyboard` | Compact 3×12 letters / shift / symbols |
| `uw_qwerty` | Wide QWERTY (drawer **More**) |

Letter cells use `send="q"`. Mapping-profile names (`backspace`, `tab`, `enter`, `space`, `escape`) inject through the mapping JSON. Modifier cells (`leftShift`, `leftCtrl`, `leftAlt`, `leftWin`) **cycle** OS hold: Up → Down → LockedDown. `releaseModifiers` clears them.

Shift and symbol layers are extra grids on the same page (`layers="2"`, `layers="3"`). A Shift cell runs `ShowLayers` instead of holding a physical shift key for the letter grid.

While the **composer** is on top, `Send` and mapping keys go into the internal phrase — they never type into the focused OS app. `qwerty_main` is unchanged and still types into Windows.

---

## Mouse

`example_mouse` is the dwell mouse pad:

- **Nudge** by step (`mouseMoveByDirection`, `cycleMouseMoveAmount`)
- **Click** at the current cursor (`mouseLeftClick` / middle / right; `double`, `toggle` hold)
- **Scroll** by step or **look-to-scroll**
- **Move to gaze** — dwell a desktop point to warp the cursor
- **Move + click at gaze** — dwell-move, then one click
- **Gaze click loop** — sticky dwell-move then click until you stop it
- **Magnify pick** / **Foresight** — zoom a region before the click (Settings → Magnify)
- **ComboMouse** — directional pie around the pointer

`stopAllActionLoops` (and Close All / Close Other) stops sticky series, click-loop, holds, and ComboMouse.

---

## Assist tools

Opened from the drawer **Assist** page (`example_assist`) or the mouse pad.

| Tool | Command | Notes |
|------|---------|--------|
| Gaze magnifier | `toggleMagnifier` | Live lens. Exclusive with the gaze reticle. |
| Show gaze | `toggleGazeReticle` | Marker at the gaze point. Exclusive with the magnifier. |
| Gaze mouse | `toggleGazeMouseFollow` | Cursor follows gaze. |
| Move mouse (dwell) | `mouseMoveToGaze` | Arm, then dwell a screen point. |
| Gaze click loop | `mouseMoveToGazeClickLoop` | Sticky dwell-move then click. |
| Magnify pick | `toggleMouseMoveMagPick` | Static zoom window for the pick. |
| Foresight | `toggleMouseMoveForesight` | Remember a desktop dwell and zoom that point when Move-to arms. |
| Look-to-scroll | `toggleLookToScroll` | Always places the cursor first, then scrolls from gaze vs a deadzone. Pie for speed, axis (vertical / horizontal / both), reset, quit. |
| ComboMouse | `toggleComboMouse` | Inner drift ring + outer command pie. |
| Edit page | `openPageEditor` | Opens the XML designer. |

Look-to-scroll peak speeds: 1, 5, 10, 20, 40 notches/sec (`lts.speed.slower` / `.faster`). Overlay z-order (front → back): reticle, live lens, mag-pick, other assist overlays, then the page host.

---

## Speech composer

Drawer **Speak** (`compose.open`) opens `compose.xml`. This is an internal phrase, not OS typing.

- Type on the composer keyboard into a buffer. Word chips: first dwell jumps the caret; later dwells delete the word (`<Phase>`).
- **Speak** synthesizes the buffer. Engine is **ElevenLabs** when a model, DPAPI-stored API key, and voice id are set; otherwise **Windows SAPI**. Three Eleven failures latch SAPI until model, voice, or key changes.
- XML `<Speak value="Hello"/>` and `gazer.speak()` are always SAPI (no cloud quota on canned cells).
- **Soundboard**: pin baked MPEG clips onto topic cells; replay without re-synthesis. Store: `%AppData%\Gazer\` (`boards.json`, `clips/`).
- **Freestyle**: saved voices + ElevenLabs v3 audio tags (`[laugh]`, accents, …).
- **History**: last 50 composed utterances; replay, restore, or delete.
- Settings → **Speech**: paste API key from the clipboard (never stored in `settings.json`), model, speed, volume.

Details: [docs/composer-elevenlabs.md](docs/composer-elevenlabs.md).

---

## Settings

Drawer **Settings** (`main_settings`) is a hub of live pages:

| Board | What you change |
|-------|-----------------|
| **Speed** | Slow / Normal / Fast / Custom. Standard vs rapid sequences, mouse-move dwell, mag-pick dwell. Advanced: scan grace. |
| **Magnify** | Mag-pick, foresight, zoom window size/shape, follow profile (slow / sticky / smooth / snappy). |
| **Indicators** | Dwell progress shape (radial, pie, fill directions), pick markers, flash. |
| **Assist** | Tracker (auto Tobii / mouse), live lens, gaze helpers. |
| **Tools** | Look-to-scroll deadzone / speed / HUD, ComboMouse radii and colors. |
| **Theme** | Light / dark, brightness, tint family, accent, progress color, saturation, custom palette. |
| **Speech** | ElevenLabs key, engine, speed, volume. |

Session toggles (start docked, auto-collapse drawer, layout auto-close) live on these boards as well.

---

## Theme

Surfaces use a Fluent-style palette: appearance (light / dark) × brightness (five shades) × optional tint family × Apple-system accent (Red … Pink) × progress color. Cells can also name a theme **role** (`background`, `surface`, `accent`, `progress`, `tertiary`, `foreground`, `danger`) or a brand (`red`, `orange`, …) instead of a hex color — names resolve from the live theme.

Icons: set `icon` to the filename stem in `resources/icons/svg/` (`menu`, `mouseLeftClick`, `keyTab`). Matching is case-insensitive; a trailing `Icon` is ignored. Unknown names fall back to the label. Most glyphs are [Material Symbols Rounded](https://fonts.google.com/icons?icon.set=Material+Symbols&icon.style=Rounded); see [resources/icons/README.md](resources/icons/README.md).

---

## Page editor

Tray → **Page editor**, command `openPageEditor`, or launch with `--editor`.

The designer edits **Page XML** (the same files the runtime loads). Three panes:

| Pane | Contents |
|------|----------|
| Left | **Add** (button, label, toggle, tab, slider, zone, grid, subgrid, named style, named dwell) and the element tree (page → styles / dwells / zones / grids → cells / subgrids). Right-click to duplicate, delete, convert cell ↔ zone, add a subgrid, or change paint order. |
| Center | **Fit grid** (default): the selected grid fills the canvas. Uncheck it to see true placement on a 1920×1080 virtual display. Click / Shift-click to select; drag to move; accent handles resize; arrows nudge (Shift = 16 px); Delete removes. **Esc** cancels click-to-place. Zones show the progress chip and the dwell region. The toolbar **layer** combo (next to Code view) filters which grid/zone layers paint on the canvas. |
| Right | Tabs follow the selection: **Page**; **Grid / Style / Placement**; **Cell** or **Zone / Style / Placement / Action**; named **Style** or **Dwell** alone. Placement holds anchor, offset, row/col/span, and zone progress/dwell regions. Cell and zone dwell inherit/overrides sit at the bottom of Action. |

File → New asks for id, name, and a template (blank, full keyboard, keyboard row, settings row, zone chip). File → Open lists shipped and user `*.xml` pages. **Save** of a shipped file writes a user copy to `%AppData%\Gazer\layouts` and leaves `resources/` unchanged. Shift is a modifier on QWERTY boards (labels switch to the shifted glyph). **Test on canvas** (F6) plays a dwell ring. **Test on desktop** (F5) attaches the current page on the live host. Master roots cannot be live-tested. Empty actions warn before Save and F5.

Schema: [docs/page-xml.md](docs/page-xml.md).

---

## Tray

The tray icon owns the process:

- **Show layout** — raise the host window
- **Head-pose preview** — 3D head model from the tracker (`openPreview`)
- **Page editor**
- **Quit**

Closing overlay pages does not exit. `quitApp` (Quit → Yes) does.

---

## Shipped pages

Catalog id = filename stem under `resources/layouts/`. User copies in `%AppData%\Gazer\layouts` override the same id.

| Id | Kind |
|----|------|
| `main` | Root dock + drawer + quit |
| `main_settings` / `main_settings_*` | Settings hub, then Speed / Magnify / Indicators / Assist / Tools / Theme / Speech |
| `compose` | Gaze composer (phrase, chips, soundboard, keyboard) |
| `example_keyboard` | Compact keyboard (layers: letters, shift, symbols, symbols+shift) |
| `example_mouse` | Mouse pad |
| `example_assist` | Assist tools |
| `qwerty_main` | Full QWERTY + edge strips |
| `uw_qwerty` | Wide QWERTY |

`OpenPage` / `ClosePage` / `CloseAllPages` / `CloseOtherPages` / `TogglePage` attach or remove a **Page**. `ShowLayers` sets which grid/zone layers are visible on a page. Closing a page removes all of its elements with it.

Idle auto-close: boards with `autoClose="true"` dismiss after Settings `layoutAutoCloseIdleMs` with no dwell. The master root never destroys itself. `suspendDwell` stops the idle timer; `resumeDwell` restarts it from zero.

---

## Files and data

| Path | Contents |
|------|----------|
| `gazer.log` | Log in the working directory |
| `%AppData%\Gazer\settings.json` | Prefs (theme, dwell, assist, …). Not the ElevenLabs key. |
| `%AppData%\Gazer\layouts\` | User Page XML (overrides shipped ids) |
| `%AppData%\Gazer\secrets\eleven.dpapi` | DPAPI-protected ElevenLabs API key |
| `%AppData%\Gazer\` speech store | Soundboard JSON, clips, voice cache, history |

Each MSI install deletes `settings.json` so the next launch writes factory defaults. Speech secrets, clips, and user layouts are left in place.

---

## Requirements

- Windows
- CMake ≥ 3.21
- Qt 6 (Core, Gui, Widgets, Qml, Quick) — tested with 6.11.1 MinGW
- Optional: [Tobii Stream Engine](https://developer.tobii.com/) headers under `third_party/` (`tobii.h`, `tobii_streams.h`) and `tobii_stream_engine.dll` on the machine. Without hardware, Gazer uses the mouse tracker.
- Optional: a local [AutoHotkey](https://www.autohotkey.com/) install for `<AHK>` cells (v2 preferred; `#Requires AutoHotkey v1` selects v1). Set `GAZER_AHK` to an exe to override discovery. AutoHotkey is not bundled.

---

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

Tests: `cmake --build build --target GazerPageTests` (and `GazerDwellTests`).

### Beta MSI

```powershell
winget install WiXToolset.WiXCLI   # once
.\scripts\build-msi.ps1            # configure, build, stage, MSI
.\scripts\build-msi.ps1 -SkipBuild # reuse existing build\Gazer.exe
```

Output: `dist\Gazer-<version>-beta.msi`. Installs to `Program Files\Gazer\` with Start Menu and desktop shortcuts. Pages ship under `resources\`.

The MSI stamps `uiAccess=true` on the staged exe and Authenticode-signs it so the **Program Files** copy can sit above Task Manager and type/click into elevated windows. `.\build\Gazer.exe` is left without UIAccess so it still launches from the build directory. A real code-signing PFX: `$env:GAZER_SIGN_PFX` and optional `$env:GAZER_SIGN_PFX_PASSWORD`. Without those, the script uses a local self-signed cert (`%LOCALAPPDATA%\Gazer\signing\`) and the MSI trusts it at install time.

Testers still need Tobii drivers for hardware gaze. Without a tracker, use the mouse backend (tray / Settings → tracker).

---

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
- Builtins run first; unknown names fall through to the mapping profile.

Boards are XML only (`resources/layouts/*.xml`). `gazer.openPage` / `loadPage` open those pages on the live host. Editor F5 previews attach XML copies under `__editor_preview_*` ids so they do not replace the live page.

Script API (`gazer` in QJS): `log`, `speak` (SAPI), `typeText`, `runCommand`, `openPage`, `loadPage` (closes the current attached page first), `focusedPageId`.

---

## Commands

Builtins first; unknown names fall through to the mapping profile. Full catalog (settings patterns, mapping-only names): [src/app/Commands.md](src/app/Commands.md).

| Command | Role |
|---------|------|
| `quitApp` | Exit |
| `toggleDwellSuspend` / `suspendDwell` / `resumeDwell` | Global dwell pause (not aliases of each other) |
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
| `compose.open` | Speech composer |
| `theme.light` / `.dark` | Light or dark surfaces |
| `theme.brightness.0`…`.4` | Background shade |
| `theme.tint.none` / `.primary` / `.complementary` / `.analogous1` / `.analogous2` / `.tertiary1` / `.tertiary2` | Surface tint hue |
| `theme.primary.0`…`.8` | Accent (Apple system: Red … Pink) |
| `theme.secondary.0`…`.8` | Progress (same 9) |
| `settings.*` | Settings hub editors, nudges, presets |
