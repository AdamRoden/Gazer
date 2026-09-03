# Commands

`CommandRegistry` runs a **builtin** first; unknown names fall through to `resources/mappings/default.json`.

| Registered in | What |
|---------------|------|
| `Application.cpp` | Shell: quit, editor, preview |
| `AssistCommands.cpp` | Dwell pause, LTS, mag, reticle, dwell-move, click-at-gaze |
| `GazerServices.cpp` | Modifier cycle, click-at-cursor, stop loops |
| `MouseAssistState.cpp` | Mouse pad: nudge, scroll, edge, holds |
| `SettingsCommands.cpp` | `settings.*` live boards |

When adding a command: register it and add a row here.

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

## Settings (`SettingsCommands.cpp`)

Patterns, not every generated name:

| Pattern | Role |
|---------|------|
| `settings.edit.<numericKey>` | Numpad (`dwellMs` / `dwellSequence` open the array editor) |
| `settings.nudge.<numericKey>.dec` / `.inc` | Step a numeric setting |
| `settings.edit.dwellSequence` | Dwell-sequence array board |
| `settings.numpad.*` | Live numpad keys |
| `settings.array.*` | Sequence editor (`nudge.N`, `edit.N`, `del.N` for N=0..11) |
| `settings.edit.color.<colorKey>` | Open color picker |
| `settings.color.select.` / `.use.` / `.suggest.<colorKey>` | Theme roles |
| `settings.color.nudge.` / `.edit.` / `.scrub.<h\|s\|v\|r\|g\|b\|a>` | Channels |
| `settings.color.editHex` / `.save` / `.cancel` | Hex pad |
| `settings.hex.digit.*` / `.backspace` / `.clear` / `.save` / `.cancel` | Hex keys |
| `settings.opacity.*` / `settings.flash.foreground` / `.custom` | Flash opacity / custom flash |
| `settings.progress.*.toggle` / `settings.mouseProgress.*.toggle` | Progress bits |
| `settings.session.autoCollapse.toggle` / `.startDocked.toggle` / `.layoutAutoClose.toggle` | Session |
| `settings.speech.alsoType.toggle` | Speak also types |
| `settings.dwell.slow` / `.normal` / `.fast` / `.custom` | Dwell presets |
| `settings.dwell.custom.save` / `.restore` | Save or restore Custom timings |
| `settings.mag.follow.slow` / `.sticky` / `.smooth` / `.snappy` | Gaze follow (indicator, gaze mouse, live lens) |
| `settings.lts.indicator.fan` / `.orb` / `.pause` | LTS HUD |
| `settings.tracker.auto` / `.mouse` | Tracker pref (restart) |
| `settings.magPickStyle.*.toggle` / `settings.mousePickStyle.*.toggle` | Pick visuals |
| `settings.pickWindow.round` / `.square` | Zoom window shape |
| `settings.pickCenter.gaze` / `.screen` | Zoom window at gaze point or screen center |
| `settings.nudge.themeSaturation.dec` / `.inc` | Saturation 20–100 in five steps (surfaces keep brightness) |
| `settings.theme.edit` | Open Custom on the role list |
| `theme.light` / `.lightTinted` / `.darkTinted` / `.dark` | Appearance |
| `theme.primary.0`…`.8` | Accent (Apple system: Red, Orange, Yellow, Green, Teal, Blue, Indigo, Purple, Pink) |
| `theme.secondary.0`…`.8` | Progress (same 9, light or dark from appearance) |
| `theme.custom` | Custom palette |
| `settings.color.roles` / `.accent` | Theme picker: full roles vs accent |
| `settings.color.preset.0`…`.8` | Apple system accent chips |
| `settings.reset` | Defaults |

Numeric keys: `dwellMs` plus `kIntSpecs` / `kDoubleSpecs` in `AppSettings.cpp`. Color keys: `SettingsUi::kColorKeys`.

`activeState` is the command name (`settings.progress.radial.toggle`, `theme.light`). Live assist uses the feature stem (`lookToScroll`, `dwellSuspend`). `CloseAllPages` / `CloseOtherPages` also disable ComboMouse.

## Mapping-only (`default.json`)

Not builtins. Examples: `backspace`, `tab`, `enter`, `space`, `escape`, `arrow*`, `clearPhrase`, `speakPhrase`, `windowMid` / `windowMax`.
