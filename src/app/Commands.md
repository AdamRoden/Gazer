# Commands

`CommandRegistry` runs a **builtin** first; unknown names fall through to `resources/mappings/default.json`. `compose.*` / `speech.*` / `soundboard.*` / `history.*` / `settings.*` / `headPose.*` / `lookTo.*` skip the Cmd toast.

User-facing catalogs (what happens, every value token): [website Actions](../../website/docs/reference/actions.md), [website Commands](../../website/docs/reference/commands.md). Action language: [docs/page-xml.md](../../docs/page-xml.md).

AHK / CLI / Python is not this table. `Gazer.exe --action …` (and the `Gazer` named pipe) parse the same action language as page XML. Frames are length-prefixed; the host ACKs before dispatch. A hung host fails the ACK and the second process takes over. `<Run>` spawns a script file; the script talks back with `--action`. Several `--action` flags run in order. After `OpenPage` in one payload, `ShowLayers` applies to the opened page (`ActionDispatcher::dispatchInbound`). A second instance with no `--action` sends `raise`. `ping` is ACK-only. `Gazer.exe --guard` is the watchdog (not a client).

| Registered in | What |
|---------------|------|
| `Application.cpp` | Shell: quit, editor, preview, splash play |
| `AssistCommands.cpp` | Dwell pause, look-to maps, mag, reticle, dwell-move, click-at-gaze |
| `GazerServices.cpp` | Modifier cycle, click-at-cursor, stop loops |
| `MouseAssistState.cpp` | Mouse pad: nudge, scroll, edge, holds |
| `SettingsCommands.cpp` | `settings.*` live boards, including `settings.speech.*` |
| `SettingsHeadPose.cpp` | `headPose.*` analog maps |
| `SettingsLookTo.cpp` | `lookTo.edit.*` / `lookTo.map.*` ring editors |
| `ComposeCommands.cpp` | `compose.*` (prefix `compose.removeWord.` / `compose.acceptPrediction.`) |

When adding a command: register it and add a row here.

## Head pose (`SettingsHeadPose.cpp`)

| Command | Role |
|---------|------|
| `headPose.enabled.toggle` | Master analog maps on/off |
| `headPose.recenter` | Zero yaw, pitch, roll, x, y, and z at the current pose |
| `headPose.chart.axis.<axis>` | Prefix; select an unmapped axis on the graph (`yaw` / `pitch` / `roll` / `x` / `y` / `z`) |
| `headPose.chart.map.<id>` | Prefix; select that map’s curve on the graph |
| `headPose.axis.<axis>.add` | Prefix; add a map for that axis and open the editor |
| `headPose.duplicate.<id>` | Prefix; clone that map and open the editor |
| `headPose.addMap` | Append a map for the chart axis and open the editor |
| `headPose.edit.<id>` | Prefix; open that map’s editor in the right column |
| `headPose.map.source.<axis>` / `headPose.map.dest.<dest>` | Prefix; editor source / destination. Choosing a destination returns to the editor |
| `headPose.map.pickOutput` / `headPose.map.outputs.back` | Output list, and back to the editor |
| `headPose.map.invert` | Negate every curve output |
| `headPose.map.enabled.toggle` / `.delete` / `.done` | Per-map enable, remove the map, or close the editor |
| `headPose.map.point.<index>.in.dec` / `.inc` / `.edit` | Nudge or type that point’s input |
| `headPose.map.point.<index>.out.dec` / `.inc` / `.edit` | Nudge or type that point’s output |
| `headPose.map.point.<index>.del` | Remove that point (at least two remain) |
| `headPose.map.point.prev` / `.next` / `.add` / `.del` | Selected point: move the halo, append, or remove |
| `headPose.map.point.in.dec` / `.inc` / `.edit` and `.out.*` | Nudge or type the selected point |
| `headPose.map.curve.scrub` | Gaze-move the selected handle (not wired on the settings graph) |
| `headPose.map.pickCommand` | Open the command list in the right column |
| `headPose.cmd.<name>` | Percent-encoded command; sets the destination to Command and returns to the editor |
| `headPose.cmdList.next` / `.prev` / `.cancel` | Command pages. Cancel returns to the output list |
| `headPose.map.commandAt.dec` / `.inc` | Trigger input for a command map |

`toggleDwellSuspend` / `suspendDwell` / `resumeDwell` are different handlers, not aliases.

## Shell (`Application.cpp`)

| Command | Role |
|---------|------|
| `quitApp` | Exit |
| `rescue.reset` | Panic: stop loops, release keys, disable assist, close pages, dock, restack |
| `openPageEditor` | Page designer (`Invocation.pageId` optional) |
| `openPreview` | Head-pose preview |
| `openLogFolder` | Open `%LOCALAPPDATA%\Gazer\logs` (Setup → Open log folder, tray) |
| `settings.session.showSplash.play` | Run the startup dock tour now |

## Assist (`AssistCommands.cpp`)

| Command | Role |
|---------|------|
| `toggleDwellSuspend` | Flip global dwell pause; 500 ms before the next dwell can start |
| `suspendDwell` / `resumeDwell` | Set pause on/off; same 500 ms start hold |
| `lookToScroll` / `toggleLookToScroll` | Gaze analog scroll (place origin first). Four maps can run at once. |
| `lookToMouse` | Gaze analog mouse move (place origin first) |
| `lookToLeftStick` / `lookToRightStick` | Gaze analog left / right stick (place origin first) |
| `lts.resume` / `lts.quit` / `lts.reset` | Scroll map while on (`lookTo.scroll.*` aliases) |
| `lts.speed.slower` / `lts.speed.faster` | Scroll map peak speed |
| `lts.cycleMode` | Scroll map axes: vertical → horizontal → both |
| `lookTo.mouse.*` / `lookTo.leftStick.*` / `lookTo.rightStick.*` | Same pie actions for the other maps (`.resume` / `.quit` / `.reset` / `.speed.*` / `.cycleMode`) |
| `lookTo.edit.scroll` / `.mouse` / `.leftStick` / `.rightStick` | Open that map’s ring editor |
| `lookTo.map.*` | Ring editor: preview, hub, radii, speed, direction, per-part border and fill |
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
| `compose.stop` | Abort the ElevenLabs stream, stop the clip, stop SAPI |
| `compose.clear` / `compose.undo` / `compose.redo` | Buffer |
| `compose.backspace` / `compose.deleteWord` | Edit |
| `compose.removeWord.<i>` | Prefix; `i` is chip slot 0–11. Pager slots are not words |
| `compose.pageChips.<n>` | Prefix; move the word window by `n` tokens (the chips use ±5) |
| `compose.selectAll` / `compose.cut` / `compose.copy` / `compose.paste` | Phrase selection and the clipboard. Copy uses the selection, or the whole phrase when nothing is selected. Cut needs a selection |
| `compose.acceptPrediction.<i>` | Prefix; dwell suggestion `i` above the phrase. Replaces the word being typed, or inserts the next word |
| `compose.moveEndOfWord.<i>` / `compose.moveStartOfWord.<i>` | Prefix; caret to that chip’s word edge |
| `compose.openVoices` / `compose.openHistory` | Voice list (model, speed, pitch, search, language); history |
| `compose.applyStyle.<name>` / `compose.clearStyle` | Standing ElevenLabs style. `name` is the percent-encoded display name. Empty clears it. The tag is added on the Eleven request only |
| `compose.openStyleEditor` / `compose.closeStyleEditor` | Style list editor: every style, with star and trash. Done returns to the favorite picker |
| `compose.favStyle.<name>` / `compose.trashStyle.<name>` | Star or remove a style. `name` is the percent-encoded display name. Trashing the standing style clears it. The catalog is saved |
| `compose.stylePage.<n>` | Move the style editor by `n` pages |
| `compose.beginStyleName` | Type a new style on the keyboard. Save adds it, starred. Cancel returns to the editor |
| `compose.insertTag.<name>` | Insert `[name]` at the caret and return to Actions. `name` is percent-encoded |
| `compose.tagPage.<n>` | Move the saved-tag page by `n` |
| `compose.beginTagName` | Type a new tag on the keyboard. Save inserts it at the caret, keeps it in the saved list (24), and returns to the tag page |
| `compose.openFiles` / `compose.importChoose` / `compose.importFile` | **Import**. Open the file browser in import/merge mode over Speak and show the keyboard layer. Cancel returns to the layer that was showing |
| `compose.exportBoard` / `compose.exportFile` | **Export**. Same browser in export mode, with the file-name editor |
| `compose.browserPrev` / `compose.browserForward` / `compose.browserUp` | Back, forward, or parent folder. A path crumb uses `compose.browserGoto.<i>`. A Places entry uses `compose.browserPlace.<i>` |
| `compose.browserSave` | **Save** on the export browser. Asks Yes/No, then writes a Voice `aac-workspace` v1 JSON file under the name in the text box |
| `compose.fileCaret.<n>` | Move the export file-name caret by `n` characters. The on-screen arrows use `-1` and `1`. The name scrolls so the caret stays in view |
| `compose.fileYes` / `compose.fileNo` | Accept or dismiss that confirm. Yes is the only step that reads or writes the file. Escape is No |
| `compose.browserBack` | **Cancel**. Close the file browser |
| `compose.fileView.list` / `compose.fileView.tiles` | File browser toolbar. List is the detail columns. Tiles is a grid of icons, eight across and four down. The current view is filled |
| `compose.importPick.<i>` / `compose.importPage.<n>` / `compose.fileRow.<n>` | Open folder `i` or select that JSON file (no read yet), move by `n` pages, or move by `n` rows. A list row is one file. A tile row is eight files |
| `compose.importMerge` / `compose.importReplace` | **Merge** or **Import**. Each asks Yes/No first. Import replaces the board and also applies model, speed, pitch, and volume |
| `compose.saveName` / `compose.cancelName` / `compose.deleteName` | Save, discard, or delete the item being edited and return to the board |
| `compose.editShowColors` | **Color** on the item editor. Opens the settings color picker. Save keeps that color on the item until the name is saved. Cancel discards it |
| `compose.editShowIcons` | **Icon**. Opens a searchable Material Symbols board the same size as the color picker, eight across. Icons come from fonts.google.com when the network works, otherwise a short offline set. The footer is Clear, the current icon, Save, and Cancel. The item does not change until Save |
| `compose.iconSearch` | Dwell **Search**. Shows the Speak keyboard, with Esc in place of Edit. Enter or Esc closes the keyboard and keeps the search |
| `compose.iconPick.<i>` / `compose.iconRow.<n>` | Highlight result `i` (`0` clears it) or move by `n` tile rows. The track and the arrows both move whole rows |
| `compose.iconSave` / `compose.iconCancel` | Apply the highlighted icon, or close the list and keep the previous icon |
| `history.play.<id>` / `history.restore.<id>` / `history.delete.<id>` | Replay, put the phrase back in the buffer, or drop the row |
| `history.list.next` / `.prev` / `history.list.goto.<n>` | History list: after the lock-on ring, looking along the track drags the thumb; commands still page/jump |
| `compose.pin` / `compose.cancelAssign` | Empty buffer no-op; otherwise dwell a cell to save the phrase on the soundboard |
| `compose.editPins` | Keyboard **Edit** key. Dwell a pin or topic to rename |
| `soundboard.edit.<id>` | Open the pin item editor |
| `soundboard.editTopic.<id>` | Open the topic item editor |
| `soundboard.play.<id>` | Prefix; play a button |
| `soundboard.assign.sb_r_c` | Prefix; create/overwrite a cell |
| `soundboard.topic.<id>` | Prefix; switch topic |
| `soundboard.newTopic` / `soundboard.loadStarters` | Topics |
| `speech.model.sapi` / `.eleven_v4_turbo` | Composer engine. `.eleven_flash_v2_5` and `.eleven_v3` also select v4 Turbo. |
| `speech.voice.<id>` | Prefix; select a voice (percent-encoded id) |
| `speech.voicePreview.<id>` | Prefix; preview that voice without selecting it |
| `speech.preview` | Preview the current voice |
| `speech.fav.toggle` / `speech.fav.toggle.<id>` | Favorite the current or named ElevenLabs voice |
| `speech.voiceList.next` / `.prev` / `.goto.<n>` | Voice list: after the lock-on ring, looking along the track drags the thumb; commands still page/jump |
| `speech.gender.all` / `.female` / `.male` | Voice filter |
| `speech.lang.all` / `speech.lang.set.<code>` / `speech.lang.browse` | Language filter. Browse replaces the voice list with every language |
| `speech.voiceSearch` | Type a voice name on the Speak keyboard. The list shrinks so the keys stay visible |
| `speech.speed.dec` / `.inc` | Nudge `speechSpeed` 0.1 |
| `speech.pitch.dec` / `.inc` | Nudge `speechPitch` 0.1 (0.5–2, ElevenLabs only) |
| `speech.volume.dec` / `.inc` | Nudge `speechVolume` 0.5 (1–5× clip boost). The voice list uses these |
| `volume.dec` / `.inc` | Nudge the Speak output device by 10%. That device is Settings → Speak, or the Windows default when none is chosen. Any page. `compose.volume.dec` / `.inc` are the same commands. Speak → Actions and the wide keyboard put a left arrow, a `role="slider"` `caption="volume"` cell, and a right arrow at the ends of the track. The ring fills for `kTrackLockMs` (400 ms) before the thumb moves. Speed is measured from the thumb. Looking at either end of the track moves the thumb there at full speed. A look between the thumb and an end moves it more slowly, so it can come back from an end toward the middle. Hue, opacity, and list scrollbars use that same ring, then the same drag. Vertical tracks have up and down arrows |

While `compose` is top (or a compose live board), `Send` and mapping keys (`backspace`, `space`, `enter`, `escape`, modifiers) are captured and never reach the OS. `volume.dec` / `volume.inc` still nudge the Speak output device. Caps uses XML `ShowLayers`.

## Settings (`SettingsCommands.cpp`)

Patterns, not every generated name:

| Pattern | Role |
|---------|------|
| `settings.edit.<numericKey>` | Numpad (`dwellMs` / `dwellSequence` / `rapidDwellMs` / `rapidDwellSequence` open the array editor; `keyGravity` is Likely keys, 0–100) |
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
| `settings.progress.*.toggle` / `settings.mouseProgress.*.toggle` | Progress bits (radial / pie / fill). Any subset, including none |
| `settings.session.autoCollapse.toggle` / `.startDocked.toggle` / `.showSplash.toggle` / `.showSplash.play` / `.layoutAutoClose.toggle` | Session |
| `settings.speech.predictions.toggle` | Word suggestions on the Speak page |
| `settings.speech.output.<id>` | Prefix. Play speech on that device. Empty id is the Windows default. `<id>` is the percent-encoded device key |
| `settings.speech.output.refresh` | Rescan playback devices on the Speak page |
| `settings.scroll.up` / `.down` | Scroll a one-column settings page by one row |
| `settings.speech.linkKey` | **Create key** on the key board. Clear the clipboard, close Settings, open the ElevenLabs authorize page, and show Click and Cancel (menu width, Click on the left). Click runs `mouseLeftClickAtGaze`. After that click, an `sk_` key on the clipboard is checked and stored. A valid key reopens Settings on Speak |
| `settings.speech.linkCancel` | Leave the create-key page and reopen Settings on Speak |
| `settings.speech.editKey` / `.clearKey` | Open the key board / wipe DPAPI |
| `settings.speech.key.paste` / `.save` / `.cancel` / `.clear` | Key board. Clear deletes the stored key. Save stores a valid key, or an empty key, and closes |
| `settings.speech.model.sapi` / `.eleven_v4_turbo` | Same handlers as `speech.model.*` (ComposeCommands) |
| `settings.dwell.slow` / `.normal` / `.fast` / `.custom` | Dwell presets (standard + rapid dwells together) |
| `settings.dwell.custom.save` / `.restore` | Save or restore Custom timings |
| `settings.mag.follow.slow` / `.sticky` / `.smooth` / `.snappy` | Gaze follow (indicator, gaze mouse, live lens) |
| `settings.tracker.auto` / `.mouse` | Tracker pref (restart) |
| `settings.capture.all` / `.pages` / `.none` | Screen capture: all (documentation), boards only, or boards and overlays omitted |
| `settings.vigem.install` / `.refresh` | Download/launch official ViGEmBus setup; re-read bus + client status |
| `settings.mousePickStyle.*.toggle` | Move-to markers (cursor, dot, crosshair). Any subset, including none. Zoom region uses the same set |
| `settings.pickWindow.round` / `.square` | Zoom window shape |
| `settings.pickCenter.gaze` / `.screen` | Zoom window at gaze point or screen center |
| `settings.nudge.themeSaturation.dec` / `.inc` | Saturation 20–100 in five steps (surfaces keep brightness) |
| `settings.theme.assign.primary` / `.secondary` | Retarget the inline theme-color editor (Primary vs Progress). Shade chips always write Progress |
| `settings.theme.shade.<family>.1`…`.9` | Assign that Material shade (100–900) to Progress at 60% opacity. Families: primary, complementary, analogous1/2, tertiary1/2 |
| `theme.light` / `.dark` | Light or dark background (keeps the current tint family) |
| `theme.lightTinted` / `.darkTinted` | Legacy: light/dark plus primary tint |
| `theme.brightness.0`…`.4` | Five background shades (how light or dark) |
| `theme.tint.none` / `.primary` / `.complementary` / `.analogous1` / `.analogous2` / `.tertiary1` / `.tertiary2` | Hue washed onto the background shades |
| `settings.reset` | Defaults |

Numeric keys: `dwellMs` / `rapidDwellMs` plus `kIntSpecs` / `kDoubleSpecs` in `AppSettings.cpp`. Color keys: `SettingsUi::kColorKeys` (`hoverColor`, `flashColor`, progress, combo, custom primary/secondary).

`activeState` is the command name (`settings.progress.radial.toggle`, `theme.light`). Live assist uses the feature stem (`lookToScroll`, `lookToMouse`, `dwellSuspend`). `CloseAllPages` / `CloseOtherPages` also disable ComboMouse.

## Mapping-only (`default.json`)

Not builtins. Examples: `backspace`, `tab`, `enter`, `space`, `escape`, `arrow*`, `clearPhrase`, `speakPhrase`, `gamepad.a` / `.b` / `.x` / `.y` / `.lb` / `.rb` / `.lt` / `.rt` / `.l3` / `.r3` / `.back` / `.start` / `.guide` / `.dpad.*` / `.ls.*` / `.rs.*` / `.analog.center`. Shipped Mid/Max window cells are AHK (`icon="WindowMid"` / `WindowMax`), not mapping names. The Gamepad board (`example_gamepad`) uses the `gamepad.*` names.
