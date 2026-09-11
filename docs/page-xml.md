# Page XML

Pages live in `resources/layouts/*.xml`. **Catalog id = filename stem** (`qwerty_main.xml` → id `qwerty_main`). User copies in `%AppData%\Gazer\layouts` override the same id. Parsed by `PageLoader` into `PageDocument` (`src/layout/`). The page editor reads and writes this format.

Runtime notes for the live session: [src/layout/README.md](../src/layout/README.md). User overview: [README.md](../README.md).

---

## Document tree

```xml
<Page id="…" name="…">
  <Style id="…"/>          <!-- named chrome; omit id for page default -->
  <Dwell id="…"/>          <!-- named timing; omit id for page default -->
  <Zone …/>                <!-- screen-anchored chip -->
  <Grid …>                 <!-- placed rectangle of rows × columns -->
    <Cell …/>
    <SubGrid …>            <!-- nested grid occupying a cell span -->
      <Cell …/>
    </SubGrid>
  </Grid>
</Page>
```

Child order is free. Grids and zones are painted and hit in document order within a page; attached pages stack newest in front.

---

## Runtime rules

| Rule | Behavior |
|------|----------|
| Dims | Integer token = pixels (`150`). Token with `.` or `/` = proportion of the bounds (`0.5`, `1/2`). Arithmetic with `A_ScreenWidth` / `A_ScreenHeight` is pixels (`A_ScreenHeight/9*16`), evaluated against the placement surface passed at resolve time (work area when `desktopMode`). `clamp(value, min, max)` bounds a pixel expression. Grid tracks add `*` / `2*` for leftover space (`rowHeights="80,*,120"`). `rowWeights` integers stay star weights, not pixels. |
| Style / dwell | Page inherits from settings, then overrides per field. Unspecified dwell uses **rapid** for Send / modifiers / mapping keys / composer typing, and **standard** for everything else (including mouse, AHK, and composer word chips). Grids, cells, and zones inherit from the **page** (never from a parent grid). Named `style` / `dwell` plus inline attrs override individual members. Grid resolve then drops `foreground` / `progressStyle` / `progressColor`. |
| Overlap | Topmost attached page’s grid is opaque. Shell grids/zones paint and hit above the rest. A cell on a buried grid does not come forward when dwelled; only the unoccluded part hit-tests. |
| Drawer / quit | Layer membership. Master XML puts dock chips on layer 1, the drawer on 2, quit on 3. `ShowLayers` sets the visible set (Main chip `1,2`; Dismiss `1`; Quit `1,3`). Consecutive ShowLayers in one cell are applied together, then the drawer animates: appear when a `drawerMotion` grid is shown, dismiss when it is the last master grid hidden, snap when another master grid remains. Hidden shell grids do not reserve host space. |
| Auto-close | Idle on an `autoClose` grid or page closes those boards (root never destroys itself). Duration and the master on/off switch are Settings (`layoutAutoClose`, `layoutAutoCloseIdleMs`). `suspendDwell` stops the idle timer; `resumeDwell` restarts it from zero. |
| Zones | Chrome is hidden until dwell progress or activation flash. Engaged dwell includes the on-screen progress strip. |

---

## Dimensions

Used by `offset`, `size`, `dwellOffset`, `dwellSize`, grid tracks, and `MouseMoveToPoint`.

| Token | Meaning | Example |
|-------|---------|---------|
| Integer | Pixels | `150`, `80px` |
| `.` or `/` | Proportion of the reference axis (width for x, height for y) | `0.25`, `1/2` |
| `h` suffix | Proportion of the **height** on either axis (square boards) | `size="0.25h,0.25h"` |
| `A_ScreenWidth` / `A_ScreenHeight` | Pixel arithmetic vs the placement surface | `A_ScreenHeight/9*16` |
| `clamp(v, min, max)` | Bound a pixel expression | `clamp(1.8*A_ScreenHeight, 1080, A_ScreenWidth)` |
| `*` / `2*` | Grid tracks only: share leftover space | `rowHeights="80,*,120"` |

**Pairs** (`offset`, `size`) are `x,y`. Commas inside parentheses do not split the pair:

```xml
size="800,400"
size="0.5,0.27"
size="A_ScreenHeight, 0.27"
size="clamp(1.8*A_ScreenHeight, 1080, A_ScreenWidth), A_ScreenHeight"
```

`desktopMode="true"` uses the virtual desktop as the bounds reference; otherwise the current screen.

---

## Colors

`background`, `foreground`, `border`, `progressColor`:

| Form | Example |
|------|---------|
| Hex | `#RRGGBB`, `#AARRGGBB` (`#aa000000`) |
| Theme role | `background`, `surface`, `accent`, `progress`, `tertiary`, `foreground`, `danger` |
| Accent brand | `red`, `orange`, `yellow`, `green`, `teal`, `blue`, `indigo`, `purple`, `pink` |

Names resolve from the live theme (Settings → Theme). Empty `progressColor` inherits settings.

---

## `<Page>`

| Attribute | Description |
|-----------|-------------|
| `id` | **Required** catalog id (match the filename stem) |
| `name` | Title (defaults to `id`) |
| `master` | Process-lifetime root. Only one. Cannot be live-tested from the editor. |
| `autoClose` | Opts the page into idle close. Duration is Settings `layoutAutoCloseIdleMs`. |
| `showLayers` | Comma-separated layer numbers visible when the page opens. Default `1`. Live `ShowLayers` actions replace this set. |
| chrome / dwell attrs | Override settings per field (`background`, `scanGrace`, `activation`, …). Grids, cells, and zones inherit these. |

Child elements: `<Style>`, `<Dwell>`, `<Zone>`, `<Grid>`.

```xml
<Page id="tools" name="Tools" autoClose="true" radius="16" showLayers="1">
  …
</Page>
```

---

## `<Style>`

Named or anonymous chrome. An unnamed `<Style>` (no `id`) sets the page default. Grids, cells, and zones reference a named style with `style="id"` and may override the same attributes inline. They inherit from the page, never from a parent grid.

| Attribute | Description |
|-----------|-------------|
| `id` | Omit to set the page default style |
| `background`, `foreground`, `border` | Colors (see [Colors](#colors)). Grids ignore `foreground`. |
| `thickness` | Border widths: one value, or `t,r,b,l` |
| `radius` | Corner radii: one value, or `tl,tr,br,bl` |
| `progressStyle` | How dwell progress is drawn. Comma-separated: `radial`, `pie`, `border`, `fill` (center), `fillup`, `filldown`, `fillleft`, `fillright`. Grids ignore this. |
| `progressColor` | Dwell-progress accent. Empty inherits settings. Grids ignore this. |
| `blur` | Frosted-glass blur radius |

The same chrome attributes may be set on `<Page>`, `<Grid>`, `<Cell>`, and `<Zone>`.

```xml
<Style id="chip" background="#99000000" blur="15" radius="900,900,0,0"
       progressStyle="border, fillup"/>
<Style id="quitYes" background="#9b2226" foreground="#ffffff" radius="12"/>
```

---

## `<Dwell>`

Named or anonymous timing. An unnamed `<Dwell>` (no `id`) sets the page default for every cell on the page. Grids, cells, and zones inherit from the page, never from a parent grid. They may reference a named dwell with `dwell="id"` and override individual members inline.

When the page does not set a default, Speed settings supply **rapid** dwell (keys, composer typing, modifiers) or **standard** dwell (settings, navigation, mouse, AHK, assist toggles, composer word chips). Scan grace is a shared advanced Speed setting.

| Attribute | Description |
|-----------|-------------|
| `id` | Omit to set the page default |
| `scanGrace` | Milliseconds on-target before progress starts |
| `dwellGrace` | Blink / look-away grace (ms). Progress **pauses** during this window. |
| `activation` | Comma-separated step times in ms. Last step repeats while gaze holds. `0` fires immediately after scan grace. |

```xml
<Dwell id="vert1" scanGrace="200" activation="200,600"/>
<Cell row="0" col="0" dwell="vert1" command="toggleLookToScroll"/>
<Cell row="0" col="1" scanGrace="80" activation="400" send="a"/>
```

---

## Inheritance

```
Settings (standard or rapid, by action type)
  └── Page (unnamed <Style>/<Dwell> or inline attrs)
        └── Cell / Zone / Grid   via style="id" / dwell="id" + inline overrides
```

Grids never pass chrome or dwell down to their cells. Grid resolve drops `foreground`, `progressStyle`, and `progressColor` (grids paint a surface only).

---

## `<Grid>` / `<SubGrid>`

A Grid is a placed rectangle of rows and columns. `SubGrid` occupies a parent cell span (`row`, `col`, `rowSpan`, `colSpan`) and has the same mesh attributes.

| Attribute | Description |
|-----------|-------------|
| `id` | Optional id |
| `anchor` | `TopLeft`, `Top`, `TopRight`, `Left`, `Center`, `Right`, `BottomLeft`, `Bottom`, `BottomRight`. Top pins top-center to top-center of the reference. |
| `offset`, `size` | `x,y` dim pairs (see [Dimensions](#dimensions)) |
| `desktopMode` | `true` = virtual desktop bounds; else current screen |
| `rows`, `columns` | Cell mesh |
| `gap`, `margin` | Pixels between cells / inside the grid |
| `rowWeights` | Legacy all-star row sizes (`1,2,2` = header half as tall as each content row). Integers are **star weights**, not pixels. Loaded as `*` / `2*` tracks. Writer emits this when every row is a star. |
| `rowHeights`, `columnWidths` | Per-track sizes, XAML GridLength-style. Integer token = pixels (`80`, `80px`). `*` / `2*` share leftover space after fixed tracks. Also accepts dim tokens (`0.25`, `1/4`, `0.25h`, `A_ScreenHeight/20`, `clamp(...)`). Missing tracks are `*`. Pixel tracks that overflow the inner size scale down together. Equal columns when `columnWidths` is omitted. |
| `drawerMotion` | Scale animation when shown/hidden as the master drawer |
| `shell` | Always-on-top layer (dock chrome). Cells inherit shell from their grid. |
| `autoClose` | Idle-close this grid |
| `layers` | Comma-separated layer membership (`1,2`). Default `1`. Visible when any listed layer is in the page's current `showLayers`. Nested subgrids are skipped when the parent is off-layer, so a parent that hosts children on several layers should list all of them (`layers="1,2"`). |
| `style`, `dwell` | Named style/dwell ids, plus inline chrome/dwell attrs |

```xml
<Grid id="board" desktopMode="true" rows="3" columns="12"
      anchor="Bottom" offset="0,0" size="1280,280"
      gap="4" margin="4">
  …
</Grid>

<Grid id="board" desktopMode="true" rows="4" columns="3"
      anchor="Top" offset="0,0"
      size="clamp(1.8*A_ScreenHeight, 1080, A_ScreenWidth), A_ScreenHeight"
      gap="72" margin="90" rowWeights="1,2,2,2">
  …
</Grid>
```

---

## `<Cell>`

Cells sit in a grid. They do **not** take `shell` or `layers` — they follow their grid.

| Attribute | Description |
|-----------|-------------|
| `id` | Optional. Keyboard letter cells often use `ch_<codepoint>_<row>_<col>` (codepoints, not key names). |
| `row`, `col` | 0-based mesh index |
| `rowSpan`, `colSpan` | Default `1` |
| `label` | Primary text (newlines allowed) |
| `icon` | Stem of a file in `resources/icons/svg/` (`menu`, `mouseLeftClick`, `keyTab`). Case-insensitive; trailing `Icon` ignored. Unknown names fall back to the label. |
| `caption` | Secondary line (settings descriptions; scrollbar `offset,visible,total`; volume slider) |
| `role` | Visual / hit role (see [Roles](#roles)) |
| `textStyle` | `caption`, `body`, `title`, `section`, `key` — fill the cell with the glyph |
| `style`, `dwell` | Named ids |
| `visibleWhen` | Predicate (see [Visibility](#visibility-and-accent)) |
| `activeState` | Accent when this key is true (see [Visibility](#visibility-and-accent)) |
| `suspendExempt` | Still a dwell target while global dwell is paused (`dwellExempt` is a legacy alias) |
| `actionLoop` | Repeat the action until activated again |
| `settingKey` | Bind to a settings field (live boards) |
| chrome / dwell / **one** action attr | Inline overrides |

Nested `<SubGrid>` occupies a cell span.

---

## Roles

`role` decides paint and whether the item is a dwell target.

| Role | Dwell target? | Notes |
|------|---------------|--------|
| (omitted) | yes | Ordinary button |
| `toggle` | yes | Independent on/off (switch chrome) |
| `choice` | yes | One-of-a-set (radio chrome). With stamped `background` + `progressColor`, paints as a scheme preview. |
| `swatch` | yes | Round color well |
| `tab` | yes, unless no actions | No actions = current tab (selected, not a target) |
| `slider` | yes if it has a command | Gaze-follow scrub; without actions it is passive |
| `colorfield` | no | HSV saturation×value square (click-at-gaze on the live color picker) |
| `label`, `value`, `display`, `preview` | no | Text / preview only |
| `scrollbar` | no | Vertical gaze track. Caption `offset,visible,total`. Looking along it scrolls. |

---

## Visibility and accent

### `visibleWhen`

Tiny predicate, **not** JavaScript.

| Expression | Show when |
|------------|-----------|
| (omitted) | Always |
| `ident` | Property is true |
| `!ident` | Property is false |

Known session properties:

| Property | True when |
|----------|-----------|
| `expanded` | Any master-page grid is shown (drawer or quit) |
| `dwellSuspend` | Global dwell pause is on |

```xml
<Zone id="mainChip" visibleWhen="!expanded" …/>
```

### `activeState`

Paints the cell as “on” when the resolver key is true. Common keys:

| Key | Meaning |
|-----|---------|
| `dwellSuspend` | Pause dwell |
| `magnifier`, `gazeReticle`, `gazeMouseFollow` | Assist toggles |
| `lookToScroll`, `comboMouse` | LTS / ComboMouse |
| `mouseMoveToGaze`, `mouseLeftClickAtGaze`, … | Armed dwell-move |
| `mouse.leftHold` / `.rightHold` / `.middleHold` | Button hold |
| `mod.shift` / `mod.ctrl` / `mod.win` / `mod.alt` | Modifier held (`mod.shift.locked` for lock) |
| `compose.busy` / `compose.speaking` / `compose.editMode` | Composer |
| `theme.light`, `settings.dwell.fast`, … | Settings / theme commands |

Prefix `!` negates (`activeState="!dwellSuspend"`).

---

## `<Zone>`

Screen-anchored chip (dock Main/Sleep, keyboard edge keys). Same leaf fields as a cell, plus placement.

| Attribute | Description |
|-----------|-------------|
| `anchor`, `offset`, `size` | Same as Grid |
| `desktopMode` | Same as Grid |
| `dwellOffset`, `dwellSize` | Optional off-screen (or larger) dwell hit region vs the painted chip |
| `shell` | Always-on-top layer; cells inherit this from their grid instead |
| `layers` | Same as Grid |

```xml
<Zone id="mainChip" anchor="Bottom" offset="0,0" size="200,100"
      dwellOffset="0,300" dwellSize="300,200"
      style="chip" label="Main" icon="menu"
      suspendExempt="true" visibleWhen="!expanded" shell="true">
  <Command value="resumeDwell"/>
  <ShowLayers value="1,2"/>
</Zone>
```

---

## Layers

Each grid/zone lists membership (`layers="1,2"`). The page has a visible set (`showLayers` on open, then `ShowLayers` actions).

An item is visible if **any** of its layers is in the visible set. Default both sides: `1`.

Typical master page:

| Layer | Content |
|-------|---------|
| 1 | Dock chips (Main, Sleep) |
| 2 | Drawer |
| 3 | Quit confirm |

Keyboard shift/symbol boards are extra layers on the **same** page (`ShowLayers` `1` ↔ `2` ↔ `3` ↔ `4`), not extra `OpenPage`s.

A parent grid that hosts children on several layers must list all of them so the parent is not skipped:

```xml
<Grid id="vert1" layers="1,2">
  <SubGrid id="edge" row="3" col="0" layers="1">…</SubGrid>
  <SubGrid id="keyboard" row="3" col="0" layers="2">…</SubGrid>
</Grid>
```

---

## Actions

Action is generic: the specific thing to do is named as an **attribute** (on `<Action>`, or on the cell/zone when there is only one) or as a **child element**.

A cell or zone may have **one** action attribute. Multiple actions use child elements.

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
<MouseMoveByDirection value="n"/>
<MouseMoveByDirection value="se,40"/>
<MouseMoveToPoint value="100,200"/>
<Command value="toggleLookToScroll"/>
<OpenPage value="uw_qwerty, true"/>
<ShowLayers value="1,2"/>
<ClosePage/>
<CloseAllPages/>
<CloseOtherPages/>
<TogglePage value="example_assist"/>
<GoBack/>
<Speak value="Hello"/>
```

`<Action send="a"/>` is the same as `<Send value="a"/>`. Legacy `<Action id="Send" value="a"/>` still loads. `Click` / `Move` / `MoveAndClick` still load (old `left`/`gaze`/`up` values) but the names above are the ones to use.

| Name | `value` |
|------|---------|
| `Send` | `key[, Down\|Up[, durationMs]]` — tap if edge omitted. A comma key is `send=","` (a leading comma is the key, not a separator). |
| `MouseLeftClick` / `MouseMiddleClick` / `MouseRightClick` | type: `default` / `double` / `down` / `up` / `toggle` (omit for a default click) |
| `MouseLeftClickAtGaze` / `MouseMiddleClickAtGaze` / `MouseRightClickAtGaze` | zoom: omit/`default` = Settings mag-pick; `0` = dwell-move, no magnify; `N` = N× zoom; `-1` = foresight; `-2` = foresight with bonus zoom |
| `MouseMoveToGaze` | same zoom tokens as click-at-gaze |
| `MouseMoveByDirection` | `n`/`s`/`e`/`w`/`ne`/`nw`/`se`/`sw`[, amount px] — amount omitted uses the mouse-assist step |
| `MouseMoveToPoint` | `x,y` screen coords (dim tokens allowed) |
| `Command` | builtin or mapping-profile name (`toggleLookToScroll`, `backspace`, `settings.dwell.fast`, …) |
| `OpenPage` | `targetId[, true]` — `true` saves a breadcrumb of the current page state |
| `TogglePage` | `targetId[, true]` — open if closed, close if this page is already attached |
| `ShowLayers` | `layer[, layer…]` — replace the source page's visible set (or the root if that page just closed). Not a Page nav action. |
| `ClosePage` | (none) — close the page that owns the cell |
| `CloseAllPages` | (none) — close every attached page; the master root stays. Also disables ComboMouse. |
| `CloseOtherPages` | (none) — close every attached page except the source page. Also disables ComboMouse. |
| `GoBack` | (none) — restore the last breadcrumb |
| `Speak` | TTS text (always Windows SAPI; not the composer / ElevenLabs path) |
| `AHK` | element body / CDATA — written to a temp `.ahk` and started with a local AutoHotkey install (v2 preferred; `#Requires AutoHotkey v1` selects v1). AutoHotkey is not bundled; set `GAZER_AHK` to an exe to override discovery. |

`Command` may take `args` (`<Command value="…" args="…"/>`).

Builtins vs mapping fallthrough: [src/app/Commands.md](../src/app/Commands.md).

### Composer capture

While `compose` is top, or the action’s source page is `compose` or a compose live board (`compose_voices_live`, `compose_history_live`, `compose_item_edit_live`), `Send` and mapping/modifier commands never inject into the OS. Letters go into the internal phrase. `backspace` / `space` / `enter` / `escape` edit or speak; `leftShift` / `Ctrl` / `tab` / arrows are no-ops. `qwerty_main` is unchanged. `compose.*` and `settings.speech.*` still run as builtins.

---

## Phases

`<Phase>` children replace that single-shot fire. Mixed leftover action attributes or children, and empty `<Phase/>`, are load errors.

1. The first dwell **activation** enters phase 0.
2. Each later activation advances to the next phase (last wraps to first).
3. After an activation, progress holds full through scan grace before the next step clocks — look-away does not start another fill.
4. Dwell progress **pauses** during blink grace.
5. The actions of the phase the cell is in run when dwell **ends** and blink grace expires (look away, or look at another cell). No activation before leave means no fire.

Each `<Phase>` takes the same action attribute or child actions as a cell.

```xml
<Cell id="chip_0" row="0" col="0">
  <Phase command="compose.moveEndOfWord.0"/>
  <Phase command="compose.removeWord.0"/>
</Cell>
```

Composer word chips use this: first dwell moves the caret to the word; a later dwell deletes it.

---

## AutoHotkey

```xml
<Cell id="mid" row="0" col="0" icon="WindowMid" label="MID">
  <AHK><![CDATA[
  {
  try {
    Title := WinGetTitle("A")
    WinRestore Title
    WinMove -4, -4, 15/16*A_ScreenWidth+8, 0.73*(A_ScreenHeight-32), Title
  } catch {
  }
  }
  ]]></AHK>
</Cell>
```

AHK cells use **standard** dwell. Discovery: local AutoHotkey v2, unless the script starts with `#Requires AutoHotkey v1`, or `GAZER_AHK` points at an exe.

---

## Examples

### Minimal page

```xml
<Page id="tools" name="Tools">
  <Grid id="board" desktopMode="true" rows="1" columns="2"
        anchor="Top" offset="0,0" size="800,400" gap="12" margin="16">
    <Cell id="hello" row="0" col="0" label="Speak" speak="Hello"/>
    <Cell id="close" row="0" col="1" label="Close" closePage="true"/>
  </Grid>
</Page>
```

### Named styles, layers, and multiple actions

```xml
<Page id="main" master="true" name="Dock"
      background="#aa000000" blur="15" radius="16" thickness="0">
  <Style id="chip" background="#99000000" blur="15"
         radius="900,900,0,0" progressStyle="border, fillup"/>
  <Style id="drawer" background="#aa000000"/>

  <Zone id="mainChip" anchor="Bottom" offset="0,0" size="200,100"
        dwellOffset="0,300" dwellSize="300,200"
        style="chip" label="Main" icon="menu"
        suspendExempt="true" visibleWhen="!expanded" shell="true">
    <Command value="resumeDwell"/>
    <ShowLayers value="1,2"/>
  </Zone>

  <Grid id="drawer" desktopMode="false" rows="1" columns="3"
        anchor="Bottom" offset="0,0" size="600,150"
        style="drawer" gap="10" margin="10"
        layers="2" drawerMotion="true" autoClose="true" shell="true">
    <Cell id="open_keyboard" row="0" col="0"
          label="Keyboard" icon="keyboardKeys" openPage="qwerty_main"/>
    <Cell id="dismiss" row="0" col="1"
          label="Dismiss" icon="AlignBottom" showLayers="1"/>
    <Cell id="close_all" row="0" col="2" label="Close All" icon="CloseAll">
      <CloseAllPages/>
      <ShowLayers value="1,2"/>
    </Cell>
  </Grid>
</Page>
```

### Compact keyboard (letters + shift layer)

```xml
<Page id="example_keyboard" name="Keyboard">
  <Grid id="board" desktopMode="true" rows="1" columns="4"
        anchor="Bottom" offset="0,0" size="640,120" gap="4" margin="4">
    <Cell id="ch_113_0_0" row="0" col="0" label="q" send="q"/>
    <Cell id="ch_119_0_1" row="0" col="1" label="w" send="w"/>
    <Cell id="shift" row="0" col="2" icon="KeyShift" showLayers="2"/>
    <Cell id="bksp" row="0" col="3" icon="keyBackspace" command="backspace"/>
  </Grid>
  <Grid id="board_shift" desktopMode="true" rows="1" columns="4"
        anchor="Bottom" offset="0,0" size="640,120" gap="4" margin="4" layers="2">
    <Cell id="ch_81_0_0" row="0" col="0" label="Q" send="Q"/>
    <Cell id="ch_87_0_1" row="0" col="1" label="W" send="W"/>
    <Cell id="shift_s" row="0" col="2" icon="KeyShift" showLayers="1"/>
    <Cell id="bksp_s" row="0" col="3" icon="keyBackspace" command="backspace"/>
  </Grid>
</Page>
```

### Mouse pad cells

```xml
<Cell id="nudge_up" row="0" col="1" icon="ArrowPointingToTop"
      label="Up" mouseMoveByDirection="n"/>
<Cell id="lclick" row="0" col="4" icon="mouseLeftClick"
      label="Left" mouseLeftClick=""/>
<Cell id="ldbl" row="0" col="3" label="L×2" mouseLeftClick="double"/>
<Cell id="ldownup" row="1" col="4" label="L Hold"
      activeState="mouse.leftHold" mouseLeftClick="toggle"/>
<Cell id="move_lclick" row="2" col="3" icon="mouseLeftClickAtGaze"
      label="Move+L" activeState="mouseLeftClickAtGaze"
      mouseLeftClickAtGaze=""/>
```

### Settings-style layout

```xml
<Page id="main_settings" name="Settings" radius="16">
  <Style id="plain" background="#00000000" thickness="0" radius="0"/>
  <Grid id="board" desktopMode="true" rows="2" columns="3"
        anchor="Top" offset="0,0"
        size="clamp(1.8*A_ScreenHeight, 1080, A_ScreenWidth), A_ScreenHeight"
        gap="72" margin="90" rowWeights="1,2">
    <Cell id="page_title" row="0" col="0" colSpan="3"
          label="Settings" role="label" textStyle="title" style="plain"/>
    <Cell id="open_speed" row="1" col="0"
          label="Speed" caption="Presets, dwell, and pointer time"
          icon="timer" openPage="main_settings_speed"/>
    <Cell id="done" row="1" col="1" label="Done" icon="keyboardHide"
          closePage="true"/>
  </Grid>
</Page>
```

Shipped boards to read next: `resources/layouts/main.xml`, `example_keyboard.xml`, `example_mouse.xml`, `compose.xml`.
