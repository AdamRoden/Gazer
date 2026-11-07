# Install

Gazer is a Windows desktop app. There is no web client.

## What you need

- Windows
- Optional: a [Tobii Eye Tracker 5](https://www.tobii.com/) with its drivers (Tobii Experience / Eye Tracking Core). Gazer loads Stream Engine from that install; the DLL is not in the MSI. Without hardware, Gazer uses the **mouse** as the gaze point (tray / Settings → **Setup**).
- Optional: a local [AutoHotkey](https://www.autohotkey.com/) install for `<AHK>` cells (v2 preferred). AutoHotkey is not bundled.
- Optional: [ViGEmBus](https://github.com/nefarius/ViGEmBus) for the virtual Xbox pad. Settings → **Setup** can download the official driver setup. Gazer ships `ViGEmClient.dll` next to `Gazer.exe`.

## MSI

Published builds go on [GitHub Releases](https://github.com/AdamRoden/Gazer/releases).

The MSI installs to `Program Files\Gazer\` with a Start Menu shortcut and a logon entry for `Gazer.exe --guard` (watchdog). Setup asks whether to add a desktop shortcut and, on the last page, whether to launch Gazer. The shortcut choice is remembered for the next install. A quiet install (`msiexec /qn`) keeps the shortcut unless you pass `INSTALLDESKTOPSHORTCUT=0`, and it does not launch Gazer. The Program Files copy is stamped `uiAccess=true` and signed so it can sit above Task Manager and type/click into elevated windows. A `Gazer.exe` built in the source tree is left without UIAccess so it still launches from the build directory.

Install, upgrade, repair, and installing the same version again keep your settings. The options page has **Reset my settings to factory defaults** (off unless you check it). A quiet install keeps settings unless you pass `RESETUSERSETTINGS=1`. That deletes `settings.json` (and a leftover copy under `Gazer\Gazer\`) so the next launch writes factory defaults. Speech secrets, clips, and user layouts stay either way. Inside Gazer, Settings → reset (`settings.reset`) writes factory defaults and keeps the previous file as `settings.json.bak`.

If no release is up yet, build an MSI from the tree: [README — Beta MSI](https://github.com/AdamRoden/Gazer/blob/master/README.md#beta-msi).

Testers still need Tobii drivers for hardware gaze. Without a tracker, use the mouse backend.

## First launch

Start Gazer. Unless **start docked** is on, the drawer opens. Dwell a cell until progress completes, then look away (or at another cell). Dock, tray, and shipped boards: [Shell](shell.md).

Closing overlay pages does not quit. **Quit → Yes** on the dock, or tray **Quit**, does.

## Files and data

| Path | Contents |
|------|----------|
| `%LOCALAPPDATA%\Gazer\logs\gazer.log` | Current session. Attach this when you report a bug. |
| `%LOCALAPPDATA%\Gazer\logs\gazer-*.log` | Earlier sessions (five kept). Attach the newest if Gazer was restarted after the problem. |
| `%LOCALAPPDATA%\Gazer\logs\guard.log` | Watchdog log (`Gazer.exe --guard`). |
| `%LOCALAPPDATA%\Gazer\crashes\` | Minidumps after a crash or hung-peer takeover |
| `%AppData%\Gazer\settings.json` | Prefs (theme, dwell, assist, …). Not the ElevenLabs key. Kept across installs. |
| `%AppData%\Gazer\settings.json.bak` | Last backup, written before a settings upgrade or an in-app factory reset. |
| `%AppData%\Gazer\settings.vN.json.bak` | First backup of an older settings schema. |
| `%AppData%\Gazer\settings.bad-*.json` | Unreadable `settings.json`, copied aside before recovery or a factory file. |
| `%AppData%\Gazer\layouts\` | User Page XML (overrides shipped ids) |
| `%AppData%\Gazer\secrets\eleven.dpapi` | DPAPI-protected ElevenLabs API key |
| `%AppData%\Gazer\` speech store | Soundboard JSON, clips, voice cache, history |

If `settings.json` is damaged, Gazer copies it to `settings.bad-*.json`. When `settings.json.bak` is valid JSON, Gazer restores that backup. Otherwise it writes factory defaults so the app can still be used, and the `.bad` file is still there to recover by hand. An older file is upgraded in place after its backup is written. A file from a newer Gazer is left on disk unchanged.

## Reporting a problem

Settings → **Setup** → **Open log folder**, or the tray menu **Open log folder**, opens `%LOCALAPPDATA%\Gazer\logs`.

Attach `gazer.log`. If you already restarted Gazer, also attach the newest `gazer-` file (that is the previous session). If Gazer crashed or a hung window was replaced, also attach the newest file in `%LOCALAPPDATA%\Gazer\crashes`. The log does not include the ElevenLabs key.

## Build from source

CMake, Qt, tests, and the MSI script: [README — Build and run](https://github.com/AdamRoden/Gazer/blob/master/README.md#build-and-run).
