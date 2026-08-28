# Commands

`CommandRegistry` runs a **builtin** first; unknown names fall through to `resources/mappings/default.json`.

| Registered in | What |
|---------------|------|
| `Application.cpp` | Shell: quit, editor, preview, theme, close others |
| `AssistCommands.cpp` | Dwell pause, LTS, mag, reticle, dwell-move, click-at-gaze |
| `GazerServices.cpp` | Modifier cycle, click-at-cursor, stop loops |
| `MouseAssistState.cpp` | Mouse pad: nudge, scroll, edge, holds |
| `SettingsCommands.cpp` | `settings.*` live boards |

When adding a command: register it, add a row here, and list every alias in the same `registerBuiltin({...})` call.

## Aliases (same handler)

| Canonical | Also |
|-----------|------|
| `leftClickAtGaze` | `mouseMoveAndLeftClick` |
| `rightClickAtGaze` | `mouseMoveAndRightClick` |
| `middleClickAtGaze` | `mouseMoveAndMiddleClick` |
| `leftClick` | `mouseLeftClick` |
| `rightClick` | `mouseRightClick` |
| `middleClick` | `mouseMiddleClick` |
| `mouseDwellMove` | `mouseMoveToGaze` |
| `openPage` / `loadPage` | script aliases `openLayout` / `loadLayout` (`ScriptHost`, not CommandRegistry) |

`toggleDwellSuspend` / `suspendDwell` / `resumeDwell` are **not** aliases (different handlers).

## Shell (`Application.cpp`)

| Command | Role |
|---------|------|
| `quitApp` | Exit |
| `closeOtherViews` | Disable ComboMouse; close attached pages |
| `openLayoutEditor` | Page designer (`Invocation.pageId` optional) |
| `openPreview` | Head-pose preview |
| `theme.dark` / `theme.light` / `theme.custom` | Theme mode |

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
| `mouseDwellMove` (alias above) | Toggle dwell-to-warp cursor |
| `mouseDwellClickLoop` | Sticky dwell-move then click |
| `leftClickAtGaze` (alias above) | Dwell-move then one click |
| `toggleMouseMoveMagPick` | Settings: magnify pick |
| `toggleMouseMoveMagPickCenter` | Mag-pick center on dwell vs screen |
| `toggleMouseMoveMagPickFullScreen` | Mag-pick full-screen zoom |
| `toggleMouseMoveForesight` | Foresight on/off |
| `toggleMouseMoveForesightSecondZoom` | Second zoom inside foresight |

## Input / loops (`GazerServices.cpp`)

Builtins **shadow** mapping keys of the same name (`leftCtrl` in `default.json` is unused while the builtin is registered).

| Command | Role |
|---------|------|
| `leftCtrl` / `rightCtrl` / `leftAlt` / `rightAlt` / `leftWin` / `rightWin` / `leftShift` / `rightShift` | Modifier cycle (Up → Down → LockedDown) |
| `releaseModifiers` | Release all modifiers |
| `leftClick` (alias above) | Click at cursor |
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
| `settings.session.autoCollapse.toggle` / `.startDocked.toggle` | Session |
| `settings.speech.alsoType.toggle` | Speak also types |
| `settings.dwell.slow` / `.normal` / `.fast` | Dwell presets |
| `settings.mag.follow.slow` / `.sticky` / `.smooth` / `.snappy` | Gaze follow (lens, cursor, mag-pick) |
| `settings.lts.indicator.fan` / `.orb` / `.pause` | LTS HUD |
| `settings.tracker.auto` / `.mouse` | Tracker pref (restart) |
| `settings.magPickStyle.*.toggle` / `settings.mousePickStyle.*.toggle` | Pick visuals |
| `settings.pickWindow.round` / `.square` | Zoom window shape |
| `settings.theme.contrast.low` / `.medium` / `.high` | Custom contrast |
| `settings.reset` | Defaults |

Numeric keys: `dwellMs` plus `kIntSpecs` / `kDoubleSpecs` in `AppSettings.cpp`. Color keys: `SettingsUi::kColorKeys`.

## Mapping-only (`default.json`)

Not builtins. Examples: `backspace`, `tab`, `enter`, `space`, `escape`, `arrow*`, `clearPhrase`, `speakPhrase`, `windowMid` / `windowMax`, `mouseLeftDoubleClick`, `gamepadA` / `B`, `lookLeft` / `lookRight`.
