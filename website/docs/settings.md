# Settings

Drawer **Settings** opens `main_settings_host`. The top row is **Settings** beside the page name, with **Close** on the right. Categories stay in a list on the left. There are no desktop dialogs for these prefs.

| Board | What you change |
|-------|-----------------|
| **Dwell** | Slow / Normal / Fast / Custom. Presets also set pointer dwell, zoom dwell, and blink grace. Standard and rapid sequences. Likely keys: dynamic size lets a likely next character take the outer edge of a key, and dynamic duration shortens that key’s rapid dwell. Scan grace, blink grace, and button progress (ring, pie, fill). |
| **Pointer** | Place: Move to…, pointer dwell, pointer grace, pointer progress, and final-pick markers. Those markers also show while choosing the zoom region. Magnify: magnify pick, zoom dwell, zoom level, zoom size, fill screen, shape, and position. |
| **Gaze** | Follow (slow / sticky / smooth / snappy, show gaze, gaze mouse). Lens (on, zoom, size). The four look-to maps. ComboMouse radii and colors. |
| **Head** | Analog head-pose maps (yaw / pitch / roll / x / y / z → commands or a virtual pad). |
| **Speak** | Composer voice: API key, SAPI or ElevenLabs model, speed, boost, word predictions, and the playback device. Volume sliders and +/− use that device. The voice library stays on the Speak board. |
| **Theme** | Light / dark, brightness, tint family, accent, progress color, saturation, custom palette, hover, and flash. |
| **Setup** | Tracker, ViGEm install, screen capture (**All** / **Pages** / **None**), startup splash (**Play tour**), auto-close, and **Open log folder**. |

Live boards fire `settings.*` / `theme.*` / `headPose.*` commands (numpad, color picker, dwell presets, ViGEm install, …). Patterns: [Commands — Settings](reference/commands.md#settings).

Prefs are `%AppData%\Gazer\settings.json`. The ElevenLabs key is **not** in that file; it is DPAPI-protected under `secrets\`. Installs and upgrades keep this file. Factory defaults come from **Reset my settings to factory defaults** in setup (`RESETUSERSETTINGS=1`) or from `settings.reset` in the app, which also copies the current file to `settings.json.bak`.

## Theme

Surfaces use a Fluent-style palette: appearance (light / dark) × brightness (five shades) × optional tint family × accent × progress color. Cells can name a theme **role** (`background`, `accent`, `progress`, `foreground`, `danger`), a tone stop (`bg100`…`bg05`, `accent100`…`accent05`), or a palette stop (`red05`…`red95` and the other picker families) instead of a hex color — names resolve from the live theme, and `bg*` stops follow the page background when one is set.

Icons: set `icon` to the filename stem in `resources/icons/svg/` (`menu`, `mouseLeftClick`, `keyTab`). Matching is case-insensitive; a trailing `Icon` is ignored. Unknown names fall back to the label. Most glyphs are [Material Symbols Rounded](https://fonts.google.com/icons?icon.set=Material+Symbols&icon.style=Rounded).
