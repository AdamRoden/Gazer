# Settings

Drawer **Settings** (`main_settings`) is a hub of live pages. There are no desktop dialogs for these prefs.

The hub and the tab bar are the same eight boards. **Done** on the tab bar closes settings.

| Board | What you change |
|-------|-----------------|
| **Dwell** | Slow / Normal / Fast / Custom. Standard and rapid sequences. Likely keys pulls rapid character keys toward what you tend to type next. Scan grace, blink grace, button progress, and pointer progress (ring, pie, fill). |
| **Zoom** | Magnify pick, foresight, repeat zoom, zoom dwell, foresight dwell and hold, window level / size / fill / shape / position, region markers, and Move to…. |
| **Place** | Pointer dwell, pointer grace, final-pick markers, Move to…, and ComboMouse radii and colors. |
| **Gaze** | Gaze follow (slow / sticky / smooth / snappy), show gaze, gaze mouse, the live lens, and the four look-to maps. |
| **Head** | Analog head-pose maps (yaw / pitch / roll / x / y / z → commands or a virtual pad). |
| **Speak** | Composer voice: API key, SAPI or ElevenLabs model, speed, boost, and word predictions. The voice library stays on the Speak board. |
| **Theme** | Light / dark, brightness, tint family, accent, progress color, saturation, custom palette, hover, and flash. |
| **Setup** | Tracker, ViGEm install, screen capture (**All** / **Pages** / **None**), startup splash (**Play tour**), auto-close, and **Open log folder**. |

Live boards fire `settings.*` / `theme.*` / `headPose.*` commands (numpad, color picker, dwell presets, ViGEm install, …). Patterns: [Commands — Settings](reference/commands.md#settings).

Prefs are `%AppData%\Gazer\settings.json`. The ElevenLabs key is **not** in that file; it is DPAPI-protected under `secrets\`. Each MSI install deletes `settings.json` so the next launch writes factory defaults.

## Theme

Surfaces use a Fluent-style palette: appearance (light / dark) × brightness (five shades) × optional tint family × accent × progress color. Cells can name a theme **role** (`background`, `accent`, `progress`, `foreground`, `danger`), a tone stop (`bg100`…`bg05`, `accent100`…`accent05`), or a palette stop (`red05`…`red95` and the other picker families) instead of a hex color — names resolve from the live theme, and `bg*` stops follow the page background when one is set.

Icons: set `icon` to the filename stem in `resources/icons/svg/` (`menu`, `mouseLeftClick`, `keyTab`). Matching is case-insensitive; a trailing `Icon` is ignored. Unknown names fall back to the label. Most glyphs are [Material Symbols Rounded](https://fonts.google.com/icons?icon.set=Material+Symbols&icon.style=Rounded).
