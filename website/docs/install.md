# Install

Gazer is a Windows desktop app. There is no web client.

## What you need

- Windows
- Optional: a [Tobii Eye Tracker 5](https://www.tobii.com/) with its drivers (Tobii Experience / Eye Tracking Core). Gazer loads Stream Engine from that install; the DLL is not in the MSI. Without hardware, Gazer uses the **mouse** as the gaze point (tray / Settings → Assist → tracker).
- Optional: a local [AutoHotkey](https://www.autohotkey.com/) install for `<AHK>` cells (v2 preferred). AutoHotkey is not bundled.
- Optional: [ViGEmBus](https://github.com/nefarius/ViGEmBus) for the virtual Xbox pad. Settings → **Assist** can download the official driver setup. Gazer ships `ViGEmClient.dll` next to `Gazer.exe`.

## MSI

Published builds go on [GitHub Releases](https://github.com/AdamRoden/Gazer/releases).

The MSI installs to `Program Files\Gazer\` with Start Menu and desktop shortcuts, and a logon entry for `Gazer.exe --guard` (watchdog). That copy is stamped `uiAccess=true` and signed so it can sit above Task Manager and type/click into elevated windows. A `Gazer.exe` built in the source tree is left without UIAccess so it still launches from the build directory.

If no release is up yet, build an MSI from the tree: [README — Beta MSI](https://github.com/AdamRoden/Gazer/blob/master/README.md#beta-msi).

Testers still need Tobii drivers for hardware gaze. Without a tracker, use the mouse backend.

## First launch

Start Gazer. Unless **start docked** is on, the drawer opens. Dwell a cell until progress completes, then look away (or at another cell). Dock, tray, and shipped boards: [Shell](shell.md).

Closing overlay pages does not quit. **Quit → Yes** on the dock, or tray **Quit**, does.

## Files and data

| Path | Contents |
|------|----------|
| `gazer.log` | Log in the working directory (rotated copies under `%LOCALAPPDATA%\Gazer\logs`) |
| `%LOCALAPPDATA%\Gazer\crashes\` | Minidumps after a crash or hung-peer takeover |
| `%AppData%\Gazer\settings.json` | Prefs (theme, dwell, assist, …). Not the ElevenLabs key. |
| `%AppData%\Gazer\layouts\` | User Page XML (overrides shipped ids) |
| `%AppData%\Gazer\secrets\eleven.dpapi` | DPAPI-protected ElevenLabs API key |
| `%AppData%\Gazer\` speech store | Soundboard JSON, clips, voice cache, history |

Each MSI install deletes `settings.json` so the next launch writes factory defaults. Speech secrets, clips, and user layouts are left in place.

## Build from source

CMake, Qt, tests, and the MSI script: [README — Build and run](https://github.com/AdamRoden/Gazer/blob/master/README.md#build-and-run).
