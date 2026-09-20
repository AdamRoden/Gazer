# Authoring pages

Boards are XML. The runtime and the [page editor](editor.md) read and write the same format. You do not write C++ to add a keyboard, a speech board, or an AHK cell.

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

A cell or zone may have **one** action attribute. Multiple actions use child elements.

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

| Name | Typical `value` |
|------|-----------------|
| `Send` | Key to type (`q`, `a`, `,`). Optional `Down` / `Up` / duration. |
| `Command` | Builtin or mapping name (`toggleLookToScroll`, `backspace`, `settings.dwell.fast`). Catalog: [Commands](reference/commands.md). |
| `OpenPage` | `targetId[, true]` — `true` saves a breadcrumb so **GoBack** can restore. |
| `ShowLayers` | Which grid/zone layers are visible on this page (`1,2`). |
| `ClosePage` / `CloseAllPages` / `CloseOtherPages` / `TogglePage` / `GoBack` | Page stack. |
| `Speak` | Windows SAPI text (not the composer / ElevenLabs path). |
| `AHK` | Element body — temp `.ahk` run with a local AutoHotkey install. |
| `Run` | `kind,file[,persist][,key]` — spawn a **file** with local Python (`.py`) or AutoHotkey (`.ahk`). |

Mouse clicks, dwell-move, and click-at-gaze have matching action names (`MouseLeftClick`, `MouseMoveToGaze`, …). See the [full schema](https://github.com/AdamRoden/Gazer/blob/master/docs/page-xml.md) for every attribute.

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

The live process owns a local pipe named `Gazer`. A second `Gazer.exe` forwards `--action` and exits. The payload is the same action language as page XML.

```
Gazer.exe --action openPage=qwerty_main --action showLayers=2
Gazer.exe --action command=toggleLookToScroll
Gazer.exe --action "<Speak value=\"Hello\"/>"
```

A second `Gazer.exe` with no `--action` sends `raise` (host to front). After `OpenPage` in one payload, `ShowLayers` applies to the opened page.

## Full schema

Dimensions, colors, roles, visibility, phases, `HostPage` slots, and every action field: [docs/page-xml.md](https://github.com/AdamRoden/Gazer/blob/master/docs/page-xml.md). Shipped examples: `resources/layouts/main.xml`, `example_keyboard.xml`, `example_mouse.xml`, `compose.xml`.
