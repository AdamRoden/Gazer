# app

Composition root, settings, command dispatch.

| File | Role |
|------|------|
| `Application` | Tracker, tray, preview, editor, overlays over `GazerServices` |
| `GazerServices` | Owns domain services. Header is a façade — include the domain header at the call site |
| `ActionDispatcher` | `PageAction` → commands / clicks / speak |
| `CommandRegistry` | Builtin names, then mapping profile. Prefix handlers. `compose.*` / `speech.*` / `soundboard.*` / `history.*` / `settings.speech.*` skip the Cmd toast. Catalog: `Commands.md` |
| `ComposeUi` | Gaze composer capture (`tryHandle`), decorate live values, stamp chrome, system-volume title slider. Composer Speak is `SpeakKind::Composed` (Eleven when configured). Helpers: `ComposeUiInternal.h` |
| `ComposeVoices.cpp` | Voice catalog (model + speed + boost) |
| `ComposeSoundboard.cpp` | Pin/assign, topic rail, soundboard grid |
| `ComposeFreestyle.cpp` | Freestyle voice rail + tag board, saved-voice presets |
| `ComposeItemEdit.cpp` | Unified name/color/icon editor overlay |
| `ComposeHistory.cpp` | History replay/restore |
| `GazeRouter` | Gaze sample → session + assist |
| `ActiveStateResolver` | `visibleWhen` / `activeState` keys (`mod.shift`, …) |
| `AppSettings` | Persisted prefs. Factory values on the struct; `defaults()` bakes Fluent neutrals + `applyTheme()`; spec tables + nudge/display in `.cpp`; JSON overlay in `AppSettingsIo.cpp`; Fluent appearance × Apple system accent in `AppSettingsTheme.cpp` |
| `SettingsUi.h` | Live settings boards (numpad, array, color, hex, speech key, opacity, slider) |
| `SettingsUi.cpp` | Ctor, decorate, live attach, keyboard focus |
| `SettingsNumpad.cpp` | Numeric editor |
| `SettingsArrayEditor.cpp` | Designer / daily-driver dwell-sequence editor |
| `SettingsColorPicker.cpp` | Color draft + theme roles |
| `SettingsOpacity.cpp` | Flash-opacity board |
| `SettingsHexEditor.cpp` | Hex color pad |
| `SettingsSpeechKey.cpp` | ElevenLabs API-key live board |
| `SettingsSliderGaze.cpp` | Gaze-follow sliders |
| `SettingsCommands.cpp` | `settings.*` builtins |
| `SettingsPageBuild.h` | Cell/grid helpers for live boards |
| `SettingsUiInternal.h` | Shared ids / color-axis helpers (SettingsUi TUs only) |
