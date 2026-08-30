# Page XML

Pages live in `resources/layouts/*.xml`. Catalog id should match the filename stem. Parsed by `PageLoader` into `PageDocument` (`src/layout/`).

## Runtime rules

| Rule | Behavior |
|------|----------|
| Dims | Integer token = pixels (`150`). Token with `.` or `/` = proportion of the bounds (`0.5`, `1/2`). Arithmetic with `A_ScreenWidth` / `A_ScreenHeight` is pixels (`A_ScreenHeight/9*16`), evaluated against the placement surface passed at resolve time (work area when `desktopMode`). |
| Style / dwell | Page inherits from settings, then overrides per field. Grids, cells, and zones inherit from the **page** (never from a grid). Named `style` / `dwell` plus inline attrs override individual members. Grid resolve then drops `foreground` / `progressStyle` / `progressColor`. |
| Overlap | Topmost attached page’s grid is opaque. Shell grids/zones paint and hit above the rest. |
| Drawer / quit | Ordinary `show` flags. Master XML uses `ShowGrid` / `HideGrid` (Main chip shows the drawer; Dismiss hides it; Quit swaps drawer ↔ quit). Consecutive Show/Hide in one cell are applied together, then the drawer animates: appear when a `drawerMotion` grid is shown, dismiss when it is the last master grid hidden, snap when another master grid remains (or is shown in that same list). Hidden shell grids do not reserve host space. |
| Auto-close | Idle on an `autoClose` grid or page closes those boards (root never destroys itself). Duration and the master on/off switch are Settings (`layoutAutoClose`, `layoutAutoCloseIdleMs`). `suspendDwell` stops the idle timer; `resumeDwell` restarts it from zero. |
| Zones | Chrome is hidden until dwell progress or activation flash. Engaged dwell includes the on-screen progress strip. |

## `<Page>`

| Attribute | Description |
|-----------|-------------|
| `id` | **Required** catalog id |
| `name` | Title |
| `master` | Process-lifetime root. Only one. |
| `autoClose` | Opts the page into idle close. Duration is Settings `layoutAutoCloseIdleMs`. |
| chrome / dwell attrs | Override settings per field (`background`, `scanGrace`, `activation`, …). Grids, cells, and zones inherit these. |

Child elements: `<Style>`, `<Dwell>`, `<Zone>`, `<Grid>`.

## `<Style>`

Named or anonymous chrome. An unnamed `<Style>` (no `id`) sets the page default. Grids, cells, and zones reference a named style with `style="id"` and may override the same attributes inline. They inherit from the page, never from a parent grid.

| Attribute | Description |
|-----------|-------------|
| `id` | Omit to set the page default style |
| `background`, `foreground`, `border` | Colors (`#RRGGBB` or `#AARRGGBB`). Grids ignore `foreground`. |
| `thickness` | Border widths: one value, or `t,r,b,l` |
| `radius` | Corner radii: one value, or `tl,tr,br,bl` |
| `progressStyle` | How dwell progress is drawn. Comma-separated: `radial`, `pie`, `border`, `fill` (center), `fillup`, `filldown`, `fillleft`, `fillright`. Grids ignore this. |
| `progressColor` | Dwell-progress accent (`#RRGGBB` or `#AARRGGBB`). Empty inherits settings. Grids ignore this. |
| `blur` | Frosted-glass blur radius |

## `<Dwell>`

Named or anonymous timing. An unnamed `<Dwell>` (no `id`) sets the page default (`scanGrace`, `dwellGrace`, `activation`). Grids, cells, and zones inherit from the page, never from a parent grid. They may reference a named dwell with `dwell="id"` and override individual members inline.

`visibleWhen` on cells and zones is a tiny predicate, **not** JavaScript: omitted = show; `ident` = show when that property is true; `!ident` = show when false. Known properties: `expanded` (any master-page grid is shown), `dwellSuspend`.

## `<Grid>` / `<SubGrid>` / `<Cell>`

A Grid is a placed rectangle of rows and columns. `desktopMode="true"` uses the virtual desktop as the bounds reference; otherwise the current screen.

| Attribute | Description |
|-----------|-------------|
| `anchor` | `TopLeft`, `Top`, `Center`, `Bottom`, … |
| `offset`, `size` | `x,y` dim pairs: pixels, axis proportion (`0.25`), height proportion (`0.25h`), or screen expressions (`A_ScreenHeight/9*16, A_ScreenHeight`) |
| `rows`, `columns`, `gap`, `margin` | Cell mesh |
| `rowWeights` | Relative row heights (`1,2,2` = header half as tall as each content row). Missing tracks are 1 |
| `drawerMotion`, `shell` | Drawer scale animation / always-on-top layer |
| `show` | `true` (default) or `false` — omit from the live session when false. Legacy `chrome="drawer"` / `"quit"` with no `show` loads as hidden and is not written back. |
| `style`, `dwell` | Named style/dwell ids, plus inline chrome/dwell attrs. Grid inherit drops `foreground` / `progressStyle` / `progressColor`. |

Cells use `row`, `col`, `rowSpan`, `colSpan`, `label`, `icon`, `caption`, `role` (`label`, `value`, `tab`, `toggle`, `choice`, `slider`, `preview`, …), `textStyle` (`caption`, `body`, `title`, `section`, `key` — fill the cell with the glyph), `show` (default true), `visibleWhen`, `suspendExempt`. Nested `<SubGrid>` occupies a cell span. Zones take the same `show` attribute. `role` decides whether the item is a dwell target: `label`, `value`, `slider`, and `preview` are not; a `tab` with no actions is the current tab (selected, not a target). `toggle` is independent on/off (switch chrome); `choice` is one-of-a-set (radio chrome). A `choice` with stamped `background` + `progressColor` paints as a scheme preview and is the dwell target. Cells do not take `shell` — they follow their grid.

## `<Zone>`

Screen-anchored chip (dock Main/Sleep, keyboard edge keys). Same leaf fields as a cell, plus `anchor` / `offset` / `size`, optional `desktopMode`, optional `dwellOffset` / `dwellSize` for off-screen dwell, and optional `shell` (always-on-top layer; cells inherit this from their grid).

## Actions

Action is generic: the specific thing to do is named as an attribute (on `<Action>`, or on the cell/zone when there is only one) or as a child element.

```xml
<Cell row="0" col="9" colSpan="10" label="1" send="1"/>
<Cell command="toggleLookToScroll"/>
<Cell openPage="uw_qwerty, true"/>

<Send value="a"/>
<LeftClick/>
<LeftClick value="double"/>
<LeftClick value="toggle"/>
<LeftClickAtGaze/>
<LeftClickAtGaze value="0"/>
<LeftClickAtGaze value="4"/>
<LeftClickAtGaze value="-1"/>
<MouseMoveToGaze/>
<MouseMoveToGaze value="0"/>
<MouseMoveByDirection value="n"/>
<MouseMoveByDirection value="se,40"/>
<MouseMoveToPoint value="100,200"/>
<Command value="toggleLookToScroll"/>
<OpenPage value="uw_qwerty, true"/>
<ShowGrid value="board, true"/>
<ShowZone value="more, true"/>
<ShowCell value="k_q"/>
<ClosePage value="-self"/>
<HideGrid value="-all"/>
<HideZone value="-!self"/>
<HideCell value="k_q"/>
<GoBack/>
<Speak value="Hello"/>
```

A cell or zone may have **one** action attribute. Multiple actions use child elements.

| Name | `value` |
|------|---------|
| `Send` | key[, Down\|Up[, durationMs]] |
| `LeftClick` / `MiddleClick` / `RightClick` | type: `default` / `double` / `down` / `up` / `toggle` (omit for a default click) |
| `LeftClickAtGaze` / `MiddleClickAtGaze` / `RightClickAtGaze` | zoom: omit/`default` = Settings mag-pick; `0` = dwell-move, no magnify; `N` = N× zoom; `-1` = foresight; `-2` = foresight with bonus zoom |
| `MouseMoveToGaze` | same zoom tokens as click-at-gaze |
| `MouseMoveByDirection` | `n`/`s`/`e`/`w`/`ne`/`nw`/`se`/`sw`[, amount px] — amount omitted uses the mouse-assist step |
| `MouseMoveToPoint` | `x,y` screen coords |
| `Command` | builtin or mapping-profile name |
| `OpenPage` | targetId[, true] — `true` saves a breadcrumb of the current page state |
| `ShowGrid` / `ShowZone` / `ShowCell` | targetId[, true] — show a grid, zone, or cell (`openGrid` / `openZone` still load) |
| `ClosePage` | targetId[, true] — `-all`, `-self`, `-!self` (all except current) |
| `HideGrid` / `HideZone` / `HideCell` | targetId[, true] — hide a grid, zone, or cell (`closeGrid` / `closeZone` still load). `-all` hides every target of that kind. |
| `GoBack` | (none) — restore the last breadcrumb |
| `Speak` | TTS text |
| `AHK` | element body / CDATA — written to a temp `.ahk` and started with a local AutoHotkey install (v2 preferred; `#Requires AutoHotkey v1` selects v1). AutoHotkey is not bundled; set `GAZER_AHK` to an exe to override discovery. |

`<Action send="a"/>` is the same as `<Send value="a"/>`. Legacy `<Action id="Send" value="a"/>` still loads. `Click` / `Move` / `MoveAndClick` still load (old `left`/`gaze`/`up` values) but the names above are the ones to use.

## Example

```xml
<Page id="tools" name="Tools">
  <Grid id="board" desktopMode="true" rows="1" columns="2"
        anchor="Top" offset="0,0" size="800,400" gap="12" margin="16">
    <Cell id="hello" row="0" col="0" label="Speak" speak="Hello"/>
    <Cell id="close" row="0" col="1" label="Close" closePage="-self"/>
  </Grid>
</Page>
```
