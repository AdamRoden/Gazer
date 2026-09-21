# Gazer

Gaze-driven system input for Windows. C++20 / Qt 6.

Gazer turns live gaze (Tobii Eye Tracker 5, or the mouse as a fallback) into on-screen pages you **dwell** to activate. Pages can type, click, move the pointer, speak, and run assist tools. The long-term aim is one stack for accessible gaming.

Version **0.6.3**. License: [GPL-3.0](LICENSE).

**User manual:** [adamroden.github.io/Gazer](https://adamroden.github.io/Gazer/) (sources in [`website/docs/`](website/docs/)).

Contributors and coding agents: start at [AGENTS.md](AGENTS.md). Page XML schema: [docs/page-xml.md](docs/page-xml.md). Command catalog: [src/app/Commands.md](src/app/Commands.md).

Preview the docs locally:

```powershell
pip install -r website/requirements.txt
mkdocs serve -f website/mkdocs.yml
```

---

## Requirements

- Windows
- CMake ≥ 3.21
- Qt 6 (Core, Gui, Widgets, Qml, Quick) — tested with 6.11.1 MinGW
- Optional: [Tobii Stream Engine](https://developer.tobii.com/) headers under `third_party/` (`tobii.h`, `tobii_streams.h`). At runtime Gazer loads `tobii_stream_engine.dll` from the Tobii Experience / Eye Tracking Core install (not copied next to the exe). Without hardware, Gazer uses the mouse tracker.
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

The post-build step copies `resources/` next to `Gazer.exe` and runs `windeployqt`. Stream Engine is not copied (a leftover next to the exe is deleted). Gazer loads it from the Tobii install, or `TOBII_STREAM_ENGINE_DLL` / `TOBII_STREAM_ENGINE_DIR`.

Tests: `cmake --build build --target GazerPageTests` (and `GazerDwellTests`, `GazerSpeechTests`).

### Beta MSI

```powershell
winget install WiXToolset.WiXCLI   # once
.\scripts\build-msi.ps1            # configure, build, stage, MSI
.\scripts\build-msi.ps1 -SkipBuild # reuse existing build\Gazer.exe
```

Output: `dist\Gazer-<version>-beta.msi`. Installs to `Program Files\Gazer\` with Start Menu and desktop shortcuts. Pages ship under `resources\`.

The MSI stamps `uiAccess=true` on the staged exe and Authenticode-signs it so the **Program Files** copy can sit above Task Manager and type/click into elevated windows. `.\build\Gazer.exe` is left without UIAccess so it still launches from the build directory. A real code-signing PFX: `$env:GAZER_SIGN_PFX` and optional `$env:GAZER_SIGN_PFX_PASSWORD`. Without those, the script uses a local self-signed cert (`%LOCALAPPDATA%\Gazer\signing\`) and the MSI trusts it at install time.

Testers still need Tobii drivers for hardware gaze. Without a tracker, use the mouse backend (tray / Settings → tracker).

User-facing install notes (data paths, first launch): [Install](https://adamroden.github.io/Gazer/install/).

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
```

- Live UI is one frameless topmost `QQuickWindow` + `QQuickPaintedItem` (software scene graph, alpha buffer) sized to painted chrome.
- Root chrome is Docked / Drawer / Quit (`PageSession`).
- Mapping profiles (`resources/mappings/default.json`) turn leftover command names into key / mouse / gamepad output. Gamepad needs ViGEmBus (Settings → Assist can download the official setup). `ViGEmClient.dll` ships next to `Gazer.exe`.
- Builtins run first; unknown names fall through to the mapping profile.

Boards are XML only (`resources/layouts/*.xml`). `openPage` / catalog ids open those pages on the live host. Editor F5 previews attach XML copies under `__editor_preview_*` ids so they do not replace the live page.

User-facing action and command catalogs: [Actions](https://adamroden.github.io/Gazer/reference/actions/) and [Commands](https://adamroden.github.io/Gazer/reference/commands/). Schema: [docs/page-xml.md](docs/page-xml.md). Contributor command list: [src/app/Commands.md](src/app/Commands.md).

---

## Actions and commands

When dwell **ends** (look away after blink grace), `PageSession` flashes the cell and `ActionDispatcher::dispatchPage` runs its `PageAction` list in order. Consecutive `ShowLayers` are batched, then applied together. `Gazer.exe --action …` and the `Gazer` named pipe parse the **same** language (`InboundActions` → `dispatchInbound`). After `OpenPage` in one inbound payload, later `ShowLayers` apply to the opened page.

A cell may have one action **attribute** (`send="q"`) or several **child elements** (`<Send value="q"/>`). `actionLoop` repeats until the cell is activated again. `<Phase>` children replace the single-shot list (first activation enters phase 0; leave commits that phase).

| Action | Runtime |
|--------|---------|
| `Send` | `KeyStateManager`: tap (`activate`), `Down` / `Up`, or timed `hold`. Modifiers cycle Up → Down → LockedDown. US punctuation maps through `KeyGlyphs` (transient Shift). Named keys in `KeyboardInjector`. |
| `Command` | `CommandRegistry`: exact builtin, longest prefix, then `resources/mappings/default.json` (`keyTap`, `keyCombo`, `gamepadButton`, …). |
| `MouseLeftClick` / Middle / Right | Click / double / down / up / toggle at the current cursor. |
| `MouseMoveToGaze` / `Mouse*ClickAtGaze` | Arm `MouseDwellMove` (zoom: Settings / `0` / `N` / `-1` / `-2`). |
| `MouseMoveByDirection` / `MouseMoveToPoint` | Nudge or warp now. |
| `OpenPage` / `TogglePage` / `ClosePage` / `CloseAllPages` / `CloseOtherPages` / `HostPage` / `GoBack` | `PageSession::applyNav`. Close All / Close Other also disable ComboMouse. |
| `ShowLayers` | Replace the source page’s visible layer set (root if that page just closed). |
| `Speak` | Windows SAPI canned utterance. |
| `AHK` / `Run` | Local AutoHotkey snippet, or a Python/AHK **file** via `SidecarHost` (path jail: page dir, `%AppData%\Gazer`, `resources/`). |

Composer capture: if the source page is `compose` or a compose live board, `Send` and mapping/modifier `Command`s are consumed by `ComposeUi` (letters into the phrase; `backspace` / `space` / `enter` / `escape` edit or speak). `qwerty_main` still injects into Windows.

Parse: `src/layout/PageActionParse*.cpp`. Dispatch: `src/app/ActionDispatcher.cpp`. Inbound: `src/app/InboundActions.cpp`. Mapping types: `src/input/InputTypes.h`.
