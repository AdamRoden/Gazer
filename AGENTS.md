# Gazer — agent notes

Gaze-driven AAC and system input for Windows. C++20 / Qt 6. Live UI is one frameless `QQuickWindow`; boards are XML in `resources/layouts/`.

Do **not** start with `README.md` (user manual). Use this file, then the folder map for the task.

## Start here

| Task | Open |
|------|------|
| Page XML schema / actions | `docs/page-xml.md`, then `src/layout/README.md` |
| Parser / hit / session | `src/layout/` — `PageLoader`, `PageHit`, `PageSession*` |
| Settings keys, JSON, editors | `src/app/README.md`, `AppSettings.h`, spec tables in `AppSettings.cpp` |
| Builtins / mapping fallthrough | `src/app/CommandRegistry.*`, `src/assist/AssistCommands.cpp`, `README.md` Commands |
| Assist (LTS, mag, dwell-move) | `src/assist/README.md` |
| Page designer | `src/editor/README.md` |
| Paint / host window / theme | `src/ui/README.md` |
| OS injectors | `src/input/README.md` |
| Mapping profiles | `src/mapping/README.md` |

## Tree

| Path | Role |
|------|------|
| `src/app/` | Shell wiring, settings, command registry, gaze router |
| `src/layout/` | Page AST, XML load/write, dwell, live session |
| `src/assist/` | Magnifier, LTS, combo mouse, scripts, TTS |
| `src/editor/` | Qt Widgets page designer |
| `src/ui/` | Host window, board paint, overlays, theme |
| `src/input/` | Keyboard / mouse / scroll / gamepad inject |
| `src/core/` | `ITracker`, Tobii + mouse backends |
| `src/mapping/` | JSON command → input profiles |
| `resources/layouts/` | Shipped Page XML (filename stem = catalog id) |
| `tests/` | Qt Test binaries (`GazerPageTests`, `GazerDwellTests`) |

## Naming traps

- `PageDim` **struct** is in `layout/PageTypes.h`. `layout/PageDim.h` is parse / `placeRect` only.
- `GazerServices.h` and `PageSession.h` are façades. Include `assist/LookToScroll.h`, `ui/PageHostWindow.h`, `ui/Theme.h`, etc. at the call site — do not expect those types from the façade.
- `SettingsUi` methods are split by board: `SettingsUi.cpp` (shared), `SettingsNumpad.cpp`, `SettingsArrayEditor.cpp`, `SettingsColorPicker.cpp`, `SettingsOpacity.cpp`, `SettingsHexEditor.cpp`, `SettingsSliderGaze.cpp`, `SettingsCommands.cpp`. Helpers: `SettingsPageBuild.h`, `SettingsUiInternal.h`.
- Keyboard XML cell ids like `ch_113_0_1` are codepoints, not key names.

## Do not read

These dump context and almost never help a code change:

- `resources/icons/key_symbols.json` (~319 KB path data; `ui/KeySymbols.cpp` loads it at runtime)
- `resources/icons/*.png`, `resources/models/head.obj`
- `third_party/tobii/**` (vendor headers)
- `dist/`, `build/`, `gazer.log`
- `.gitignore` (stock Visual Studio template)

## Build

```powershell
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64" -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Gazer
cmake --build build --target GazerPageTests
```

MinGW `bin` on `PATH`. One Ninja version per tree. Includes are `"layout/PageLoader.h"` from `src/`.

User-facing install / MSI / tray: `README.md`.
