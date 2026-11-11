# app

Composition root, settings, command dispatch.

| File | Role |
|------|------|
| `Application` | Tracker, tray, preview, editor, overlays over `GazerServices`. Owns `SplashOverlay` and starts it when `showSplash` is on. |
| `GazerServices` | Owns domain services. Header is a façade — include the domain header at the call site |
| `ActionDispatcher` | `PageAction` → commands / clicks / speak. `dispatchInbound` follows the top page after OpenPage so ShowLayers hits the opened board |
| `InboundActions` | Parse `--action` / pipe payload (attribute lines or action elements) |
| `ActionChannel` | Same-user named pipe (`Gazer`). Framed write + `ok` ACK. Hung peer: second instance terminates and takes over. `ping` is ACK-only |
| `GuardApp` | `Gazer.exe --guard`: heartbeat watchdog, crash-loop rescue board, Pause hotkey |
| `CommandRegistry` | Builtin names, then mapping profile. Prefix handlers. `compose.*` / `speech.*` / `soundboard.*` / `history.*` / `settings.*` / `headPose.*` skip the Cmd toast. Catalog: `Commands.md` |
| `ComposeUi` | Gaze composer capture (`tryHandle`), decorate live values, stamp chrome. Composer Speak is `SpeakKind::Composed` (Eleven when configured). Helpers: `ComposeUiInternal.h` |
| `ComposeVoices.cpp` | Voice catalog (model, speed, pitch, name search, languages) |
| `ComposeStyles.cpp` | Standing style picker, editor, and keyboard naming |
| `ComposeTags.cpp` | Insert-tag page and keyboard naming |
| `ComposeSoundboard.cpp` | Pin/assign, topic rail, soundboard grid, Voice workspace import/export |
| `ComposeItemEdit.cpp` | Topic/pin name editor. Color opens the settings picker. Icon opens a searchable Material Symbols board (fonts.google.com, then a short offline set). Search dwell shows the Speak keyboard with Esc in place of Edit. The footer is Clear, the current icon, Save, and Cancel. Tiles scroll by row |
| `ComposeHistory.cpp` | History replay/restore |
| `ComposeFiles.cpp` | Import/merge and export browser, confirm, and file-name caret |
| `ComposeFilesWindow.cpp` | That browser (places, list or tile view, footer). Import/merge has Cancel. Export has caret arrows, Save, and Cancel |
| `ComposeListScroll.cpp` | Scrollbar drag on history, voices, the file list, and the icon tiles, plus the export file-name caret (`GazeTrack`) |
| `GazeRouter` | Gaze sample → session + assist |
| `ActiveStateResolver` | `visibleWhen` / `activeState` keys (`mod.shift`, …) |
| `AppSettings` | Persisted prefs. Factory values on the struct; `defaults()` calls `applyTheme()`; spec tables + nudge/display in `.cpp`; JSON overlay in `AppSettingsIo.cpp` (`loadManaged`: backup, schema migrate, restore); Fluent appearance × accent/progress in `AppSettingsTheme.cpp`. Installer wipe policy: `SettingsInstallPolicy.h` |
| `SettingsUi.h` | Live settings boards (numpad, array, color, hex, speech key, slider) |
| `SettingsUi.cpp` | Ctor, decorate, live attach, keyboard focus |
| `SettingsNumpad.cpp` | Numeric editor |
| `SettingsArrayEditor.cpp` | Standard / rapid dwell-sequence editor |
| `SettingsColorPicker.cpp` | HSV field + hue/opacity sliders, eyedropper, picker palette swatches. Hue and opacity drag with `GazeTrack`; the field stays absolute |
| `SettingsHexEditor.cpp` | Hex color pad (6–8 digits, copy/paste) |
| `SettingsSpeechKey.cpp` | ElevenLabs key board. Create key closes Settings and shows Click / Cancel |
| `SettingsSpeechOutput.cpp` | Speak-page playback devices, one 96px row each, on the page scroller (`settings.speech.output.*`) |
| `SettingsSliderGaze.cpp` | Head-pose curve gaze-scrub |
| `SettingsCommands.cpp` | `settings.*` builtins |
| `SettingsHeadPose.cpp` | Head-pose axis list and in-column editor (`headPose.*`) |
| `SettingsLookTo.cpp` | Look-to map ring editor (`lookTo.edit.*` / `lookTo.map.*`) |
| `SettingsPageBuild.h` | Cell/grid helpers for live boards |
| `SettingsUiInternal.h` | Shared ids / color-axis helpers (SettingsUi TUs only) |
