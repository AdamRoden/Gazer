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
| `ComposeBorrow.cpp` | Borrowed Speak keyboard for Settings search, and the Settings import/export strip. Search keyboards and that strip use Esc and Ctrl. Import and export share the file-browser height |
| `ComposeVoices.cpp` | Voice catalog (model, speed, pitch, languages, voice list) |
| `ComposeSearch.cpp` | Keyboard capture for the voice, history, style, tag, and favorite search sheets |
| `ComposeForm.cpp` | Topic and phrase edit sheet. Settings-style rows: name, value, and an edit icon. Rows, columns, and index are − / value / +. Footer is Delete, Export, Save, and Cancel |
| `ComposeFieldEdit.cpp` | One-row popup for a name or phrase, the height of that field. Save, Cancel, and Clear |
| `ComposeRail.cpp` | Favorites sidebar and the add-favorite tiles |
| `ComposeStyles.cpp` | Standing style picker, editor, and keyboard naming |
| `ComposeTags.cpp` | Insert-tag page and keyboard naming |
| `ComposeSoundboard.cpp` | Pin/assign, topic rail, soundboard grid, Voice workspace import/export |
| `ComposeItemEdit.cpp` | Topic/pin name editor. Color opens the settings picker. Icon opens a searchable Material Symbols board (fonts.google.com, then a short offline set). Search dwell shows the Speak keyboard with Esc and Ctrl in place of Edit and Actions. The footer is Clear, the current icon, Save, and Cancel. Tiles scroll by row |
| `ComposeHistory.cpp` | History replay/restore. Phrase **Export** saves that clip as an MP3 |
| `ComposeFiles.cpp` | Import/merge and export browser, confirm, and file-name caret |
| `ComposeFilesWindow.cpp` | That browser (places, list or tile view, footer). Import/merge has Cancel. Export has caret arrows, Save, and Cancel. Phrase audio adds Assign and Clear |
| `ComposeListScroll.cpp` | Scrollbar drag on history, voices, styles, tags, favorites, the file list, and the icon tiles |
| `GazeRouter` | Gaze sample → session + assist |
| `ActiveStateResolver` | `visibleWhen` / `activeState` keys (`mod.shift`, …) |
| `AppSettings` | Persisted prefs. Factory values on the struct; `defaults()` calls `applyTheme()`; spec tables + nudge/display in `.cpp`; JSON snapshot in `AppSettingsIo.cpp` (`toJsonObject`, `applyJsonObject`, `resetKeysToDefaults`; `loadManaged`: backup, schema migrate, restore); Fluent appearance × accent/progress in `AppSettingsTheme.cpp`. Every install kind keeps `settings.json`: `SettingsInstallPolicy.h` |
| `SettingsCatalog` | View-defaults rows (id, page, title, description, JSON keys), `fillSettingsDefaultsGrid`, and import classification |
| `UpdateRelease` | GitHub release JSON → newest `Gazer-X.Y.Z.msi` / `Gazer-X.Y.Z-beta.msi` |
| `AppUpdater` | Routine update check, background MSI download, restart handoff to `GazerUpdate.exe` |
| `SettingsDefaults.cpp` | View defaults filter menu, Speak keyboard search, Speak file import/export, update commands. The list is `fillSettingsDefaultsGrid` |
| `SettingsDefaultsEdit.cpp` | View defaults Edit: numpad, color picker, inline on/off switch, choice pad, or the settings board |
| `SettingsUi.h` | Live settings boards (numpad, array, color, hex, speech key, slider) |
| `SettingsUi.cpp` | Ctor, decorate, live attach, keyboard focus |
| `SettingsNumpad.cpp` | Numeric editor |
| `SettingsArrayEditor.cpp` | Standard / rapid dwell-sequence editor |
| `SettingsColorPicker.cpp` | HSV field + hue/opacity sliders, eyedropper, picker palette swatches. Hue and opacity drag with `GazeTrack`; the field stays absolute |
| `SettingsHexEditor.cpp` | Hex color pad (6–8 digits, copy/paste) |
| `SettingsSpeechKey.cpp` | ElevenLabs key board. Create key closes Settings and shows Click / Cancel |
| `SettingsSpeechOutput.cpp` | Speak-page playback devices, one 96px row each, on the page scroller (`settings.speech.output.*`) |
| `SettingsSliderGaze.cpp` | Head-pose curve gaze-scrub |
| `SettingsCommands.cpp` | `settings.*` builtins. Calls `registerDefaultsCommands` and `registerUpdateCommands` |
| `SettingsHeadPose.cpp` | Head-pose axis list and in-column editor (`headPose.*`) |
| `SettingsLookTo.cpp` | Look-to map ring editor (`lookTo.edit.*` / `lookTo.map.*`) |
| `SettingsPageBuild.h` | Cell/grid helpers for live boards |
| `SettingsUiInternal.h` | Shared ids / color-axis helpers (SettingsUi TUs only) |
