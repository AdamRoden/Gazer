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
| `ComposeUi` | Gaze composer capture (`tryHandle`), decorate live values, stamp chrome, system-volume title slider. Composer Speak is `SpeakKind::Composed` (Eleven when configured). Helpers: `ComposeUiInternal.h` |
| `ComposeVoices.cpp` | Voice catalog (model + speed + boost) |
| `ComposeSoundboard.cpp` | Pin/assign, topic rail, soundboard grid |
| `ComposeFreestyle.cpp` | Freestyle voice rail + tag board, saved-voice presets |
| `ComposeItemEdit.cpp` | Unified name/color/icon editor overlay |
| `ComposeHistory.cpp` | History replay/restore |
| `ComposeListScroll.cpp` | Gaze-follow scrollbar on history / voices live boards |
| `GazeRouter` | Gaze sample → session + assist |
| `ActiveStateResolver` | `visibleWhen` / `activeState` keys (`mod.shift`, …) |
| `AppSettings` | Persisted prefs. Factory values on the struct; `defaults()` calls `applyTheme()`; spec tables + nudge/display in `.cpp`; JSON overlay in `AppSettingsIo.cpp`; Fluent appearance × accent/progress in `AppSettingsTheme.cpp` |
| `SettingsUi.h` | Live settings boards (numpad, array, color, hex, speech key, slider) |
| `SettingsUi.cpp` | Ctor, decorate, live attach, keyboard focus |
| `SettingsNumpad.cpp` | Numeric editor |
| `SettingsArrayEditor.cpp` | Standard / rapid dwell-sequence editor |
| `SettingsColorPicker.cpp` | HSV field + hue/opacity sliders, eyedropper, picker palette swatches |
| `SettingsHexEditor.cpp` | Hex color pad (6–8 digits, copy/paste) |
| `SettingsSpeechKey.cpp` | ElevenLabs API-key live board |
| `SettingsSliderGaze.cpp` | Head-pose curve gaze-scrub |
| `SettingsCommands.cpp` | `settings.*` builtins |
| `SettingsHeadPose.cpp` | Head-pose maps live board (`headPose.*`) |
| `SettingsLookTo.cpp` | Look-to map ring editor (`lookTo.edit.*` / `lookTo.map.*`) |
| `SettingsPageBuild.h` | Cell/grid helpers for live boards |
| `SettingsUiInternal.h` | Shared ids / color-axis helpers (SettingsUi TUs only) |
