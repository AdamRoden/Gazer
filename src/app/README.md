# app

Composition root, settings, command dispatch.

| File | Role |
|------|------|
| `Application` | Tracker, tray, preview, editor, overlays over `GazerServices` |
| `GazerServices` | Owns domain services. Header is a façade — include the domain header at the call site |
| `ActionDispatcher` | `PageAction` → commands / clicks / speak |
| `CommandRegistry` | Builtin names, then mapping profile. Catalog: `Commands.md` |
| `GazeRouter` | Gaze sample → session + assist |
| `ActiveStateResolver` | `visibleWhen` / `activeState` keys (`mod.shift`, …) |
| `AppSettings` | Persisted prefs. Spec tables + nudge/display in `.cpp`; JSON in `AppSettingsIo.cpp`; palette in `AppSettingsTheme.cpp` |
| `SettingsUi.h` | Live settings boards (numpad, array, color, hex, opacity, slider) |
| `SettingsUi.cpp` | Ctor, decorate, live attach, keyboard focus |
| `SettingsNumpad.cpp` | Numeric editor |
| `SettingsArrayEditor.cpp` | Dwell-sequence editor |
| `SettingsColorPicker.cpp` | Color draft + theme roles |
| `SettingsOpacity.cpp` | Flash-opacity board |
| `SettingsHexEditor.cpp` | Hex color pad |
| `SettingsSliderGaze.cpp` | Gaze-follow sliders |
| `SettingsCommands.cpp` | `settings.*` builtins |
| `SettingsPageBuild.h` | Cell/grid helpers for live boards |
| `SettingsUiInternal.h` | Shared ids / color-axis helpers (SettingsUi TUs only) |
