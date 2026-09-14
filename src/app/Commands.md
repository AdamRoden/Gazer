# Commands

`CommandRegistry` runs a **builtin** first; unknown names fall through to `resources/mappings/default.json`.

| Registered in | What |
|---------------|------|
| `Application.cpp` | Shell: quit, editor, preview |
| `AssistCommands.cpp` | Dwell pause, LTS, mag, reticle, dwell-move, click-at-gaze |
| `GazerServices.cpp` | Modifier cycle, click-at-cursor, stop loops |
| `MouseAssistState.cpp` | Mouse pad: nudge, scroll, edge, holds |
| `SettingsCommands.cpp` | `settings.*` live boards, including `settings.speech.*` |
| `SettingsHeadPose.cpp` | `headPose.*` analog maps |
| `ComposeCommands.cpp` | `compose.*` (prefix `compose.removeWord.`) |

When adding a command: register it and add a row here.

## Head pose (`SettingsHeadPose.cpp`)

| Command | Role |
|---------|------|
| `headPose.enabled.toggle` | Master analog maps on/off |
| `headPose.recenter` | Zero yaw, pitch, roll, x, y, and z at the current pose |
| `headPose.chart.axis.<axis>` | Prefix; select graph input (yaw/pitch/roll/x/y/z) |
| `headPose.map.source.<axis>` / `headPose.map.dest.<dest>` | Prefix; editor source/destination |
| `headPose.addMap` | Append a map for the selected axis and open the editor |
| `headPose.edit.<id>` | Prefix; open that map’s editor |
| `headPose.map.enabled.toggle` / `.delete` / `.done` | Editor |
| `headPose.map.point.*` | Curve points (prev/next/add/del/nudge/edit) |
| `headPose.map.curve.scrub` | Gaze-move the selected handle |
| `headPose.map.pickCommand` / `headPose.cmd.<name>` / `headPose.cmdList.next` / `.prev` / `.cancel` | Command dest |
| `headPose.map.commandAt.dec` / `.inc` | Trigger input value |

`toggleDwellSuspend` / `suspendDwell` / `resumeDwell` are different handlers, not aliases.

Script: `gazer.openPage` / `loadPage` / `focusedPageId` (`ScriptHost`, not CommandRegistry). `loadPage` closes the current attached page first.

## Shell (`Application.cpp`)

| Command | Role |
|---------|------|
| `quitApp` | Exit |
| `openPageEditor` | Page designer (`Invocation.pageId` optional) |
| `openPreview` | Head-pose preview |

## Assist (`AssistCommands.cpp`)

| Command | Role |
|---------|------|
| `toggleDwellSuspend` | Flip global dwell pause |
| `suspendDwell` / `resumeDwell` | Set pause on/off |
| `toggleLookToScroll` | Gaze scroll (always place-cursor first) |
| `lts.resume` / `lts.quit` / `lts.reset` | LTS while on |
| `lts.speed.slower` / `lts.speed.faster` | LTS peak speed (1, 5, 10, 20, 40) |
| `lts.cycleMode` | LTS axes: vertical → horizontal → both |
| `toggleMagnifier` | Live lens (exclusive with reticle) |
| `toggleGazeReticle` | Gaze marker (exclusive with magnifier) |
| `toggleGazeMouseFollow` | Cursor follows gaze |
| `toggleComboMouse` | Arm ComboMouse place, or disable |
| `mouseMoveToGaze` | Toggle dwell-to-warp cursor |
| `mouseMoveToGazeClickLoop` | Sticky dwell-move then click |
| `mouseLeftClickAtGaze` / `mouseMiddleClickAtGaze` / `mouseRightClickAtGaze` | Dwell-move then one click |
| `toggleMouseMoveMagPick` | Settings: magnify pick |
| `toggleMouseMoveMagPickCenter` | Mag-pick center on dwell vs screen |
| `toggleMouseMoveMagPickFullScreen` | Mag-pick full-screen zoom |
| `toggleMouseMoveForesight` | Foresight on/off |
| `toggleMouseMoveForesightSecondZoom` | Second zoom inside foresight |

## Input / loops (`GazerServices.cpp`)

| Command | Role |
|---------|------|
| `leftCtrl` / `rightCtrl` / `leftAlt` / `rightAlt` / `leftWin` / `rightWin` / `leftShift` / `rightShift` | Modifier cycle (Up → Down → LockedDown) |
| `releaseModifiers` | Release all modifiers |
| `mouseLeftClick` / `mouseRightClick` / `mouseMiddleClick` | Click at cursor |
| `stopAllActionLoops` | Stop sticky series, click-loop, holds, modifiers |

## Mouse pad (`MouseAssistState.cpp`)

| Command | Role |
|---------|------|
| `mouseLeftDownUp` / `mouseRightDownUp` / `mouseMiddleDownUp` | Toggle button hold |
| `cycleMouseMoveAmount` / `cycleMouseScrollAmount` | Step size |
| `mouseMoveUp` / `Down` / `Left` / `Right` | Nudge by step |
| `mouseScrollUp` / `Down` / `Left` / `Right` | Scroll by step |
| `mouseMoveToTop` / `Bottom` / `Left` / `Right` | Jump to screen edge |

## Composer (`ComposeCommands.cpp`)

| Command | Role |
|---------|------|
| `compose.open` | Open `compose.xml` |
| `compose.speak` | Speak buffer, or stop if busy |
| `compose.stop` | Abort Eleven POST, stop clip, stop SAPI |
| `compose.clear` / `compose.undo` / `compose.redo` | Buffer |
| `compose.backspace` / `compose.deleteWord` | Edit |
| `compose.removeWord.<i>` | Prefix; `i` is visible chip 0–11 |
| `compose.moveEndOfWord.<i>` / `compose.moveStartOfWord.<i>` | Prefix; caret to that chip’s word edge |
| `compose.openVoices` / `compose.openHistory` | Voice list (model, speed, catalog); history |
| `compose.toggleFreestyle` | Switch Speak between topics and Freestyle (saved voices + tags) |
| `compose.saveName` / `compose.cancelName` / `compose.deleteName` | Save, discard, or delete the item being edited and return to Topics/Freestyle |
| `compose.editShowColors` / `compose.editShowIcons` | Switch the edit overlay between color chips and icons |
| `compose.editColor.<i>` / `compose.editIcon.<i>` | Set color or icon on the item being edited |
| `compose.editClearColor` / `compose.editClearIcon` | Clear custom color or icon on the item being edited |
| `compose.voicePreset.<id>` / `compose.editVoicePreset.<id>` / `compose.newVoicePreset` | Apply, rename, or save a Freestyle voice (voice + speed + boost) |
| `compose.editTag.<i>` / `compose.newTag` | Rename or create a saved tag from the composer |
| `history.play.<id>` / `history.restore.<id>` / `history.delete.<id>` | Replay, put the phrase back in the buffer, or drop the row |
| `history.list.next` / `.prev` / `history.list.goto.<n>` | History list: gaze along the track to scroll; commands still page/jump |
| `compose.pin` / `compose.cancelAssign` | Topics: empty buffer no-op; phrase assigns onto the soundboard. Freestyle: save the current voice + speed + boost |
| `compose.editPins` | Edit mode: dwell a pin/topic (or a Freestyle voice/tag) to rename |
| `soundboard.edit.<id>` | Open the pin item editor |
| `soundboard.editTopic.<id>` | Open the topic item editor |
| `soundboard.play.<id>` | Prefix; play a button |
| `soundboard.assign.sb_r_c` | Prefix; create/overwrite a cell |
| `soundboard.topic.<id>` | Prefix; switch topic |
| `soundboard.newTopic` / `soundboard.loadStarters` | Topics |
| `compose.insertTagAt.<i>` | Prefix; insert `savedSpeechTags[i]` |
| `speech.model.sapi` / `.eleven_flash_v2_5` / `.eleven_v3` | Composer engine |
| `speech.voice.<id>` | Prefix; select a voice (percent-encoded id) |
| `speech.voicePreview.<id>` | Prefix; preview that voice without selecting it |
| `speech.preview` | Preview the current voice |
| `speech.fav.toggle` / `speech.fav.toggle.<id>` | Favorite the current or named ElevenLabs voice |
| `speech.voiceList.next` / `.prev` / `.goto.<n>` | Voice list: gaze along the track to scroll; commands still page/jump |
| `speech.gender.all` / `.female` / `.male` | Voice filter |
| `speech.lang.all` / `speech.lang.set.<code>` | Language filter |
| `speech.speed.dec` / `.inc` | Nudge `speechSpeed` 0.1 |
| `speech.volume.dec` / `.inc` | Nudge `speechVolume` 0.5 (1–5× clip boost) |
| `compose.volume.dec` / `.inc` | Nudge Windows master volume by 10% |

While `compose` is top (or a compose live board), `Send` and mapping keys (`backspace`, `space`, `enter`, `escape`, modifiers) are captured and never reach the OS. Caps uses XML `ShowLayers`.

## Settings (`SettingsCommands.cpp`)

Patterns, not every generated name:

| Pattern | Role |
|---------|------|
| `settings.edit.<numericKey>` | Numpad (`dwellMs` / `dwellSequence` / `rapidDwellMs` / `rapidDwellSequence` open the array editor) |
| `settings.nudge.<numericKey>.dec` / `.inc` | Step a numeric setting |
| `settings.edit.dwellSequence` / `.rapidDwellSequence` | Standard / rapid dwell-sequence array boards |
| `settings.numpad.*` | Live numpad keys |
| `settings.array.*` | Sequence editor (`nudge.N`, `edit.N`, `del.N` for N=0..11) |
| `settings.edit.color.<colorKey>` | Open color picker |
| `settings.color.nudge.h` / `.a` / `.edit.a` | Hue / opacity |
| `settings.color.field.left` / `.right` / `.up` / `.down` | Nudge HSV handle on the saturation×value square |
| `settings.color.pickAtGaze` | Arm move-to on the picker (no OS click) |
| `settings.color.eyedropper` | Hide Gazer and sample a screen pixel |
| `settings.color.palette.0`… | Color-picker palette (red→neutral columns, 950→50 down) |
| `settings.color.editHex` / `.save` / `.cancel` | Hex pad |
| `settings.hex.digit.*` / `.backspace` / `.clear` / `.copy` / `.paste` / `.save` / `.cancel` | Hex keys |
| `settings.numpad.copy` / `.paste` | Clipboard on the numeric editor |
| `settings.flash.custom.toggle` | Custom flash color (off = item foreground) |
| `settings.hover.custom.toggle` | Custom hover outline (off = progress color) |
| `settings.progress.*.toggle` / `settings.mouseProgress.*.toggle` | Progress bits (radial / pie / fill) |
| `settings.session.autoCollapse.toggle` / `.startDocked.toggle` / `.layoutAutoClose.toggle` | Session |
| `settings.speech.editKey` / `.clearKey` | Paste-from-clipboard API-key board / wipe DPAPI |
| `settings.speech.key.paste` / `.save` / `.cancel` / `.clear` | Key board |
| `settings.speech.model.sapi` / `.eleven_flash_v2_5` / `.eleven_v3` | Same handlers as `speech.model.*` (ComposeCommands) |
| `settings.dwell.slow` / `.normal` / `.fast` / `.custom` | Speed presets (standard + rapid dwells together) |
| `settings.dwell.custom.save` / `.restore` | Save or restore Custom timings |
| `settings.mag.follow.slow` / `.sticky` / `.smooth` / `.snappy` | Gaze follow (indicator, gaze mouse, live lens) |
| `settings.lts.indicator.filled` / `.hollow` / `.pause` | LTS HUD (`fan`/`orb` aliases) |
| `settings.tracker.auto` / `.mouse` | Tracker pref (restart) |
| `settings.magPickStyle.*.toggle` / `settings.mousePickStyle.*.toggle` | Pick visuals |
| `settings.pickWindow.round` / `.square` | Zoom window shape |
| `settings.pickCenter.gaze` / `.screen` | Zoom window at gaze point or screen center |
| `settings.nudge.themeSaturation.dec` / `.inc` | Saturation 20–100 in five steps (surfaces keep brightness) |
| `settings.theme.source` | Open the color picker for Primary |
| `settings.theme.assign.primary` / `.secondary` | Legacy: select which color inline HSL sliders edit |
| `settings.theme.shade.<family>.1`…`.9` | Assign that Material shade (100–900) to Progress at 60% opacity. Families: primary, complementary, analogous1/2, tertiary1/2 |
| `theme.light` / `.dark` | Light or dark background (keeps the current tint family) |
| `theme.lightTinted` / `.darkTinted` | Legacy: light/dark plus primary tint |
| `theme.brightness.0`…`.4` | Five background shades (how light or dark) |
| `theme.tint.none` / `.primary` / `.complementary` / `.analogous1` / `.analogous2` / `.tertiary1` / `.tertiary2` | Hue washed onto the background shades |
| `settings.reset` | Defaults |

Numeric keys: `dwellMs` / `rapidDwellMs` plus `kIntSpecs` / `kDoubleSpecs` in `AppSettings.cpp`. Color keys: `SettingsUi::kColorKeys` (`hoverColor`, `flashColor`, progress, combo, custom primary/secondary).

`activeState` is the command name (`settings.progress.radial.toggle`, `theme.light`). Live assist uses the feature stem (`lookToScroll`, `dwellSuspend`). `CloseAllPages` / `CloseOtherPages` also disable ComboMouse.

## Mapping-only (`default.json`)

Not builtins. Examples: `backspace`, `tab`, `enter`, `space`, `escape`, `arrow*`, `clearPhrase`, `speakPhrase`, `windowMid` / `windowMax`.
