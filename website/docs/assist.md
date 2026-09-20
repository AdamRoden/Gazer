# Assist tools

Opened from the drawer **Assist** page (`example_assist`) or the mouse pad.

| Tool | Command | Notes |
|------|---------|--------|
| Gaze magnifier | `toggleMagnifier` | Live lens. Exclusive with the gaze reticle. |
| Show gaze | `toggleGazeReticle` | Marker at the gaze point. Exclusive with the magnifier. |
| Gaze mouse | `toggleGazeMouseFollow` | Cursor follows gaze. |
| Move mouse (dwell) | `mouseMoveToGaze` | Arm, then dwell a screen point. |
| Gaze click loop | `mouseMoveToGazeClickLoop` | Sticky dwell-move then click. |
| Magnify pick | `toggleMouseMoveMagPick` | Static zoom window for the pick. |
| Foresight | `toggleMouseMoveForesight` | Remember a desktop dwell and zoom that point when Move-to arms. |
| Look-to-scroll | `toggleLookToScroll` | Always places the cursor first, then scrolls from gaze vs a deadzone. Pie for speed, axis (vertical / horizontal / both), reset, quit. |
| ComboMouse | `toggleComboMouse` | Inner drift ring + outer command pie. |
| Edit page | `openPageEditor` | Opens the XML designer. |

Look-to-scroll peak speeds: 1, 2, 5, 10, 20 notches/sec (`lts.speed.slower` / `.faster`). Overlay z-order (front → back): reticle, live lens, mag-pick, other assist overlays, then the page host.

Fine-tuning lives on Settings → **Assist**, **Magnify**, and **Tools**.
