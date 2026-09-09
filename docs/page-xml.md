# Page XML

Pages live in `resources/layouts/*.xml`. Catalog id should match the filename stem. Parsed by `PageLoader` into `PageDocument` (`src/layout/`).

## Runtime rules

| Rule | Behavior |
|------|----------|
| Dims | Integer token = pixels (`150`). Token with `.` or `/` = proportion of the bounds (`0.5`, `1/2`). Arithmetic with `A_ScreenWidth` / `A_ScreenHeight` is pixels (`A_ScreenHeight/9*16`), evaluated against the placement surface passed at resolve time (work area when `desktopMode`). `clamp(value, min, max)` bounds a pixel expression (`clamp(1.8*A_ScreenHeight, 1080, A_ScreenWidth)`). Grid tracks add `*` / `2*` for leftover space (`rowHeights="80,*,120"`). `rowWeights` integers stay star weights, not pixels. |
| Style / dwell | Page inherits from settings, then overrides per field. Unspecified dwell uses **daily driver** for Send / mouse / AHK / modifiers / mapping keys / composer typing, and **designer** for everything else. Grids, cells, and zones inherit from the **page** (never from a grid). Named `style` / `dwell` plus inline attrs override individual members. Grid resolve then drops `foreground` / `progressStyle` / `progressColor`. |
| Overlap | Topmost attached page’s grid is opaque. Shell grids/zones paint and hit above the rest. |
| Drawer / quit | Layer membership. Master XML puts dock chips on layer 1, the drawer on 2, quit on 3. `ShowLayers` sets the visible set (Main chip `1,2`; Dismiss `1`; Quit `1,3`). Consecutive ShowLayers in one cell are applied together, then the drawer animates: appear when a `drawerMotion` grid is shown, dismiss when it is the last master grid hidden, snap when another master grid remains. Hidden shell grids do not reserve host space. |
| Auto-close | Idle on an `autoClose` grid or page closes those boards (root never destroys itself). Duration and the master on/off switch are Settings (`layoutAutoClose`, `layoutAutoCloseIdleMs`). `suspendDwell` stops the idle timer; `resumeDwell` restarts it from zero. |
| Zones | Chrome is hidden until dwell progress or activation flash. Engaged dwell includes the on-screen progress strip. |

## `<Page>`

| Attribute | Description |
|-----------|-------------|
| `id` | **Required** catalog id |
| `name` | Title |
| `master` | Process-lifetime root. Only one. |
| `autoClose` | Opts the page into idle close. Duration is Settings `layoutAutoCloseIdleMs`. |
| `showLayers` | Comma-separated layer numbers visible when the page opens. Default `1`. Live `ShowLayers` actions replace this set. |
| chrome / dwell attrs | Override settings per field (`background`, `scanGrace`, `activation`, …). Grids, cells, and zones inherit these. |

Child elements: `<Style>`, `<Dwell>`, `<Zone>`, `<Grid>`.

## `<Style>`

Named or anonymous chrome. An unnamed `<Style>` (no `id`) sets the page default. Grids, cells, and zones reference a named style with `style="id"` and may override the same attributes inline. They inherit from the page, never from a parent grid.

| Attribute | Description |
|-----------|-------------|
| `id` | Omit to set the page default style |
| `background`, `foreground`, `border` | Colors (`#RRGGBB` or `#AARRGGBB`), a Settings theme role (`background`, `surface`, `accent`, `progress`, `tertiary`, `foreground`, `danger`), or an accent brand (`red`, `orange`, `yellow`, `green`, `teal`, `blue`, `indigo`, `purple`, `pink`). Names resolve from the live theme. Grids ignore `foreground`. |
| `thickness` | Border widths: one value, or `t,r,b,l` |
| `radius` | Corner radii: one value, or `tl,tr,br,bl` |
| `progressStyle` | How dwell progress is drawn. Comma-separated: `radial`, `pie`, `border`, `fill` (center), `fillup`, `filldown`, `fillleft`, `fillright`. Grids ignore this. |
| `progressColor` | Dwell-progress accent. Same tokens as `background` / `foreground` / `border`. Empty inherits settings. Grids ignore this. |
| `blur` | Frosted-glass blur radius |

## `<Dwell>`

Named or anonymous timing. An unnamed `<Dwell>` (no `id`) sets the page default (`scanGrace`, `dwellGrace`, `activation`) for every cell on the page. Grids, cells, and zones inherit from the page, never from a parent grid. They may reference a named dwell with `dwell="id"` and override individual members inline. When the page does not set a default, Speed settings supply **daily driver dwell** (keys, mouse, composer, modifiers, AHK) or **designer dwell** (settings, navigation, assist toggles).

`visibleWhen` on cells and zones is a tiny predicate, **not** JavaScript: omitted = show; `ident` = show when that property is true; `!ident` = show when false. Known properties: `expanded` (any master-page grid is shown), `dwellSuspend`.

## `<Grid>` / `<SubGrid>` / `<Cell>`

A Grid is a placed rectangle of rows and columns. `desktopMode="true"` uses the virtual desktop as the bounds reference; otherwise the current screen.

| Attribute | Description |
|-----------|-------------|
| `anchor` | `TopLeft`, `Top`, `Center`, `Bottom`, … |
| `offset`, `size` | `x,y` dim pairs: pixels, axis proportion (`0.25`), height proportion (`0.25h`), or screen expressions (`A_ScreenHeight/9*16, A_ScreenHeight`). Commas inside parentheses do not split the pair. |
| `rows`, `columns`, `gap`, `margin` | Cell mesh |
| `rowWeights` | Legacy all-star row sizes (`1,2,2` = header half as tall as each content row). Integers are **star weights**, not pixels. Loaded as `*` / `2*` tracks. Writer emits this when every row is a star |
| `rowHeights`, `columnWidths` | Per-track sizes, XAML GridLength-style. Integer token = pixels (`80`, `80px`). `*` / `2*` share leftover space after fixed tracks. Also accepts the usual dim tokens (`0.25`, `1/4`, `0.25h`, `A_ScreenHeight/20`, `clamp(...)`). Missing tracks are `*`. Pixel tracks that overflow the inner size scale down together. Equal columns when `columnWidths` is omitted |
| `drawerMotion`, `shell` | Drawer scale animation / always-on-top layer |
| `layers` | Comma-separated layer membership (`1,2`). Default `1`. Visible when any listed layer is in the page's current `showLayers`. Nested subgrids are skipped when the parent is off-layer, so a parent that hosts children on several layers should list all of them (`layers="1,2"`). |
| `style`, `dwell` | Named style/dwell ids, plus inline chrome/dwell attrs. Grid inherit drops `foreground` / `progressStyle` / `progressColor`. |

Cells use `row`, `col`, `rowSpan`, `colSpan`, `label`, `icon` (stem of a file in `resources/icons/svg/`, e.g. `menu`, `mouseLeftClick`, `keyTab`), `caption`, `role` (`label`, `value`, `tab`, `toggle`, `choice`, `swatch`, `slider`, `preview`, `scrollbar`, …), `textStyle` (`caption`, `body`, `title`, `section`, `key` — fill the cell with the glyph), `visibleWhen`, `suspendExempt`. Nested `<SubGrid>` occupies a cell span. Zones take the same `layers` attribute as grids. `role` decides whether the item is a dwell target: `label`, `value`, `preview`, and `scrollbar` are not; a `slider` with no actions is not, but a `slider` with a command is (starts gaze-follow scrub); a `tab` with no actions is the current tab (selected, not a target). `toggle` is independent on/off (switch chrome); `choice` is one-of-a-set (radio chrome). A `choice` with stamped `background` + `progressColor` paints as a scheme preview and is the dwell target. `swatch` is a dwellable round color well. `scrollbar` is a vertical gaze track (caption `offset,visible,total`); looking along it scrolls, it is not a dwell cell. Cells do not take `shell` or `layers` — they follow their grid.

## `<Zone>`

Screen-anchored chip (dock Main/Sleep, keyboard edge keys). Same leaf fields as a cell, plus `anchor` / `offset` / `size`, optional `desktopMode`, optional `dwellOffset` / `dwellSize` for off-screen dwell, and optional `shell` (always-on-top layer; cells inherit this from their grid).

## Actions

Action is generic: the specific thing to do is named as an attribute (on `<Action>`, or on the cell/zone when there is only one) or as a child element.

```xml
<Cell row="0" col="9" colSpan="10" label="1" send="1"/>
<Cell command="toggleLookToScroll"/>
<Cell openPage="uw_qwerty, true"/>

<Send value="a"/>
<MouseLeftClick/>
<MouseLeftClick value="double"/>
<MouseLeftClick value="toggle"/>
<MouseLeftClickAtGaze/>
<MouseLeftClickAtGaze value="0"/>
<MouseLeftClickAtGaze value="4"/>
<MouseLeftClickAtGaze value="-1"/>
<MouseMoveToGaze/>
<MouseMoveToGaze value="0"/>
<MouseMoveByDirection value="n"/>
<MouseMoveByDirection value="se,40"/>
<MouseMoveToPoint value="100,200"/>
<Command value="toggleLookToScroll"/>
<OpenPage value="uw_qwerty, true"/>
<ShowLayers value="1,2"/>
<ClosePage/>
<CloseAllPages/>
<CloseOtherPages/>
<GoBack/>
<Speak value="Hello"/>
```

A cell or zone may have **one** action attribute. Multiple actions use child elements.

`<Phase>` children replace that single-shot fire. Mixed leftover action attributes or children, and empty `<Phase/>`, are load errors. The first dwell **activation** enters phase 0; each later activation advances to the next phase (last wraps to first). After an activation, progress holds full through scan grace before the next step clocks — look-away does not start another fill. Dwell progress **pauses** during blink grace. The actions of the phase the cell is in run when dwell **ends** and blink grace expires (look away, or look at another cell). No activation before leave means no fire. Example:

```xml
<Cell id="chip_0" row="0" col="0">
  <Phase command="compose.moveEndOfWord.0"/>
  <Phase command="compose.moveStartOfWord.0"/>
  <Phase command="compose.removeWord.0"/>
</Cell>
```

Each `<Phase>` takes the same action attribute or child actions as a cell.

| Name | `value` |
|------|---------|
| `Send` | key[, Down\|Up[, durationMs]] |
| `MouseLeftClick` / `MouseMiddleClick` / `MouseRightClick` | type: `default` / `double` / `down` / `up` / `toggle` (omit for a default click) |
| `MouseLeftClickAtGaze` / `MouseMiddleClickAtGaze` / `MouseRightClickAtGaze` | zoom: omit/`default` = Settings mag-pick; `0` = dwell-move, no magnify; `N` = N× zoom; `-1` = foresight; `-2` = foresight with bonus zoom |
| `MouseMoveToGaze` | same zoom tokens as click-at-gaze |
| `MouseMoveByDirection` | `n`/`s`/`e`/`w`/`ne`/`nw`/`se`/`sw`[, amount px] — amount omitted uses the mouse-assist step |
| `MouseMoveToPoint` | `x,y` screen coords |
| `Command` | builtin or mapping-profile name |
| `OpenPage` | targetId[, true] — `true` saves a breadcrumb of the current page state |
| `ShowLayers` | layer[, layer…] — replace the source page's visible set (or the root if that page just closed). Not a Page nav action. |
| `ClosePage` | (none) — close the page that owns the cell |
| `CloseAllPages` | (none) — close every attached page; the master root stays. Also disables ComboMouse. |
| `CloseOtherPages` | (none) — close every attached page except the source page. Also disables ComboMouse. |
| `GoBack` | (none) — restore the last breadcrumb |
| `Speak` | TTS text |
| `AHK` | element body / CDATA — written to a temp `.ahk` and started with a local AutoHotkey install (v2 preferred; `#Requires AutoHotkey v1` selects v1). AutoHotkey is not bundled; set `GAZER_AHK` to an exe to override discovery. |

While `compose` is top, or the action’s source page is `compose` or a compose live board (`compose_voices_live`, `compose_history_live`, `compose_item_edit_live`), `Send` and mapping/modifier commands never inject into the OS. Letters go into the internal phrase. `backspace` / `space` / `enter` / `escape` edit or speak; `leftShift` / `Ctrl` / `tab` / arrows are no-ops. `qwerty_main` is unchanged. `compose.*` and `settings.speech.*` still run as builtins.

`<Action send="a"/>` is the same as `<Send value="a"/>`. Legacy `<Action id="Send" value="a"/>` still loads. `Click` / `Move` / `MoveAndClick` still load (old `left`/`gaze`/`up` values) but the names above are the ones to use.

## Example

```xml
<Page id="tools" name="Tools">
  <Grid id="board" desktopMode="true" rows="1" columns="2"
        anchor="Top" offset="0,0" size="800,400" gap="12" margin="16">
    <Cell id="hello" row="0" col="0" label="Speak" speak="Hello"/>
    <Cell id="close" row="0" col="1" label="Close" closePage="true"/>
  </Grid>
</Page>
```
