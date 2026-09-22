# Settings

Drawer **Settings** (`main_settings`) is a hub of live pages. There are no desktop dialogs for these prefs.

The hub opens the basic settings layer. **Advanced** on the hub, and on the basic tab bar, opens a second layer. **Basic** on the advanced layer closes it. **Done** on a tab bar closes settings.

| Board | Layer | What you change |
|-------|-------|-----------------|
| **Speed** | Basic | Slow / Normal / Fast / Custom. Standard vs rapid sequences, mouse-move dwell, mag-pick dwell. **More…** opens Advanced speed. |
| **Magnify** | Basic | Mag-pick, foresight, zoom window size/shape, follow profile (slow / sticky / smooth / snappy). |
| **Assist** | Basic | Gaze helpers and the live lens. |
| **Theme** | Basic | Light / dark, brightness, tint family, accent, progress color, saturation, custom palette. |
| **Head** | Basic | Analog head-pose maps (yaw / pitch / roll / x / y / z → commands or a virtual pad). |
| **Adv. speed** | Advanced | Scan grace, blink grace, pointer grace, foresight hold, auto-close. |
| **Indicators** | Advanced | Dwell progress shape (radial, pie, fill directions), pick markers, flash. |
| **Tools** | Advanced | Four look-to maps (scroll, mouse, left stick, right stick) with per-map ring editors, ComboMouse radii and colors. |
| **Admin** | Advanced | Tracker, ViGEm install, speech (ElevenLabs key, engine, speed, boost), then screen capture (**All** / **Pages** / **None**). |

Session toggles (start docked, auto-collapse drawer, layout auto-close, startup dock tour) live on these boards as well.

Live boards fire `settings.*` / `theme.*` / `headPose.*` commands (numpad, color picker, speed presets, ViGEm install, …). Patterns: [Commands — Settings](reference/commands.md#settings).

Prefs are `%AppData%\Gazer\settings.json`. The ElevenLabs key is **not** in that file; it is DPAPI-protected under `secrets\`. Each MSI install deletes `settings.json` so the next launch writes factory defaults.

## Theme

Surfaces use a Fluent-style palette: appearance (light / dark) × brightness (five shades) × optional tint family × accent × progress color. Cells can name a theme **role** (`background`, `accent`, `progress`, `foreground`, `danger`), a tone stop (`bg100`…`bg05`, `accent100`…`accent05`), or a palette stop (`red05`…`red95` and the other picker families) instead of a hex color — names resolve from the live theme, and `bg*` stops follow the page background when one is set.

Icons: set `icon` to the filename stem in `resources/icons/svg/` (`menu`, `mouseLeftClick`, `keyTab`). Matching is case-insensitive; a trailing `Icon` is ignored. Unknown names fall back to the label. Most glyphs are [Material Symbols Rounded](https://fonts.google.com/icons?icon.set=Material+Symbols&icon.style=Rounded).
