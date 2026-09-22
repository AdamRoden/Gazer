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

Gamepad needs [ViGEmBus](https://github.com/nefarius/ViGEmBus). Settings → Assist can install the bus. Gazer ships `ViGEmClient.dll` next to `Gazer.exe`.

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
| `settings.session.showSplash.play` | Run the startup dock tour now (Assist → Play tour). |

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
| `lookTo.map.*` | Ring editor: preview, hub, radii, speed, direction, overlay show/fill/border. |
| `toggleComboMouse` | Arm ComboMouse place, or disable. |
| `mouseMoveToGaze` | Toggle dwell-to-warp cursor. |
| `mouseMoveToGazeClickLoop` | Sticky dwell-move then click until stopped. |
| `mouseLeftClickAtGaze` / `mouseMiddleClickAtGaze` / `mouseRightClickAtGaze` | Dwell-move then one click. |
| `toggleMouseMoveMagPick` | Settings: magnify pick on/off. |
| `toggleMouseMoveMagPickCenter` | Mag-pick center on dwell vs screen. |
| `toggleMouseMoveMagPickFullScreen` | Mag-pick full-screen zoom. |
| `toggleMouseMoveForesight` | Foresight on/off. |
| `toggleMouseMoveForesightSecondZoom` | Second zoom inside foresight. |

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
| `compose.stop` | Abort Eleven POST, stop clip, stop SAPI. |
| `compose.clear` / `compose.undo` / `compose.redo` | Buffer. |
| `compose.backspace` / `compose.deleteWord` | Edit (rapid dwell). |
| `compose.removeWord.<i>` | Prefix; `i` is visible chip 0–11. |
| `compose.moveEndOfWord.<i>` / `compose.moveStartOfWord.<i>` | Caret to that chip’s word edge. |
| `compose.openVoices` / `compose.openHistory` | Voice list; history. |
| `compose.toggleFreestyle` | Speak board: topics ↔ Freestyle. |
| `compose.saveName` / `compose.cancelName` / `compose.deleteName` | Item editor: save, discard, or delete and return. |
| `compose.editShowColors` / `compose.editShowIcons` | Color chips vs icons on the editor. |
| `compose.editColor.<i>` / `compose.editIcon.<i>` | Set color or icon. |
| `compose.editClearColor` / `compose.editClearIcon` | Clear custom color or icon. |
| `compose.voicePreset.<id>` / `compose.editVoicePreset.<id>` / `compose.newVoicePreset` | Apply, rename, or save a Freestyle voice (voice + speed + boost). |
| `compose.editTag.<i>` / `compose.newTag` | Rename or create a saved tag. |
| `history.play.<id>` / `history.restore.<id>` / `history.delete.<id>` | Replay, put the phrase back, or drop the row. |
| `history.list.next` / `.prev` / `history.list.goto.<n>` | History list page/jump (gaze along the track also scrolls). |
| `compose.pin` / `compose.cancelAssign` | Topics: empty buffer no-op; phrase assigns onto the soundboard. Freestyle: save the current voice + speed + boost. |
| `compose.editPins` | Edit mode: dwell a pin/topic (or Freestyle voice/tag) to rename. |
| `soundboard.edit.<id>` / `soundboard.editTopic.<id>` | Pin / topic item editor. |
| `soundboard.play.<id>` | Play a button. |
| `soundboard.assign.sb_r_c` | Create/overwrite a cell. |
| `soundboard.topic.<id>` | Switch topic. |
| `soundboard.newTopic` / `soundboard.loadStarters` | Topics. |
| `compose.insertTagAt.<i>` | Insert `savedSpeechTags[i]`. |
| `speech.model.sapi` / `.eleven_flash_v2_5` / `.eleven_v3` | Composer engine. |
| `speech.voice.<id>` | Select a voice (percent-encoded id). |
| `speech.voicePreview.<id>` / `speech.preview` | Preview without selecting / preview current. |
| `speech.fav.toggle` / `speech.fav.toggle.<id>` | Favorite the current or named ElevenLabs voice. |
| `speech.voiceList.next` / `.prev` / `.goto.<n>` | Voice list page/jump. |
| `speech.gender.all` / `.female` / `.male` | Voice filter. |
| `speech.lang.all` / `speech.lang.set.<code>` | Language filter. |
| `speech.speed.dec` / `.inc` | Nudge `speechSpeed` by 0.1. |
| `speech.volume.dec` / `.inc` | Nudge `speechVolume` by 0.5 (1–5× clip boost). |
| `compose.volume.dec` / `.inc` | Nudge Windows master volume by 10%. |

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
| `settings.progress.*.toggle` / `settings.mouseProgress.*.toggle` | Progress bits (radial / pie / fill). |
| `settings.session.autoCollapse.toggle` / `.startDocked.toggle` / `.showSplash.toggle` / `.showSplash.play` / `.layoutAutoClose.toggle` | Session. |
| `settings.speech.editKey` / `.clearKey` | Paste-from-clipboard API-key board / wipe DPAPI. |
| `settings.speech.key.paste` / `.save` / `.cancel` / `.clear` | Key board. |
| `settings.speech.model.sapi` / `.eleven_flash_v2_5` / `.eleven_v3` | Same handlers as `speech.model.*`. |
| `settings.dwell.slow` / `.normal` / `.fast` / `.custom` | Speed presets (standard + rapid together). |
| `settings.dwell.custom.save` / `.restore` | Save or restore Custom timings. |
| `settings.mag.follow.slow` / `.sticky` / `.smooth` / `.snappy` | Gaze follow (indicator, gaze mouse, live lens). |
| `settings.tracker.auto` / `.mouse` | Tracker pref (restart). |
| `settings.capture.all` / `.pages` / `.none` | Screen capture. All includes overlays (for documentation). Pages keeps boards and hides look-to, the reticle, and the lens. None hides boards and those overlays. |
| `settings.vigem.install` / `.refresh` | Download/launch official ViGEmBus setup; re-read bus + client status. |
| `settings.magPickStyle.*.toggle` / `settings.mousePickStyle.*.toggle` | Pick visuals. |
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
| `headPose.chart.axis.<axis>` | Select graph input (`yaw` / `pitch` / `roll` / `x` / `y` / `z`). |
| `headPose.map.source.<axis>` / `headPose.map.dest.<dest>` | Editor source / destination. |
| `headPose.addMap` | Append a map for the selected axis and open the editor. |
| `headPose.edit.<id>` | Open that map’s editor. |
| `headPose.map.enabled.toggle` / `.delete` / `.done` | Editor. |
| `headPose.map.point.*` | Curve points (prev/next/add/del/nudge/edit). |
| `headPose.map.curve.scrub` | Gaze-move the selected handle. |
| `headPose.map.pickCommand` / `headPose.cmd.<name>` / `headPose.cmdList.next` / `.prev` / `.cancel` | Command destination. |
| `headPose.map.commandAt.dec` / `.inc` | Trigger input value. |

Head-pose analog maps live in settings (`headPoseMaps`), not in `default.json`.
