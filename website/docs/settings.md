# Settings

Drawer **Settings** (`main_settings`) is a hub of live pages. There are no desktop dialogs for these prefs.

| Board | What you change |
|-------|-----------------|
| **Speed** | Slow / Normal / Fast / Custom. Standard vs rapid sequences, mouse-move dwell, mag-pick dwell. Advanced: scan grace. |
| **Magnify** | Mag-pick, foresight, zoom window size/shape, follow profile (slow / sticky / smooth / snappy). |
| **Indicators** | Dwell progress shape (radial, pie, fill directions), pick markers, flash. |
| **Assist** | Tracker (auto Tobii / mouse), live lens, gaze helpers. |
| **Tools** | Four look-to maps (scroll, mouse, left stick, right stick) with per-map ring editors, ComboMouse radii and colors. |
| **Theme** | Light / dark, brightness, tint family, accent, progress color, saturation, custom palette. |
| **Speech** | ElevenLabs key, engine, speed, volume. |
| **Head** | Analog head-pose maps (yaw / pitch / roll / x / y / z → commands or a virtual pad). |

Session toggles (start docked, auto-collapse drawer, layout auto-close, startup dock tour) live on these boards as well.

Prefs are `%AppData%\Gazer\settings.json`. The ElevenLabs key is **not** in that file; it is DPAPI-protected under `secrets\`. Each MSI install deletes `settings.json` so the next launch writes factory defaults.

## Theme

Surfaces use a Fluent-style palette: appearance (light / dark) × brightness (five shades) × optional tint family × accent × progress color. Cells can name a theme **role** (`background`, `accent`, `progress`, `foreground`, `danger`), a tone stop (`bg100`…`bg05`, `accent100`…`accent05`), or a palette stop (`red05`…`red95` and the other picker families) instead of a hex color — names resolve from the live theme, and `bg*` stops follow the page background when one is set.

Icons: set `icon` to the filename stem in `resources/icons/svg/` (`menu`, `mouseLeftClick`, `keyTab`). Matching is case-insensitive; a trailing `Icon` is ignored. Unknown names fall back to the label. Most glyphs are [Material Symbols Rounded](https://fonts.google.com/icons?icon.set=Material+Symbols&icon.style=Rounded).
