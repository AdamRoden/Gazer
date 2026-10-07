# Shell

`resources/layouts/main.xml` is the process-lifetime root. It never closes. One host window is sized to visible chrome (not the whole desktop). Grids and zones are regions on that surface, not extra windows.

| Piece | XML | Role |
|-------|-----|------|
| Root page | `main` (`master="true"`) | Dock **Main** / **Hide** and **Sleep** zones, plus drawer, More, and quit grids |
| Drawer grid | `drawer` | More, Settings, Close All, Pause dwell, Quit |
| More grid | `more` | Keyboard, Speak, Mouse, Gamepad, Assist, wide QWERTY (**More**), Numpad, Back |
| Quit grid | `quit` | Yes exits; No returns to the drawer |

Drawer (layer 2), More (layer 4), and quit (layer 3) start hidden; the page opens on layer 1. **Main** shows the drawer (`ShowLayers` `2`); **Hide** returns to layer 1; **Quit** switches to layers `1,3`. Drawer **More** shows layer 4. **Back** on that layer returns to the drawer. Opening Keyboard, Speak, Mouse, Gamepad, Assist, the wide QWERTY, Numpad, or Settings attaches that page on the same host, then the strip auto-collapses (unless you turned that off).

- **Main** is shown only while no master grid is up. Dwell it to grow the drawer from the bottom.
- **Sleep** stays available while the drawer or More is open. Shell zones and grids paint and hit above other boards.
- **Rescue** (next to Sleep) stops loops, releases keys, turns off assist tools, closes pages, and docks. Use it when a board or sticky input has you stuck.
- Gaze on the drawer or the dock chips counts as using the shell, so the drawer idle timer does not fire while you look at Sleep.
- The host window stays above the Windows taskbar.

## First session

1. Start Gazer. A splash tour walks the dock (Settings → **Setup** → **Play tour** to replay). Unless **start docked** is on, the drawer opens after the tour.
2. Dwell a cell until progress completes.
3. Open Settings from the drawer, or Keyboard, Speak, Mouse, Gamepad, Assist, wide QWERTY, or Numpad from **More**. They stay up after the strip collapses.
4. **Hide** hides the drawer. **Close All** closes other pages and then collapses.
5. Tray: show layout (raise host), show head-pose preview, quit.

## Tray

The tray icon owns the process:

- **Show layout** — raise the host window
- **Head-pose preview** — 3D head model from the tracker (`openPreview`)
- **Page editor**
- **Open log folder** — `%LOCALAPPDATA%\Gazer\logs` (`openLogFolder`). Attach `gazer.log` when you report a bug. See [Install — Reporting a problem](install.md#reporting-a-problem).
- **Quit**

Closing overlay pages does not exit. `quitApp` (Quit → Yes) does.

## If Gazer freezes

The installed copy starts a tiny **guard** (`Gazer.exe --guard`) at logon. If the host hangs, launching Gazer again (or the Pause key) kills the hung process and brings a new one up. After three crashes in two minutes the guard shows **Restart** / **Mouse pointer** / **Quit**. A second `Gazer.exe --action` waits for an ACK; if the host does not answer, that second process takes over.

UAC, the lock screen, and Ctrl+Alt+Del sit on a Secure Desktop Gazer cannot draw on. Inject pauses until you return. Exclusive-mode fullscreen games can cover Gazer until they yield (borderless windowed is the reliable setting); the guard can offer **Show desktop**.

Hardware backups: a USB switch or Xbox Adaptive Controller mapped to launch `Gazer.exe`, or Windows Eye Control.

## Shipped pages

Catalog id = filename stem under `resources/layouts/`. User copies in `%AppData%\Gazer\layouts` override the same id.

| Id | Kind |
|----|------|
| `main` | Root dock + drawer + quit |
| `main_settings` / `main_settings_*` | Settings hub and tabs (Dwell, Zoom, Place, Gaze, Head, Speak, Theme, Setup). Done closes settings. |
| `compose` | Gaze composer (phrase, chips, soundboard, keyboard) |
| `example_keyboard` | Compact keyboard (layers: letters, shift, symbols, symbols+shift) |
| `example_mouse` | Mouse pad |
| `example_gamepad` | Xbox-layout virtual pad (ViGEm) |
| `example_assist` | Assist tools |
| `qwerty_main` | Full QWERTY + edge strips |
| `uw_qwerty` | Wide QWERTY |
| `uw_qwerty_pad` | Wide QWERTY with no number row, a numpad, and the nav cluster beside the edge strip |

`OpenPage` / `ClosePage` / `CloseAllPages` / `CloseOtherPages` / `TogglePage` attach or remove a **Page**. `ShowLayers` sets which grid/zone layers are visible on a page. Closing a page removes all of its elements with it.

Idle auto-close: boards with `autoClose="true"` dismiss after Settings `layoutAutoCloseIdleMs` with no dwell. The master root never destroys itself. `suspendDwell` stops the idle timer; `resumeDwell` restarts it from zero.
