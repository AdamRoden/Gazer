# Gazer

**Gaze input mapper** for **Tobii Eye Tracker 5** — C++20 / Qt 6.

Maps live gaze into on-screen AAC layouts **and** system / virtual-controller
input. Long-term target: replace **OptiKey + OpenTrack + UCR + AutoHotkey** in
one stack.

Origin design thread: [Gaze Input Mapper: Tobii to Virtual Controllers](https://grok.com/share/c2hhcmQtMw_bf8d0b51-bdf9-484a-9707-d77e7450e240).

## Phases

| Phase | Scope | Status |
|-------|--------|--------|
| **0** | Skeleton: tray, preview, tracker backends | Done |
| **1** | JSON layouts, layout window, dwell activate | Done |
| **2** | Multi-instance layouts + off-screen indicators | Done |
| **3** | Mapping profiles + keyboard/mouse/gamepad injection | Done |
| **4** | QJSEngine, TTS, Look-to-Scroll, magnification | **Done** |

## Status

| Feature | Status |
|--------|--------|
| Live gaze crosshair preview | Done |
| System tray (layout / preview / quit) | Done |
| `TrackerMouse` cursor gaze fallback (~60 Hz) | Done |
| `TrackerTobii` Stream Engine (runtime DLL) | **Done** (needs hardware + Tobii service) |
| `utils/Log.h` | Done |
| JSON layout load (`LayoutLoader` / `LayoutManager`) | **Phase 1** |
| `LayoutWindow` on-screen board | **Phase 1** |
| `DwellStateMachine` hit-test + progress + activate | **Phase 1** |
| Actions: speak/command/script; `loadLayout` / `openLayout` / `closeLayout` | **Phase 2** |
| Multi-instance simultaneous layouts | **Phase 2** |
| Multi-instance boards + gaze-reveal dock | **Phase 2** |
| Mapping profiles (`resources/mappings/*.json`) | **Phase 3** |
| Keyboard / mouse `SendInput` | **Phase 3** |
| Virtual gamepad | Stub (ViGEm later) |
| QJSEngine scripts (`gazer.*` API) | **Phase 4** |
| TTS (Windows SAPI) | **Phase 4** |
| Look-to-Scroll | **Phase 4** |
| Magnifier lens | **Phase 4** |

## Architecture (planned)

```
Tobii / Mock ──► ITracker ──► GazePoint stream
                                │
                    ┌───────────┼───────────┐
                    ▼           ▼           ▼
              LayoutManager  Mapping    (debug)
              + dwell/OSK    profiles   PreviewWindow
                    │           │
                    ▼           ▼
              QJSEngine    Virtual controllers
              scripts      mouse / keys / gamepad
                           (ViGEm-class, etc.)

UI shell: TrayIcon · PreviewWindow · (future layout surface)
```

### Layout JSON (schema v1)

See [`resources/layouts/example_main.json`](resources/layouts/example_main.json).

| Field | Role |
|-------|------|
| `grid` | columns/rows, gap, margin |
| `dwell` | default dwell timing / progress style |
| `items[]` | cell placement + `action` |

**Action types:** `speak` | `loadLayout` | `openLayout` | `closeLayout` | `command` | `script`

**Command mapping** (`resources/mappings/default.json`): each `command` name maps to
an ordered list of outputs — `keyTap`, `keyCombo`, `text`, `mouseClick`,
`mouseMove`, `mouseScroll`, `gamepadButton`, `gamepadAxis`.

Future: layout **instance ids**, cross-instance control, and mapping profiles
that bind gaze regions / dwell events to virtual axes and buttons.

### Gaze coordinates

`GazePoint.x/y` are **Qt virtual-desktop logical pixels** (same space as
`QWidget::mapToGlobal`), not normalized [0, 1].

## Requirements

- Windows 10/11 (primary)
- CMake ≥ 3.21
- C++20 (MSVC 2022 or MinGW as shipped with Qt)
- Qt 6 Widgets (`Core`, `Gui`, `Widgets`)
- Optional later: Tobii Stream Engine under `third_party/tobii` or
  `TOBII_STREAM_ENGINE_DIR`

## Build

### Without Tobii (always works)

```powershell
# MinGW example — adjust paths for your Qt install
$env:PATH = "C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\Ninja;C:\Qt\Tools\CMake_64\bin;" + $env:PATH

cmake -S . -B build -G Ninja `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64" `
  -DGAZER_USE_TOBII=OFF `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_CXX_COMPILER="C:/Qt/Tools/mingw1310_64/bin/g++.exe"

cmake --build build
```

### With Tobii (default ON)

```powershell
cmake -S . -B build -G Ninja `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64" `
  -DGAZER_USE_TOBII=ON `
  -DCMAKE_BUILD_TYPE=Release
```

**Runtime requirements for real gaze:**

1. Tobii Experience / Eye Tracking Core installed (provides `tobii_stream_engine.dll`)
2. Eye Tracker 5 connected and service running
3. Optional: `TOBII_STREAM_ENGINE_DLL` if the DLL is not under  
   `C:\Program Files\Tobii\Tobii EyeX\`

Gazer **loads the DLL at runtime** (no static `.lib`). If load/device open fails,
the app falls back to **TrackerMouse** (cursor as gaze).

Build copies `tobii_stream_engine.dll` next to `Gazer.exe` when found on the
machine.

### Run

```powershell
.\build\Gazer.exe
```

If Qt DLLs are missing, add the Qt `bin` directory to `PATH`, or:

```powershell
cmake --build build --target deploy   # windeployqt, if found
```

## Runtime behavior (Phase 4)

1. Tobii or **Mouse**; load mapping profile; SAPI TTS when available
2. Multi-instance boards + edge switcher + input mapping (Phases 2–3)
3. **Speak** → TTS voice + optional type-through
4. **Assist ★** board: script demos, Look-to-Scroll toggle, magnifier toggle
5. Scripts use global `gazer`: `speak`, `typeText`, `runCommand`, `openLayout`, `log`, …
6. Look-to-Scroll: when ON, gaze near screen edges scrolls/pans (not over boards by default)
7. Magnifier: zoom lens follows gaze

### Script example

```js
gazer.speak('Script says hello');
gazer.typeText('typed-by-script ');
gazer.runCommand('backspace');
```

### Mapping profile sketch

```json
{
  "id": "default",
  "speak": { "alsoType": true },
  "commands": {
    "backspace": [ { "type": "keyTap", "key": "Backspace" } ],
    "gamepadA": [ { "type": "gamepadButton", "button": "A" } ]
  }
}
```

## Project layout

```
Gazer/
  CMakeLists.txt
  README.md
  resources/
    icons/
    layouts/example_main.json  example_more.json
  third_party/tobii/
  src/
    main.cpp
    app/Application.*
    core/     trackers + Stream Engine loader
    layout/   documents, instances, dwell, geometry
    mapping/  MappingLoader  MappingEngine  profiles
    input/    Keyboard  Mouse  VirtualGamepad  InputService
    app/      GazerServices  CommandRegistry  ActionDispatcher  Application
    assist/   PhraseService  Tts  ScriptHost  LookToScroll
    ui/       LayoutWindow  edge switcher  Magnifier  Preview  Tray
```





## License

TBD.
