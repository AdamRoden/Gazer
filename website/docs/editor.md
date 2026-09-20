# Page editor

Tray → **Page editor**, command `openPageEditor`, or launch with `--editor`.

The designer edits **Page XML** (the same files the runtime loads). Three panes:

| Pane | Contents |
|------|----------|
| Left | **Add** (button, label, toggle, tab, slider, zone, grid, subgrid, named style, named dwell) and the element tree (page → styles / dwells / zones / grids → cells / subgrids). Right-click to duplicate, delete, convert cell ↔ zone, add a subgrid, or change paint order. |
| Center | **Fit grid** (default): the selected grid fills the canvas. Uncheck it to see true placement on a 1920×1080 virtual display. Click / Shift-click to select; drag to move; accent handles resize; arrows nudge (Shift = 16 px); Delete removes. **Esc** cancels click-to-place. Zones show the progress chip and the dwell region. The toolbar **layer** combo (next to Code view) filters which grid/zone layers paint on the canvas. |
| Right | Tabs follow the selection: **Page**; **Grid / Style / Placement**; **Cell** or **Zone / Style / Placement / Action**; named **Style** or **Dwell** alone. Placement holds anchor, offset, row/col/span, and zone progress/dwell regions. Cell and zone dwell inherit/overrides sit at the bottom of Action. |

File → New asks for id, name, and a template (blank, full keyboard, keyboard row, settings row, zone chip). File → Open lists shipped and user `*.xml` pages. **Save** of a shipped file writes a user copy to `%AppData%\Gazer\layouts` and leaves `resources/` unchanged. Shift is a modifier on QWERTY boards (labels switch to the shifted glyph). **Test on canvas** (F6) plays a dwell ring. **Test on desktop** (F5) attaches the current page on the live host. Master roots cannot be live-tested. Empty actions warn before Save and F5.

Editor F5 previews attach XML copies under `__editor_preview_*` ids so they do not replace the live page.

Schema and action language: [Authoring pages](authoring.md). Full attribute list: [docs/page-xml.md](https://github.com/AdamRoden/Gazer/blob/master/docs/page-xml.md) in the repo.
