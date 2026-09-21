# Gazer — agent notes

Gaze-driven system input for Windows. C++20 / Qt 6. Live UI is one frameless `QQuickWindow`; boards are XML in `resources/layouts/`.

Do **not** start with `README.md` or `website/docs/` (user manual). Use this file, then the folder map for the task.

## Live z-order (front → back)

This is load-bearing. Wrong HWND stacking or paint order looks like “missing” chrome, mag covering the drawer, or flashing.

**HWND band** (always `WS_EX_TOPMOST`, above the taskbar and other apps, including fullscreen). Never `HWND_NOTOPMOST` — that hop flashes the desktop. Restack only when a foreign window occludes Gazer or Gazer-internal order is wrong (`WinOverlay::restackGazerBand`). `OverlayStackWatch` hooks other-process foreground/move/minimize and polls so other apps cannot sit in front of Gazer. Exclusive-mode fullscreen can still win until it yields; we reassert TOPMOST then.

Task Manager (and other system-tools windows) sit in an OS band above ordinary TOPMOST. The installed MSI restamps `Gazer.exe` with `uiAccess="true"` and Authenticode-signs it so `HWND_TOPMOST` lands in the UIAccess band. `.\build\Gazer.exe` stays `uiAccess="false"` so it still starts from the build dir. `WinOverlay::processHasUiAccess` is the runtime check. Signing: `scripts/GazerSign.ps1`, called from `scripts/build-msi.ps1`.

| Front | HWND |
|-------|------|
| 1 | `GazeReticle` |
| 2 | `MagnifierOverlay` (live lens) |
| 3 | `MouseDwellMove` mag-pick |
| 4 | Other assist overlays (ComboMouse, LTS ring, dwell-move cursor, dwell-suspend) |
| 5 | Single `PageHostWindow` (all boards) |

Do **not** split master chrome into a second HWND. Mag overlays stay in front of the master page.

**Inside `PageHostWindow`**, front → back (paint/hit back-to-front; hit-test walks reverse). Each page is one layer (its grids and cells stay together). A cell on a buried grid does not come forward when dwelled; only the unoccluded part hit-tests.

1. Zones on the main master page
2. Grids / cells on the main master page
3. Each open page, newest in front: that page’s zones, then its grids / cells

Do not `raise()` / `HWND_TOP` an overlay on every gaze sample. `showOverlay()` is a no-op when already visible.

## Start here

| Task | Open |
|------|------|
| User docs (GitHub Pages) | `website/docs/` (`mkdocs.yml`, workflow `.github/workflows/pages.yml`) |
| Page XML schema / actions | `docs/page-xml.md`, then `src/layout/README.md` |
| Parser / hit / session | `src/layout/` — `PageLoader`, `PageHit`, `PageSession*` |
| Settings keys, JSON, editors | `src/app/README.md`, `AppSettings.h`, spec tables in `AppSettings.cpp`. Speed: standard `dwellSequence` vs rapid `rapidDwellSequence`; shared `scanGraceMs`. Head-pose maps: `headPoseMaps` + `SettingsHeadPose.cpp` |
| Builtins / mapping fallthrough | `src/app/Commands.md`, then the file listed in that table |
| Assist (look-to maps, mag, dwell-move) | `src/assist/README.md` |
| Composer / speech | `src/app/ComposeUi.h`, `src/assist/SpeechEngine.h`, `src/app/Commands.md`. `docs/composer-elevenlabs.md` is a frozen 2026-09-03 spec — live behavior is Commands.md / shipped XML. |
| Page designer | `src/editor/README.md` |
| Paint / host window / theme | `src/ui/README.md`. Startup dock tour: `ui/SplashOverlay` (`showSplash` setting, Assist overlay) |
| Material palettes (theme) | `src/ui/MaterialPalette.h`, `resources/layouts/main_settings_theme.xml` |
| OS injectors | `src/input/README.md` |
| Mapping profiles | `src/mapping/README.md` (command JSON). Analog head maps: `HeadPoseCurve` / `assist/HeadPoseMapper` |

## Tree

| Path | Role |
|------|------|
| `src/app/` | Shell wiring, settings, command registry, gaze router |
| `src/layout/` | Page AST, XML load/write, dwell, live session |
| `src/assist/` | Magnifier, LTS, combo mouse, TTS, head-pose analog maps |
| `src/editor/` | Qt Widgets page designer |
| `src/ui/` | Host window, board paint, overlays, theme |
| `src/input/` | Keyboard / mouse / scroll / gamepad inject |
| `src/core/` | `ITracker`, Tobii + mouse backends |
| `src/mapping/` | JSON command → input profiles; `HeadPoseCurve` analog eval |
| `resources/layouts/` | Shipped Page XML (filename stem = catalog id) |
| `packaging/` | MSI (`Gazer.wxs`), `Gazer.exe.manifest.in` (uiAccess), cert-trust cmd |
| `tests/` | Qt Test binaries (`GazerPageTests`, `GazerDwellTests`, `GazerSpeechTests`) |

## Naming traps

- `PageDim` **struct** is in `layout/PageTypes.h`. `layout/PageDim.h` is parse / `placeRect` only.
- `GazerServices.h` and `PageSession.h` are façades. Include `assist/LookToScroll.h`, `ui/PageHostWindow.h`, `ui/Theme.h`, etc. at the call site — do not expect those types from the façade.
- `SettingsUi` methods are split by board: `SettingsUi.cpp` (shared), `SettingsNumpad.cpp`, `SettingsArrayEditor.cpp`, `SettingsColorPicker.cpp`, `SettingsHexEditor.cpp`, `SettingsSpeechKey.cpp`, `SettingsSliderGaze.cpp`, `SettingsCommands.cpp`, `SettingsHeadPose.cpp`, `SettingsLookTo.cpp`. Helpers: `SettingsPageBuild.h`, `SettingsUiInternal.h`.
- `ComposeUi` methods are split by board: `ComposeUi.cpp` (capture + chrome stamp/decorate), `ComposeSoundboard.cpp`, `ComposeFreestyle.cpp`, `ComposeItemEdit.cpp`, `ComposeVoices.cpp`, `ComposeHistory.cpp`. Helpers: `ComposeUiInternal.h`.
- `AppSettings` JSON is `AppSettingsIo.cpp`; theme palette is `AppSettingsTheme.cpp`.
- Drawer motion: `PageSessionChrome.cpp`. Quit is master XML `ShowLayers`. Gaze/dwell/auto-close: `PageSessionGaze.cpp`.
- Head-preview math/shaders: `ui/PreviewGeometry.*`. GL draw is `ui/HeadPreviewRenderer` (tray `PreviewWindow` and the Head settings cell). Page XML action tests: `tests/PageLoaderActionTest.cpp`. Live hit tests: `tests/PageHitLiveTest.cpp`.
- Editor inspector forms: `LayoutEditorPropertiesFill.cpp`, `LayoutEditorFieldsGroups.cpp`. File dialogs: `LayoutEditorWindowFile.cpp`.
- Keyboard XML cell ids like `ch_113_0_1` are codepoints, not key names.

## Do not read

These dump context and almost never help a code change:

- `resources/icons/svg/*.svg` (icon catalog; `ui/KeySymbols.cpp` loads stems at runtime)
- `resources/icons/*.png`, `resources/models/head.obj`
- `third_party/tobii.h`, `third_party/tobii_streams.h` (vendor ABI mirrors)
- `dist/`, `build/`, `gazer.log`
- `.gitignore` (stock Visual Studio template)

## Build

```powershell
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="C:/Qt/6.11.1/mingw_64" -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Gazer
cmake --build build --target GazerPageTests
cmake --build build --target GazerDwellTests
cmake --build build --target GazerSpeechTests
```

MinGW `bin` on `PATH`. One Ninja version per tree. Includes are `"layout/PageLoader.h"` from `src/`.

User-facing install / MSI / tray: `website/docs/`. Contributor build / architecture: `README.md`.
