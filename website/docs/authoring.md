# Authoring pages

Boards are XML. The runtime and the [page editor](editor.md) read and write the same format. You do not write C++ to add a keyboard, a speech board, or an AHK cell.

What a cell **does** when dwell finishes is an **action**. Every action type, value token, and runtime path: [Actions](reference/actions.md). Every `command="…"` name: [Commands](reference/commands.md).

## Where files live

| Location | Role |
|----------|------|
| `resources/layouts/*.xml` | Shipped pages. Catalog **id** = filename stem (`qwerty_main.xml` → `qwerty_main`). |
| `%AppData%\Gazer\layouts\` | User copies. A file with the same stem **overrides** the shipped page. Saving a shipped page from the editor writes here and leaves `resources/` unchanged. |

`OpenPage` / catalog ids open those pages on the live host.

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

Put that at `%AppData%\Gazer\layouts\tools.xml` (or next to other pages) and open it with `openPage="tools"`.

## Cells and actions

A cell or zone may have **one** action **attribute**. Several actions use **child elements**. Attribute names are camelCase; element names are PascalCase. They are the same action.

When dwell **ends** (look away or at another cell, after blink grace), the host flashes the cell and runs that list **in order**. Consecutive `ShowLayers` in the list are applied together. `actionLoop="true"` repeats until the cell is activated again. `<Phase>` children replace the single-shot list (first dwell enters phase 0; later dwells wrap; the current phase fires on leave).

```xml
<Cell row="0" col="0" label="q" send="q"/>
<Cell command="toggleLookToScroll"/>
<Cell openPage="uw_qwerty, true"/>

<Send value="a"/>
<Command value="toggleLookToScroll"/>
<OpenPage value="uw_qwerty, true"/>
<ShowLayers value="1,2"/>
<ClosePage/>
<Speak value="Hello"/>
```

| Name | `value` | What happens |
|------|---------|----------------|
| `Send` | `key[, Down\|Up[, durationMs]]` | Type a key into the focused window (`q`, `,`, `Enter`, `a,500`). A comma key is `send=","`. Named keys, Shifted punctuation, and composer capture: [Send](reference/actions.md#send). |
| `Command` | builtin or mapping name | Look up `toggleLookToScroll`, `backspace`, `settings.dwell.fast`, `gamepad.a`, … Exact builtin, then prefix, then `default.json`. [Commands](reference/commands.md). |
| `MouseLeftClick` (also Middle / Right) | `default` / `double` / `down` / `up` / `toggle` | Click or hold at the **current** cursor. Empty value is a default click. |
| `MouseMoveToGaze` | zoom: empty / `0` / `N` / `-1` / `-2` | Arm dwell-move. Next desktop dwell warps the cursor. Empty follows Settings → Magnify. |
| `MouseLeftClickAtGaze` (also Middle / Right) | same zoom tokens | Arm dwell-move, then one click. |
| `MouseMoveByDirection` | `n`/`s`/`e`/`w`/`ne`/`nw`/`se`/`sw`[, px] | Nudge now. Omitted amount uses the mouse-assist step. |
| `MouseMoveToPoint` | `[relative,]x,y` | Warp (or relative-move) now. Dim tokens allowed. |
| `OpenPage` | `targetId[, true]` | Attach that catalog page. `true` saves a breadcrumb for **GoBack**. |
| `TogglePage` | `targetId[, true]` | Open if that id is closed; close if it is already attached. |
| `HostPage` | `fragmentId` or `hostId, fragmentId` | Swap a `src` slot on the host (Settings body). No second opaque page. |
| `ShowLayers` | `layer[, layer…]` | Replace this page’s visible layer set (`1,2`). |
| `ClosePage` | (empty) | Close the page that owns the cell (the host, if this cell is in a `src` slot). |
| `CloseAllPages` / `CloseOtherPages` | (empty) | Drop attached pages (all / except the source). Also disable ComboMouse. |
| `GoBack` | (empty) | Restore the last breadcrumb. |
| `Speak` | text | Windows SAPI canned utterance. Composer synthesis is `compose.speak`. |
| `AHK` | element body | Temp `.ahk` with a local AutoHotkey install. |
| `Run` | `kind,file[,persist][,key]` | Spawn a Python (`.py`) or AutoHotkey (`.ahk`) **file**. `args` is extra argv. |

`<Action send="a"/>` is the same as `<Send value="a"/>`. Legacy `<Action id="Send" value="a"/>` still loads.

Full options (keys, zoom tokens, inbound `--action`, composer capture, editor fields): [Actions](reference/actions.md). Attribute list including chrome, roles, and dims: [docs/page-xml.md](https://github.com/AdamRoden/Gazer/blob/master/docs/page-xml.md).

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

Pairs (`offset`, `size`) are `x,y`. Commas inside parentheses do not split the pair. `desktopMode="true"` uses the virtual desktop as the bounds reference; otherwise the current screen.

## Layers

Each grid/zone lists membership (`layers="1,2"`). The page has a visible set (`showLayers` on open, then `ShowLayers` actions). An item is visible if **any** of its layers is in the visible set. Default both sides: `1`.

Typical master page: layer 1 = dock chips, 2 = drawer, 3 = quit confirm. Keyboard shift/symbol boards are extra layers on the **same** page, not extra `OpenPage`s.

## Dwell on a cell

Unspecified dwell uses **rapid** for Send / modifiers / mapping keys / composer typing, and **standard** for everything else. Override per page or per cell:

```xml
<Dwell id="vert1" scanGrace="200" activation="200,600"/>
<Cell row="0" col="0" dwell="vert1" command="toggleLookToScroll"/>
<Cell row="0" col="1" scanGrace="80" activation="400" send="a"/>
```

`activation` is comma-separated step times in ms. The last step repeats while gaze holds. `0` fires immediately after scan grace.

## AutoHotkey and script files

Inline snippets stay `<AHK>`. `<Run>` starts a real file next to the page (or under `%AppData%\Gazer`). Absolute paths must stay under the page directory, `%AppData%\Gazer`, or the app `resources` folder.

```xml
<Cell id="predict" row="0" col="0" label="Predict">
  <Run value="python,scripts/predict.py,persist,predict"/>
</Cell>
<Cell id="snap" row="0" col="1" label="Snap" run="ahk,scripts/snap.ahk"/>
```

Python is not bundled. Scripts see `GAZER_EXE` and `GAZER_ACTION_PIPE`. Talk back with `Gazer.exe --action …` — do not `FileOpen` the named pipe write-only.

## Drive a running instance

The live process owns a local pipe named `Gazer` (`\\.\pipe\Gazer`; `GAZER_ACTION_PIPE` overrides). A second `Gazer.exe` forwards `--action` and exits. The payload is the same action language as page XML: `name=value` lines, `name value` lines, or XML elements. `#` comments and empty lines are ignored. Several `--action` flags run in order.

```
Gazer.exe --action openPage=qwerty_main --action showLayers=2
Gazer.exe --action command=toggleLookToScroll
Gazer.exe --action "<Speak value=\"Hello\"/>"
Gazer.exe --action run=python,scripts/predict.py
```

A second `Gazer.exe` with no `--action` sends `raise` (host to front). `--editor` on a second instance also queues `command=openPageEditor`. After `OpenPage` in one payload, `ShowLayers` applies to the opened page. Empty pipe reads are ignored.

Forms, composer capture, and `raise`: [Actions — Inbound](reference/actions.md#inbound-cli-and-the-pipe).

## Full schema

Colors, roles, visibility, phases, `HostPage` slots, and every attribute: [docs/page-xml.md](https://github.com/AdamRoden/Gazer/blob/master/docs/page-xml.md). Action runtime: [Actions](reference/actions.md). Command names: [Commands](reference/commands.md). Shipped examples: `resources/layouts/main.xml`, `example_keyboard.xml`, `example_mouse.xml`, `example_gamepad.xml`, `compose.xml`.
