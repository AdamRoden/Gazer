# Commands

`CommandRegistry` runs a **builtin** first; unknown names fall through to `resources/mappings/default.json`. `compose.*` / `speech.*` / `soundboard.*` / `history.*` / `settings.*` / `headPose.*` skip the Cmd toast.

AHK / CLI / Python is not this table. `Gazer.exe --action …` (and the `Gazer` named pipe) parse the same action language as [page XML](../authoring.md). After `OpenPage` in one payload, `ShowLayers` applies to the opened page.

| Command | Role |
|---------|------|
| `quitApp` | Exit |
| `toggleDwellSuspend` / `suspendDwell` / `resumeDwell` | Global dwell pause (not aliases of each other) |
| `toggleMagnifier` | Lens (exclusive with gaze reticle) |
| `toggleLookToScroll` | Gaze-driven scroll |
| `toggleGazeReticle` | Gaze marker (exclusive with magnifier) |
| `toggleGazeMouseFollow` | Cursor follows gaze |
| `mouseMoveToGaze` | Dwell to place the cursor |
| `mouseMoveToGazeClickLoop` | Sticky dwell-move then click |
| `mouseLeftClick` / `mouseRightClick` / `mouseMiddleClick` | Click at cursor |
| `mouseLeftClickAtGaze` / `mouseRightClickAtGaze` / `mouseMiddleClickAtGaze` | Dwell-move then click |
| `stopAllActionLoops` | Stop sticky series and assist loops |
| `openPreview` | Head-pose preview |
| `openPageEditor` | XML page designer |
| `compose.open` | Speech composer |
| `theme.light` / `.dark` | Light or dark background |
| `theme.brightness.0`…`.4` | Background shade |
| `theme.tint.none` / `.primary` / `.complementary` / `.analogous1` / `.analogous2` / `.tertiary1` / `.tertiary2` | Background tint hue |
| `settings.*` | Settings hub editors, nudges, presets |

Mapping-only names (not builtins) include `backspace`, `tab`, `enter`, `space`, `escape`. Full catalog, including settings patterns and head-pose maps: [`src/app/Commands.md`](https://github.com/AdamRoden/Gazer/blob/master/src/app/Commands.md).
