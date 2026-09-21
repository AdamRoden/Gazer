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
| Look to scroll | `lookToScroll` (`toggleLookToScroll`) | Place an origin, then scroll from gaze vs a circular deadzone. Optional hub pie for speed, axis, reset, quit. |
| Look to mouse | `lookToMouse` | Same analog disk, driving pointer velocity. |
| Look to left / right stick | `lookToLeftStick` / `lookToRightStick` | Same analog disk, driving a virtual Xbox stick (ViGEm). |

All four look-to maps can run at once, each with its own origin and rings. Tune them on Settings → **Tools**. Edit opens a ring editor (deadzone, 0–100% ramp, 100% plateau, optional outer deadzone, optional hub). Preview draws analog rings at the origin (independent of whether the map is enabled). Overlay style is per-map switches: pause, inner deadzone, max, outer deadzone, plus border and fill.
| ComboMouse | `toggleComboMouse` | Inner drift ring + outer command pie. |
| Edit page | `openPageEditor` | Opens the XML designer. |

Scroll-map peak speeds: 0.5–8 notches/sec (`lts.speed.slower` / `.faster`). Overlay z-order (front → back): reticle, live lens, mag-pick, other assist overlays, then the page host.

Fine-tuning lives on Settings → **Assist**, **Magnify**, and **Tools**.
