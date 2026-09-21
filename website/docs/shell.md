# Shell

`resources/layouts/main.xml` is the process-lifetime root. It never closes. One host window is sized to visible chrome (not the whole desktop). Grids and zones are regions on that surface, not extra windows.

| Piece | XML | Role |
|-------|-----|------|
| Root page | `main` (`master="true"`) | Dock **Main** / **Hide** and **Sleep** zones, plus drawer and quit grids |
| Drawer grid | `drawer` | Keyboard, Speak, Mouse, Gamepad, Assist, More, Settings, Close All, Pause dwell, Quit |
| Quit grid | `quit` | Yes exits; No returns to the drawer |

Drawer (layer 2) and quit (layer 3) start hidden; the page opens on layer 1. **Main** shows the drawer (`ShowLayers` `2`); **Hide** returns to layer 1; **Quit** switches to layers `1,3`. Opening Keyboard / Speak / Mouse / Gamepad / Assist / Settings / More attaches that page on the same host, then the drawer auto-collapses (unless you turned that off).

- **Main** is shown only while no master grid is up. Dwell it to grow the drawer from the bottom.
- **Sleep** stays available while the drawer is open. Shell zones and grids paint and hit above other boards.
- Gaze on the drawer or the dock chips counts as using the shell, so the drawer idle timer does not fire while you look at Sleep.
- The host window stays above the Windows taskbar.

## First session

1. Start Gazer. A splash tour walks the dock (Assist → **Play tour** to replay). Unless **start docked** is on, the drawer opens after the tour.
2. Dwell a cell until progress completes.
3. Open Keyboard, Speak, Mouse, Gamepad, Assist, or Settings as extra pages. They stay up after the drawer collapses.
4. **Hide** hides the drawer. **Close All** closes other pages and then collapses.
5. Tray: show layout (raise host), show head-pose preview, quit.

## Tray

The tray icon owns the process:

- **Show layout** — raise the host window
- **Head-pose preview** — 3D head model from the tracker (`openPreview`)
- **Page editor**
- **Quit**

Closing overlay pages does not exit. `quitApp` (Quit → Yes) does.

## Shipped pages

Catalog id = filename stem under `resources/layouts/`. User copies in `%AppData%\Gazer\layouts` override the same id.

| Id | Kind |
|----|------|
| `main` | Root dock + drawer + quit |
| `main_settings` / `main_settings_*` | Settings hub, then Speed / Magnify / Indicators / Assist / Tools / Theme / Speech |
| `compose` | Gaze composer (phrase, chips, soundboard, keyboard) |
| `example_keyboard` | Compact keyboard (layers: letters, shift, symbols, symbols+shift) |
| `example_mouse` | Mouse pad |
| `example_gamepad` | Xbox-layout virtual pad (ViGEm) |
| `example_assist` | Assist tools |
| `qwerty_main` | Full QWERTY + edge strips |
| `uw_qwerty` | Wide QWERTY |

`OpenPage` / `ClosePage` / `CloseAllPages` / `CloseOtherPages` / `TogglePage` attach or remove a **Page**. `ShowLayers` sets which grid/zone layers are visible on a page. Closing a page removes all of its elements with it.

Idle auto-close: boards with `autoClose="true"` dismiss after Settings `layoutAutoCloseIdleMs` with no dwell. The master root never destroys itself. `suspendDwell` stops the idle timer; `resumeDwell` restarts it from zero.
