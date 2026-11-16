# Settings

Drawer **Settings** opens `main_settings_host`. The top row is **Settings** beside the page name, with **Close** on the right. Categories stay in a list on the left. The boards themselves stay on the gaze host. **Import** and **Export** open the Speak file browser. The Speak keyboard sits against the bottom of that browser. On that keyboard, Edit is Esc and Actions is Ctrl.

| Board | What you change |
|-------|-----------------|
| **Dwell** | Slow / Normal / Fast / Custom. Presets also set pointer dwell, zoom dwell, and blink grace. Standard and rapid sequences. Likely keys: dynamic size lets a likely next character take the outer edge of a key, and dynamic duration shortens that key’s rapid dwell. Scan grace, blink grace, and button progress (ring, pie, fill). |
| **Pointer** | Place: Move to…, pointer dwell, pointer grace, pointer progress, and final-pick markers. Those markers also show while choosing the zoom region. Magnify: magnify pick, zoom dwell, zoom level, zoom size, fill screen, shape, and position. |
| **Gaze** | Follow (slow / sticky / smooth / snappy, show gaze, gaze mouse). Lens (on, zoom, size). The four look-to maps. ComboMouse radii and colors. |
| **Head** | Analog head-pose maps (yaw / pitch / roll / x / y / z → commands or a virtual pad). |
| **Speak** | Composer voice: API key, SAPI or ElevenLabs model, speed, boost, word predictions, and the playback device. Volume sliders and +/− use that device. The voice library stays on the Speak board. |
| **Theme** | Light / dark, brightness, tint family, accent, progress color, saturation, custom palette, hover, and flash. |
| **Setup** | Tracker, ViGEm install, screen capture (**All** / **Pages** / **None**), startup splash (**Play tour**), auto-close, **Open log folder**, **View defaults**, **Import**, **Export**, and **Check for updates**. |

Live boards fire `settings.*` / `theme.*` / `headPose.*` commands (numpad, color picker, dwell presets, ViGEm install, …). Patterns: [Commands — Settings](reference/commands.md#settings).

Prefs are `%AppData%\Gazer\settings.json`. The ElevenLabs key is **not** in that file; it is DPAPI-protected under `secrets\`. Installs and upgrades keep this file. `settings.reset` writes factory defaults and copies the current file to `settings.json.bak`.

**View defaults** lists each setting’s name and description, its factory value, its current value, an Edit button, and a Select toggle. Edit sits to the right of the current value. Colors show swatches for the factory and current values, and Edit opens the color picker. An on/off setting puts the switch on the current value, draws an accent border around that cell, and turns Edit and Select into Save and Cancel. Other choices open a pad in the same place as the numpad, with Save and Cancel. A setting that opens another board uses a go-to arrow. Numbers open the numpad, and look-to maps open their editor. The rows sit in group cards, one card per page, the same way Theme groups its settings. One row holds **Search**, **Select all**, **Deselect all**, **Filter**, and **Reset**. Filter drops a list: All, Dwell, Pointer, Gaze, Head, Speak, Theme, Setup. Search types on the Speak keyboard, including its shift and symbol layers. That keyboard’s Edit key is Esc and its Actions key is Ctrl. Enter or Escape closes that keyboard and keeps the query. Select all marks the rows that match the current search and filter. Reset returns the selected settings to their factory values.

**Import** and **Export** use the same Speak file browser as Speak boards. The Speak keyboard types the file name. Its top edge meets the bottom of the browser. Edit is Esc, which steps back, and Actions is Ctrl. Export writes the full settings snapshot (default name `gazer-settings.json`, Documents folder). Import replaces the live settings from that file after copying `settings.json` to `settings.json.bak`. Keys omitted from the file return to factory values. A file with no settings, or from a newer Gazer, is left unused. The ElevenLabs key stays where it is. There is no merge for settings.

**Check for updates** looks for a newer release. **Update now** downloads it while you keep working, then asks you to restart to finish. See [Install — Updates](install.md#updates).

## Theme

Surfaces use a Fluent-style palette: appearance (light / dark) × brightness (five shades) × optional tint family × accent × progress color. Cells can name a theme **role** (`background`, `accent`, `progress`, `foreground`, `danger`), a tone stop (`bg100`…`bg05`, `accent100`…`accent05`), or a palette stop (`red05`…`red95` and the other picker families) instead of a hex color — names resolve from the live theme, and `bg*` stops follow the page background when one is set.

Icons: set `icon` to the filename stem in `resources/icons/svg/` (`menu`, `mouseLeftClick`, `keyTab`). Matching is case-insensitive; a trailing `Icon` is ignored. Unknown names fall back to the label. Most glyphs are [Material Symbols Rounded](https://fonts.google.com/icons?icon.set=Material+Symbols&icon.style=Rounded).
