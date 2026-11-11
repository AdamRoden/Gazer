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
- **Retry eye tracker** — reconnect Tobii, or start it again if Gazer is on the mouse tracker
- **Use mouse tracker** — switch the gaze point to the pointer (does not happen by itself after a stall)
- **Quit**

Closing overlay pages does not exit. `quitApp` (Quit → Yes) does.

## If tracking stops

A blink is not a stall. While the eye tracker is running, invalid gaze or a quiet stream waits 60 seconds before it counts as lost. A disconnect is immediate and does not show the notice. Gazer then releases keys, repeating actions, look-to and head-stick output, and cancels the dwell that was in progress. It does not switch to the mouse on its own.

The notice says **No gaze detected** and stays up for 10 seconds. A valid gaze sample hides it and leaves open pages alone. If gaze is still missing when those 10 seconds end, the notice closes and so do the open pages. The dock chip stays. The tray still has **Retry eye tracker** and **Use mouse tracker**. A quiet head stream releases head output after 10 seconds on its own, including when gaze is also missing, and does not cancel board dwell. The next valid head pose takes head control back.

## If Gazer freezes

The installed copy starts a tiny **guard** (`Gazer.exe --guard`) at logon. If the host hangs, launching Gazer again (or the Pause key) kills the hung process and brings a new one up. After three crashes in two minutes the guard shows **Restart** / **Mouse pointer** / **Quit**. A second `Gazer.exe --action` waits for an ACK; if the host does not answer, that second process takes over.

UAC, the lock screen, and Ctrl+Alt+Del sit on a Secure Desktop Gazer cannot draw on. Inject pauses until you return. Exclusive-mode fullscreen games can cover Gazer until they yield (borderless windowed is the reliable setting); the guard can offer **Show desktop**.

Hardware backups: a USB switch or Xbox Adaptive Controller mapped to launch `Gazer.exe`, or Windows Eye Control.

## Shipped pages

Catalog id = filename stem under `resources/layouts/`. User copies in `%AppData%\Gazer\layouts` override the same id.

| Id | Kind |
|----|------|
| `main` | Root dock + drawer + quit |
| `main_settings_host` / `main_settings_*` | Settings. The drawer opens the host. The top row is Settings, the page name, and Close. Categories are on the left. |
| `compose` | Gaze composer (phrase, chips, soundboard, keyboard) |
| `example_keyboard` | Compact keyboard (layers: letters, shift, symbols, symbols+shift) |
| `example_mouse` | Mouse pad |
| `example_gamepad` | Xbox-layout virtual pad (ViGEm) |
| `example_assist` | Assist tools |
| `qwerty_main` | Full QWERTY + edge strips |
| `qwerty_file` | Same keyboard as `qwerty_main`, for a Windows open or save dialog. Speak **Import** and **Export** use the in-app file browser |
| `uw_qwerty` | Wide QWERTY |
| `uw_qwerty_pad` | Wide QWERTY with no number row, a numpad, and the nav cluster beside the edge strip |

`OpenPage` / `ClosePage` / `CloseAllPages` / `CloseOtherPages` / `TogglePage` attach or remove a **Page**. `ShowLayers` sets which grid/zone layers are visible on a page. Closing a page removes all of its elements with it.

Idle auto-close: boards with `autoClose="true"` dismiss after Settings `layoutAutoCloseIdleMs` with no dwell. The master root never destroys itself. `suspendDwell` stops the idle timer; `resumeDwell` restarts it from zero.
