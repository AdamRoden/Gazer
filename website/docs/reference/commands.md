# Commands

A cell with `command="…"` (or `<Command value="…"/>`) runs a **name**. Builtins run first; unknown names fall through to `resources/mappings/default.json`. How that sits next to `Send`, mouse actions, and page navigation: [Actions](actions.md).

`Gazer.exe --action command=…` (and the `Gazer` named pipe) use the same names. Inline AutoHotkey and script files are `AHK` / `Run` actions; those processes call back with `--action`.

`compose.*` / `speech.*` / `soundboard.*` / `history.*` / `settings.*` / `headPose.*` / `lookTo.*` skip the `Cmd …` toast.

## How a name is resolved

1. **Exact builtin.** Registered at startup (shell, assist, mouse pad, settings, composer, head pose, look-to editors).
2. **Longest prefix.** Names such as `compose.removeWord.3` or `speech.voice.<id>` hit a prefix handler. The suffix is part of the name.
3. **Mapping profile.** `resources/mappings/default.json` next to `Gazer.exe` maps leftover names to a list of inject steps. Those steps run in order through `InputService` (keyboard, mouse, virtual Xbox pad).

Unknown names fail (`Command failed: …`). `Invocation.pageId` is the cell’s source page (used by a few builtins such as `openPageEditor`).

Contributor catalog (file names, register-here rule): [`src/app/Commands.md`](https://github.com/AdamRoden/Gazer/blob/master/src/app/Commands.md).

## Mapping JSON (fallthrough)

Each mapping name is an array of objects. Types:

| `type` | Fields | What happens |
|--------|--------|----------------|
| `keyTap` | `key` | Same as a `Send` tap of that key (modifier names cycle). |
| `keyCombo` | `keys` (array; last is the main key) | Hold the leading keys, tap the last, release. |
| `text` | `value` | Unicode string, then release one-shot modifiers. |
| `mouseClick` / `mouseDoubleClick` / `mouseDown` / `mouseUp` | `button` (`left` / `right` / `middle`; empty = left) | At the current cursor. |
| `mouseMove` | `dx`, `dy` | Relative pixels. |
| `mouseMoveTo` | `dx`, `dy` as absolute x,y | Warp. |
| `mouseScroll` / `mouseScrollH` | `notches` | Vertical / horizontal wheel. |
| `gamepadButton` | `button`: `a` `b` `x` `y` `lb` `rb` `l3` `r3` `back` `start` `guide` `dpadUp` `dpadDown` `dpadLeft` `dpadRight` | Press then release on the virtual Xbox pad. Triggers use `gamepadAxis` (`lt` / `rt`). |
| `gamepadAxis` | `axis` (`lx` `ly` `rx` `ry` `lt` `rt`), `axisValue` (−1…1, or 0…1 for triggers) | Set that axis; it **stays** until another axis command. |

Gamepad needs [ViGEmBus](https://github.com/nefarius/ViGEmBus). Settings → Setup can install the bus. Gazer ships `ViGEmClient.dll` next to `Gazer.exe`.

### Mapping-only names in `default.json`

These names live in the mapping profile. Keyboards use the key names; the Gamepad board uses `gamepad.*`.

| Name | Inject |
|------|--------|
| `backspace` `tab` `enter` `space` `escape` `delete` | `keyTap` of that key |
| `home` `end` `pageUp` `pageDown` | `keyTap` |
| `arrowUp` `arrowDown` `arrowLeft` `arrowRight` | `keyTap` Up/Down/Left/Right |
| `clearPhrase` | Ctrl+A then Backspace |
| `speakPhrase` | Ctrl+Enter |
| `gamepad.a` `.b` `.x` `.y` `.lb` `.rb` `.l3` `.r3` `.back` `.start` `.guide` | Face / shoulder / stick-click / View / Menu / Xbox |
| `gamepad.dpad.up` `.down` `.left` `.right` | D-pad tap |
| `gamepad.lt` `.rt` | Trigger axis to 1.0 (stays until `gamepad.analog.center` or another axis) |
| `gamepad.ls.n` `.s` `.e` `.w` `.ne` `.nw` `.se` `.sw` | Left stick hat (cardinals ±1; diagonals ±0.707) |
| `gamepad.rs.*` | Same for the right stick |
| `gamepad.analog.center` | Zero lx, ly, rx, ry, lt, rt |

Shipped Mid/Max window cells are `<AHK>` (`icon="WindowMid"` / `WindowMax`), not mapping names.

---

## Shell

| Command | What happens |
|---------|----------------|
| `quitApp` | Exit the process (drawer Quit → Yes). |
| `rescue.reset` | Panic reset: stop loops, release modifiers, turn off look-to / ComboMouse / mag / follow, close pages, dock, restack. Dock **Rescue** chip. Pause key on the guard process. |
| `openPageEditor` | Open the XML page designer. Optional `Invocation.pageId`. |
| `openPreview` | Head-pose 3D preview window. |
| `openLogFolder` | Open the log folder (`%LOCALAPPDATA%\Gazer\logs`). Settings → Setup → **Open log folder**, and the tray menu. |
| `settings.session.showSplash.play` | Run the startup dock tour now (Settings → Setup → Play tour). |

---

## Dwell pause

`toggleDwellSuspend`, `suspendDwell`, and `resumeDwell` are **three handlers**, not aliases.

| Command | What happens |
|---------|----------------|
| `toggleDwellSuspend` | Flip global dwell pause. |
| `suspendDwell` | Pause on. |
| `resumeDwell` | Pause off. Sleep / Main use this with `ShowLayers`. |

After any of them, a **500 ms** hold runs before a new dwell can start. `suspendExempt` cells (Main, Sleep) stay dwellable while paused. `suspendDwell` also stops the layout idle auto-close timer; `resumeDwell` restarts it from zero.

---

## Assist

| Command | What happens |
|---------|----------------|
| `toggleMagnifier` | Live lens. Exclusive with the gaze reticle. |
| `toggleGazeReticle` | Gaze marker. Exclusive with the magnifier. |
| `toggleGazeMouseFollow` | Cursor follows gaze. |
| `lookToScroll` / `toggleLookToScroll` | Gaze analog **scroll**. Dwell to place an origin, then gaze vs the ring drives wheel. Four look-to maps can run at once. |
| `lookToMouse` | Same analog disk, pointer velocity. |
| `lookToLeftStick` / `lookToRightStick` | Same analog disk, virtual Xbox stick (ViGEm). |
| `lts.resume` / `lts.quit` / `lts.reset` | Scroll map while on (`lookTo.scroll.*` aliases). |
| `lts.speed.slower` / `lts.speed.faster` | Scroll peak speed (0.5–8 notches/sec). |
| `lts.cycleMode` | Scroll axes: vertical → horizontal → both. |
| `lookTo.mouse.*` / `lookTo.leftStick.*` / `lookTo.rightStick.*` | Same pie actions (`.resume` / `.quit` / `.reset` / `.speed.*` / `.cycleMode`). |
| `lookTo.edit.scroll` / `.mouse` / `.leftStick` / `.rightStick` | Open that map’s ring editor. |
| `lookTo.map.*` | Ring editor: preview, hub, radii, speed, direction, per-part border and fill. |
| `toggleComboMouse` | Arm ComboMouse place, or disable. |
| `mouseMoveToGaze` | Toggle dwell-to-warp cursor. |
| `mouseMoveToGazeClickLoop` | Sticky dwell-move then click until stopped. |
| `mouseLeftClickAtGaze` / `mouseMiddleClickAtGaze` / `mouseRightClickAtGaze` | Dwell-move then one click. |
| `toggleMouseMoveMagPick` | Settings: magnify pick on/off. |
| `toggleMouseMoveMagPickCenter` | Mag-pick center on dwell vs screen. |
| `toggleMouseMoveMagPickFullScreen` | Mag-pick full-screen zoom. |

XML click-at-gaze actions can pin a zoom token per cell; these commands use Settings.

---

## Clicks, modifiers, loops

| Command | What happens |
|---------|----------------|
| `leftCtrl` / `rightCtrl` / `leftAlt` / `rightAlt` / `leftWin` / `rightWin` / `leftShift` / `rightShift` | Modifier cycle Up → Down → LockedDown → Up (OS key down/up). `activeState="mod.shift"` (and `.ctrl` / `.win` / `.alt`; `.locked` variants). |
| `releaseModifiers` | Release all modifier slots. |
| `mouseLeftClick` / `mouseRightClick` / `mouseMiddleClick` | One click at the current cursor. |
| `stopAllActionLoops` | Stop XML `actionLoop` series, the gaze click loop, button holds, and modifiers. |

---

## Mouse pad

| Command | What happens |
|---------|----------------|
| `mouseLeftDownUp` / `mouseRightDownUp` / `mouseMiddleDownUp` | Toggle button hold (`activeState="mouse.leftHold"` …). |
| `cycleMouseMoveAmount` / `cycleMouseScrollAmount` | Step size for nudge / scroll cells. |
| `mouseMoveUp` / `Down` / `Left` / `Right` | Nudge by the move step. |
| `mouseScrollUp` / `Down` / `Left` / `Right` | Wheel by the scroll step. |
| `mouseMoveToTop` / `Bottom` / `Left` / `Right` | Jump to that screen edge. |

XML `mouseMoveByDirection` is the compass form of the nudge commands.

---

## Composer

| Command | What happens |
|---------|----------------|
| `compose.open` | Open `compose.xml`. |
| `compose.speak` | Speak the buffer, or stop if busy. |
| `compose.stop` | Abort the ElevenLabs stream, stop the clip, stop SAPI. |
| `compose.clear` / `compose.undo` / `compose.redo` | Buffer. |
| `compose.backspace` / `compose.deleteWord` | Edit (rapid dwell). |
| `compose.removeWord.<i>` | Prefix; `i` is chip slot 0–11. Pager slots are not words. |
| `compose.pageChips.<n>` | Move the word window by `n` tokens. The chips use ±5. |
| `compose.selectAll` / `compose.cut` / `compose.copy` / `compose.paste` | Phrase selection and the clipboard. |
| `compose.acceptPrediction.<i>` | Prefix; dwell suggestion `i` above the phrase. |
| `compose.moveEndOfWord.<i>` / `compose.moveStartOfWord.<i>` | Caret to that chip’s word edge. |
| `compose.openVoices` / `compose.openHistory` | Voice list; history. |
| `compose.applyStyle.<name>` / `compose.clearStyle` | Standing ElevenLabs style. `name` is percent-encoded. The tag is added on the Eleven request only. |
| `compose.openStyleEditor` / `compose.closeStyleEditor` | Edit the style list (every style, star, trash). Done returns to the favorites. |
| `compose.favStyle.<name>` / `compose.trashStyle.<name>` | Star or remove a style. Trashing the standing style clears it. |
| `compose.stylePage.<n>` | Move the style editor by `n` pages. |
| `compose.beginStyleName` | Type a new style on the keyboard. Save adds it, already starred. |
| `compose.insertTag.<name>` | Insert `[name]` at the caret and return to Actions. `name` is percent-encoded. |
| `compose.tagPage.<n>` | Move the saved-tag page by `n`. |
| `compose.beginTagName` | Type a new tag on the keyboard. Save inserts it and keeps it in the saved list. |
| `compose.openFiles` / `compose.importChoose` / `compose.importFile` | **Import**. Open the file browser in import/merge mode over Speak and show the keyboard. Cancel returns to the layer that was showing. |
| `compose.exportBoard` / `compose.exportFile` | **Export**. Same browser in export mode, with the file-name editor. |
| `compose.browserPrev` / `compose.browserForward` / `compose.browserUp` | Back, forward, or the parent folder. A path crumb runs `compose.browserGoto.<i>`. A Places entry runs `compose.browserPlace.<i>`. |
| `compose.browserSave` | **Save** on the export browser. Asks Yes/No, then writes an `aac-workspace` v1 file using the name in the text box. |
| `compose.fileCaret.<n>` | Move the export file-name caret by `n` characters. The arrows use `-1` and `1`. The name scrolls so the caret stays in view. |
| `compose.browserBack` | **Cancel**. Close the file browser. |
| `compose.fileView.list` / `compose.fileView.tiles` | File browser toolbar. List is the detail columns. Tiles is a grid of icons, eight across and four down. The current view is filled. |
| `compose.importPick.<i>` / `compose.importPage.<n>` / `compose.fileRow.<n>` | Open folder `i` or select that JSON file (no read yet), move by `n` pages, or move by `n` rows. A list row is one file. A tile row is eight files. |
| `compose.fileYes` / `compose.fileNo` | Accept or dismiss the file-browser confirm. Yes is the only step that reads or writes the file. |
| `compose.importMerge` / `compose.importReplace` | **Merge**, or **Import** (replace the board and apply model, speed, pitch, and volume). Each asks Yes/No before reading the file. |
| `compose.saveName` / `compose.cancelName` / `compose.deleteName` | Item editor: save, discard, or delete and return. |
| `compose.editShowColors` | **Color** on the item editor. Opens the settings color picker. Save keeps that color on the item until the name is saved. Cancel discards it. |
| `compose.editShowIcons` | **Icon**. Opens a searchable Material Symbols board the same size as the color picker, eight across. Icons come from fonts.google.com when the network works, otherwise a short offline set. The footer is Clear, the current icon, Save, and Cancel. The item does not change until Save. |
| `compose.iconSearch` | Dwell **Search**. Shows the Speak keyboard, with Esc in place of Edit. Enter or Esc closes the keyboard and keeps the search. |
| `compose.iconPick.<i>` / `compose.iconRow.<n>` | Highlight result `i` (`0` clears it) or move by `n` tile rows. The track and the arrows both move whole rows. |
| `compose.iconSave` / `compose.iconCancel` | Apply the highlighted icon, or close the list and keep the previous icon. |
| `history.play.<id>` / `history.restore.<id>` / `history.delete.<id>` | Replay, put the phrase back, or drop the row. |
| `history.list.next` / `.prev` / `history.list.goto.<n>` | History list page/jump. After the lock-on ring fills, looking along the track drags the thumb. |
| `compose.pin` / `compose.cancelAssign` | Empty buffer no-op; otherwise dwell a cell to save the phrase. |
| `compose.editPins` | Keyboard **Edit** key. Dwell a pin or topic to rename. |
| `soundboard.edit.<id>` / `soundboard.editTopic.<id>` | Pin / topic item editor. |
| `soundboard.play.<id>` | Play a button. |
| `soundboard.assign.sb_r_c` | Create/overwrite a cell. |
| `soundboard.topic.<id>` | Switch topic. |
| `soundboard.newTopic` / `soundboard.loadStarters` | Topics. |
| `speech.model.sapi` / `.eleven_v4_turbo` | Composer engine. `.eleven_flash_v2_5` and `.eleven_v3` also select v4 Turbo. |
| `speech.voice.<id>` | Select a voice (percent-encoded id). |
| `speech.voicePreview.<id>` / `speech.preview` | Preview without selecting / preview current. |
| `speech.fav.toggle` / `speech.fav.toggle.<id>` | Favorite the current or named ElevenLabs voice. |
| `speech.voiceList.next` / `.prev` / `.goto.<n>` | Voice list page/jump. After the lock-on ring fills, looking along the track drags the thumb. |
| `speech.gender.all` / `.female` / `.male` | Voice filter. |
| `speech.lang.all` / `speech.lang.set.<code>` / `speech.lang.browse` | Language filter. Browse lists every language. |
| `speech.voiceSearch` | Search voice names. The list shortens so the Speak keyboard can type the query. |
| `speech.speed.dec` / `.inc` | Nudge `speechSpeed` by 0.1. |
| `speech.pitch.dec` / `.inc` | Nudge `speechPitch` by 0.1 (0.5–2, ElevenLabs only). |
| `speech.volume.dec` / `.inc` | Nudge `speechVolume` by 0.5 (1–5× clip boost). The voice list uses these. |
| `volume.dec` / `.inc` | Nudge the Speak output device by 10% on any page. That device is Settings → Speak, or the Windows default when none is chosen. `compose.volume.dec` / `.inc` are the same commands. A slider with `caption="volume"` shows that percent. Left and right arrows sit at the ends of the track. Look at the track until its ring fills. Looking at either end moves the thumb there at full speed. A look between the thumb and an end moves it more slowly. Hue, opacity, and list scrollbars use that same ring. Vertical tracks have up and down arrows. |

While `compose` (or a compose live board) is the source page, `Send` and mapping keys are captured into the phrase. See [Actions — Composer capture](actions.md#composer-capture).

---

## Settings

Patterns — boards generate many concrete names from these:

| Pattern | What happens |
|---------|----------------|
| `settings.edit.<numericKey>` | Open the numpad for that setting (`dwellMs`, `dwellSequence`, `rapidDwellMs`, `rapidDwellSequence` open the array editor). |
| `settings.nudge.<numericKey>.dec` / `.inc` | Step a numeric setting. |
| `settings.edit.dwellSequence` / `.rapidDwellSequence` | Standard / rapid dwell-sequence array boards. |
| `settings.numpad.*` | Live numpad keys. |
| `settings.array.*` | Sequence editor (`nudge.N`, `edit.N`, `del.N` for N = 0…11). |
| `settings.edit.color.<colorKey>` | Open the color picker (`hoverColor`, `flashColor`, progress, combo, custom primary/secondary, …). |
| `settings.color.nudge.h` / `.a` / `.edit.a` | Hue / opacity. |
| `settings.color.field.left` / `.right` / `.up` / `.down` | Nudge the HSV handle on the saturation×value square. |
| `settings.color.pickAtGaze` | Arm move-to on the picker (no OS click). |
| `settings.color.eyedropper` | Hide Gazer and sample a screen pixel. |
| `settings.color.palette.0`… | Palette wells (red→neutral columns, 950→50 down). |
| `settings.color.editHex` / `.save` / `.cancel` | Hex pad. |
| `settings.hex.digit.*` / `.backspace` / `.clear` / `.copy` / `.paste` / `.save` / `.cancel` | Hex keys. |
| `settings.numpad.copy` / `.paste` | Clipboard on the numeric editor. |
| `settings.flash.custom.toggle` | Custom flash color (off = item foreground). |
| `settings.hover.custom.toggle` | Custom hover outline (off = progress color). |
| `settings.progress.*.toggle` / `settings.mouseProgress.*.toggle` | Progress bits (radial / pie / fill). Any subset, including none. |
| `settings.session.autoCollapse.toggle` / `.startDocked.toggle` / `.showSplash.toggle` / `.showSplash.play` / `.layoutAutoClose.toggle` | Session. |
| `settings.speech.linkKey` | **Create key** on the key board. Clears the clipboard, closes Settings, and shows Click and Cancel above the taskbar. Click is a left click at gaze for Authorize. A valid `sk_` key on the clipboard after that click is saved, and Settings returns to Speak. |
| `settings.speech.linkCancel` | Leave the create-key page and reopen Settings on Speak. |
| `settings.speech.editKey` / `.clearKey` | Open the key board / wipe the stored key. |
| `settings.speech.key.paste` / `.save` / `.cancel` / `.clear` | Key board. Clear deletes the stored key. Save stores a valid key, or an empty key, and closes. |
| `settings.speech.predictions.toggle` | Turn word suggestions on the Speak page on or off. |
| `settings.speech.output.<id>` | Play speech on that device. An empty id follows the Windows default. `<id>` is the percent-encoded device key. |
| `settings.speech.output.refresh` | Rescan playback devices on the Speak page. |
| `settings.speech.model.sapi` / `.eleven_v4_turbo` | Same handlers as `speech.model.*`. |
| `settings.dwell.slow` / `.normal` / `.fast` / `.custom` | Dwell presets (standard + rapid together). |
| `settings.dwell.custom.save` / `.restore` | Save or restore Custom timings. |
| `settings.mag.follow.slow` / `.sticky` / `.smooth` / `.snappy` | Gaze follow (indicator, gaze mouse, live lens). |
| `settings.tracker.auto` / `.mouse` | Tracker pref (restart). |
| `settings.capture.all` / `.pages` / `.none` | Screen capture. All includes overlays (for documentation). Pages keeps boards and hides look-to, the reticle, and the lens. None hides boards and those overlays. |
| `settings.vigem.install` / `.refresh` | Download/launch official ViGEmBus setup; re-read bus + client status. |
| `settings.mousePickStyle.*.toggle` | Move-to markers (cursor, dot, crosshair). Any subset, including none. The zoom region uses the same set. |
| `settings.pickWindow.round` / `.square` | Zoom window shape. |
| `settings.pickCenter.gaze` / `.screen` | Zoom window at gaze or screen center. |
| `settings.nudge.themeSaturation.dec` / `.inc` | Saturation 20–100 in five steps. |
| `settings.theme.assign.primary` / `.secondary` | Which accent the shade chips write (Primary vs Progress). |
| `settings.theme.shade.<family>.1`…`.9` | Assign that Material shade (100–900) to Progress at 60% opacity. Families: `primary`, `complementary`, `analogous1` / `2`, `tertiary1` / `2`. |
| `theme.light` / `.dark` | Light or dark background (keeps the current tint family). |
| `theme.lightTinted` / `.darkTinted` | Legacy: light/dark plus primary tint. |
| `theme.brightness.0`…`.4` | Five background shades. |
| `theme.tint.none` / `.primary` / `.complementary` / `.analogous1` / `.analogous2` / `.tertiary1` / `.tertiary2` | Hue washed onto the background shades. |
| `settings.reset` | Factory defaults. |

`activeState` for these cells is the command name (`theme.light`, `settings.progress.radial.toggle`). Live assist uses the feature stem (`lookToScroll`, `dwellSuspend`).

---

## Head pose

| Command | What happens |
|---------|----------------|
| `headPose.enabled.toggle` | Master analog maps on/off. |
| `headPose.recenter` | Zero yaw, pitch, roll, x, y, and z at the current pose. |
| `headPose.chart.axis.<axis>` | Select an unmapped axis on the graph (`yaw` / `pitch` / `roll` / `x` / `y` / `z`). |
| `headPose.chart.map.<id>` | Select that map’s curve on the graph. |
| `headPose.axis.<axis>.add` | Add a map for that axis and open the editor. |
| `headPose.duplicate.<id>` | Clone that map and open the editor. |
| `headPose.addMap` | Append a map for the chart axis and open the editor. |
| `headPose.edit.<id>` | Open that map’s editor in the right column. |
| `headPose.map.source.<axis>` / `headPose.map.dest.<dest>` | Editor source / destination. Choosing a destination returns to the editor. |
| `headPose.map.pickOutput` / `headPose.map.outputs.back` | Output list, and back to the editor. |
| `headPose.map.invert` | Negate every curve output. |
| `headPose.map.enabled.toggle` / `.delete` / `.done` | Per-map enable, remove the map, or close the editor. |
| `headPose.map.point.<index>.in.dec` / `.inc` / `.edit` | Nudge or type that point’s input. |
| `headPose.map.point.<index>.out.dec` / `.inc` / `.edit` | Nudge or type that point’s output. |
| `headPose.map.point.<index>.del` | Remove that point (at least two remain). |
| `headPose.map.point.prev` / `.next` / `.add` / `.del` | Selected point: move the halo, append, or remove. |
| `headPose.map.point.in.dec` / `.inc` / `.edit` and `.out.*` | Nudge or type the selected point. |
| `headPose.map.curve.scrub` | Gaze-move the selected handle. Not wired on the settings graph. |
| `headPose.map.pickCommand` | Open the command list in the right column. |
| `headPose.cmd.<name>` | Percent-encoded command. Sets the destination to Command and returns to the editor. |
| `headPose.cmdList.next` / `.prev` / `.cancel` | Command pages. Cancel returns to the output list. |
| `headPose.map.commandAt.dec` / `.inc` | Trigger input for a command map. |

Head-pose analog maps live in settings (`headPoseMaps`), not in `default.json`.
