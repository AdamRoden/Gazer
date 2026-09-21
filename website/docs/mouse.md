# Mouse

`example_mouse` is the dwell mouse pad (drawer **Mouse**). Cells are XML **actions** or **commands**. Full tokens: [Actions](reference/actions.md#mouse-clicks-at-the-cursor). Names: [Commands](reference/commands.md).

| What you dwell | XML / command | What happens |
|----------------|---------------|----------------|
| **Nudge** | `mouseMoveByDirection="n"` (also `s` `e` `w` `ne` `nw` `se` `sw`, optional `,px`) | Move the cursor now. Omitted amount uses the mouse-assist step (`cycleMouseMoveAmount`). Commands `mouseMoveUp` / `Down` / `Left` / `Right` are the same step. |
| **Jump to edge** | `mouseMoveToTop` / `Bottom` / `Left` / `Right` | Warp to that screen edge. |
| **Warp to a point** | `mouseMoveToPoint="x,y"` | Absolute (or `relative,x,y`) using [dim tokens](authoring.md#dimensions). |
| **Click** at the cursor | `mouseLeftClick=""` / `double` / `down` / `up` / `toggle` (also Middle / Right) | One click, double-click, hold, release, or flip hold. `activeState="mouse.leftHold"`. Commands `mouseLeftClick` and `mouseLeftDownUp` are the click / toggle-hold forms. |
| **Scroll** | `mouseScrollUp` / `Down` / `Left` / `Right` | Wheel by `cycleMouseScrollAmount`. |
| **Look-to-scroll** | `lookToScroll` / `toggleLookToScroll` | Place an origin, then gaze vs the ring drives wheel. |
| **Move to gaze** | `mouseMoveToGaze=""` (zoom token optional) | **Arm**: the next desktop dwell warps the cursor. Arming again cancels. Zoom: empty = Settings mag-pick; `0` = no magnify; `N` = N×; `-1` / `-2` = foresight. |
| **Move + click at gaze** | `mouseLeftClickAtGaze=""` (also Middle / Right) | Arm dwell-move, then one click of that button. Same zoom tokens. |
| **Gaze click loop** | `mouseMoveToGazeClickLoop` | Sticky dwell-move then click until you stop it. |
| **ComboMouse** | `toggleComboMouse` | Inner drift ring + outer command pie. |

`stopAllActionLoops` (and Close All / Close Other) stops XML `actionLoop` series, the gaze click loop, button holds, modifiers, and ComboMouse.

Look-to-scroll, ComboMouse, and dwell-move also live on the [Assist](assist.md) board.
