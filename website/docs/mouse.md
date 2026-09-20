# Mouse

`example_mouse` is the dwell mouse pad (drawer **Mouse**):

- **Nudge** by step (`mouseMoveByDirection`, `cycleMouseMoveAmount`)
- **Click** at the current cursor (`mouseLeftClick` / middle / right; `double`, `toggle` hold)
- **Scroll** by step or **look-to-scroll**
- **Move to gaze** — dwell a desktop point to warp the cursor
- **Move + click at gaze** — dwell-move, then one click
- **Gaze click loop** — sticky dwell-move then click until you stop it
- **Magnify pick** / **Foresight** — zoom a region before the click (Settings → Magnify)
- **ComboMouse** — directional pie around the pointer

`stopAllActionLoops` (and Close All / Close Other) stops sticky series, click-loop, holds, and ComboMouse.

Look-to-scroll, ComboMouse, and dwell-move also live on the [Assist](assist.md) board.
