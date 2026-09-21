# Actions

An **action** is what a cell or zone does when dwell finishes (look away, or look at another cell). The same language is used in Page XML, the page editor Action tab, `Gazer.exe --action …`, and scripts talking back over the named pipe.

This page lists every action type, every value token, and the runtime path each one takes. Command **names** (`toggleLookToScroll`, `backspace`, `settings.dwell.fast`, …) live on [Commands](commands.md).

## When an action runs

1. Gaze stays on a cell through **scan grace**, then through the **activation** sequence. Progress fills; the last step repeats while gaze holds.
2. Dwell **ends** when you look away or at another cell. Blink grace **pauses** progress; it does not start a new fill.
3. After blink grace expires, the cell fires. The host flashes the cell, then `ActionDispatcher` runs its actions **in document order**.
4. Consecutive `ShowLayers` in that list are collected and applied together (one drawer animation). Any other action flushes the pending layers first, then runs.

Inbound `--action` payloads skip dwell. They parse into the same action list and run immediately.

| Extra switch | What happens |
|--------------|----------------|
| `actionLoop="true"` | The first fire **starts** a sticky loop that keeps dispatching the same actions. The next activation of that cell **stops** it. `stopAllActionLoops` (and Close All / Close Other) stop every loop. |
| `<Phase>` children | The cell no longer has a single-shot list. The first dwell **activation** enters phase 0. Each later activation advances (the last wraps to the first). The **current** phase’s actions run when dwell **ends**. Mixed leftover attributes plus `<Phase>` is a load error. Empty `<Phase/>` is a load error. |

Unspecified dwell uses **rapid** timing for `Send`, modifier commands, mapping keys (`backspace`, `space`, `enter`, …), and `compose.backspace` / `compose.deleteWord`. Everything else uses **standard** (settings, navigation, mouse, AHK, Run, assist toggles, composer word chips). Per-cell `scanGrace` / `activation` still override.

## How you write an action

A cell or zone may have **one** action **attribute**. Several actions use **child elements**. Attribute names are camelCase; element names are PascalCase. They are the same action.

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

`<Action send="a"/>` is the same as `<Send value="a"/>`. Legacy `<Action id="Send" value="a"/>` still loads. Old `Click` / `Move` / `MoveAndClick` names still load; the names in the table below are the ones to author.

`Command` and `Run` may also take `args` (`<Command value="…" args="…"/>`, `<Run value="…" args="…"/>`). On `Run`, `args` is extra process argv. On `Command`, if `value` is empty, `args` becomes the command name.

The page editor Action tab is the same catalog: **Type** chooses the element; the fields below it are that type’s `value`.

## Catalog

| Element / attribute | `value` | Runtime |
|---------------------|---------|---------|
| `Send` / `send` | `key[, Down\|Up[, durationMs]]` | Keyboard inject (`KeyStateManager` → Windows `SendInput`) |
| `Command` / `command` | builtin or mapping name | [CommandRegistry](commands.md): exact builtin, then longest prefix, then `default.json` |
| `MouseLeftClick` / `MouseMiddleClick` / `MouseRightClick` | click kind | Click or hold at the **current** cursor |
| `MouseMoveToGaze` | zoom token | Arm dwell-move: next desktop dwell warps the cursor |
| `MouseLeftClickAtGaze` / `MouseMiddleClickAtGaze` / `MouseRightClickAtGaze` | zoom token | Arm dwell-move, then one click of that button |
| `MouseMoveByDirection` | `n`/`s`/`e`/`w`/`ne`/`nw`/`se`/`sw`[, px] | Nudge the cursor now |
| `MouseMoveToPoint` | `[relative,]x,y` | Warp (or relative-move) now |
| `OpenPage` | `targetId[, true]` | Attach a catalog page in front |
| `TogglePage` | `targetId[, true]` | Attach if closed; close if that id is already attached |
| `HostPage` | `fragmentId` or `hostId, fragmentId` | Swap a `src` slot (no second opaque board) |
| `ShowLayers` | `layer[, layer…]` | Replace the source page’s visible layer set |
| `ClosePage` | (empty) | Close the page that owns the cell (the host, if this cell is in a `src` slot) |
| `CloseAllPages` | (empty) | Close every attached page; master root stays. Also disables ComboMouse. |
| `CloseOtherPages` | (empty) | Close every attached page except the source page. Also disables ComboMouse. |
| `GoBack` | (empty) | Restore the last breadcrumb |
| `Speak` | text | Windows SAPI canned utterance |
| `AHK` | element body / CDATA | Temp `.ahk` with a local AutoHotkey install |
| `Run` / `run` | `kind,file[,persist][,key]` | Spawn a Python or AutoHotkey **file** |

---

## Send

`Send` types a key into the focused OS window (or into the [composer](../speech.md) phrase when the source page is captured — see [Composer capture](#composer-capture)).

### Value

```
key
key, Down
key, Up
key, durationMs
```

| Part | Meaning |
|------|---------|
| `key` | What to inject. See [Keys](#keys). |
| `Down` / `Up` | Hold or release. Case-insensitive. |
| `durationMs` | Integer milliseconds. Used only when **Edge is omitted**: key down now, key up after that many ms. `Down` / `Up` ignore duration. |

A comma **key** is `send=","` (a leading comma is the character, not a separator). Optional edge after that: `send=",,Down"`.

```xml
<Cell label="q" send="q"/>
<Cell label="comma" send=","/>
<Cell label="Hold A" send="a,500"/>
<Cell label="Alt down" send="LAlt,Down"/>
<Cell label="Alt up" send="LAlt,Up"/>
```

### What happens

1. Empty `key` is a no-op.
2. If the source page is a composer capture page, the send is consumed there ([Composer capture](#composer-capture)).
3. Otherwise `KeyStateManager` runs:
   - **Edge omitted, duration 0** — `activate`: a **modifier** name cycles Up → Down → LockedDown → Up; any other key is a tap (down+up). After a standard tap, one-shot **Down** modifiers (not LockedDown) are released so `leftShift` then `send="q"` types `Q`.
   - **`Down`** — key down (modifiers enter Down if they were Up).
   - **`Up`** — key up.
   - **duration &gt; 0, no edge** — down now, up after `durationMs`, then release one-shot modifiers.

US QWERTY punctuation that needs Shift (`!`, `:`, `?`, …) injects the physical OEM/digit key and a **transient** Shift when OS Shift is not already held. Letters use the letter virtual-key; whether the OS types `q` or `Q` follows the live Shift slot (`leftShift` / `rightShift` commands, or a `Send` of a Shift name).

Failures toast (`Send failed` or the injector error, for example `Unknown key: …`).

Dwell: **rapid**.

### Keys

**Single character.** ASCII letters and digits become those virtual-keys. These US punctuation characters map to a physical key (and extra Shift when the glyph is the shifted one):

| Character | Physical key |
|-----------|----------------|
| `` ` `` `~` | Oem3 |
| `1` `!` … `0` `)` | `1`…`0` |
| `-` `_` | OemMinus |
| `=` `+` | OemPlus |
| `[` `{` | Oem4 |
| `]` `}` | Oem6 |
| `;` `:` | Oem1 |
| `'` `"` | Oem7 |
| `\` `|` | Oem5 |
| `,` `<` | OemComma |
| `.` `>` | OemPeriod |
| `/` `?` | Oem2 |

Any other **single** character (including non-ASCII) is injected as Unicode `SendInput`.

**Named keys** (case-insensitive). These are virtual-keys, not characters:

| Names | Key |
|-------|-----|
| `Back` / `Backspace` | Backspace |
| `Return` / `Enter` | Enter |
| `Tab` | Tab |
| `Escape` / `Esc` | Escape |
| `Space` | Space |
| `Delete` / `Del` | Delete (extended) |
| `Insert` | Insert |
| `Home` / `End` | Home / End |
| `Prior` / `PageUp` | Page Up |
| `Next` / `PageDown` | Page Down |
| `Left` / `Right` / `Up` / `Down` | Arrows (extended; Shift+Up stays arrow, not Numpad 8) |
| `F1` … `F12` | Function keys |
| `Shift` / `LShift` / `RShift` | Shift |
| `Control` / `Ctrl` / `LControl` / `RControl` | Ctrl |
| `Alt` / `Menu` / `LMenu` / `RMenu` / `LAlt` / `RAlt` | Alt |
| `Win` / `LWin` / `RWin` | Windows key |
| `Oem1`…`Oem7`, `OemMinus`, `OemPlus`, `OemComma`, `OemPeriod` (and `Oem_…` aliases) | OEM keys |

Unknown multi-character names fail (`Unknown key: …`). Prefer mapping commands (`command="backspace"`) for editing keys on keyboards so composer capture can rewrite them.

---

## Command

`Command` looks up a **name**. It does not type the string.

```xml
<Cell icon="keyBackspace" command="backspace"/>
<Cell command="toggleLookToScroll"/>
<Command value="settings.dwell.fast"/>
```

### What happens

1. If the source page is a composer capture page and the name is **not** in the allow-through families (`compose.*`, `speech.*`, `soundboard.*`, `history.*`, `settings.speech.*`, `toggleDwellSuspend` / `suspendDwell` / `resumeDwell`), the command is captured ([Composer capture](#composer-capture)).
2. Otherwise `CommandRegistry.run(name, sourcePageId)`:
   1. **Exact builtin** (shell, assist, settings, composer, …).
   2. Else **longest prefix** handler (`settings.edit.`, `compose.removeWord.`, `speech.voice.`, `headPose.map.`, …).
   3. Else **mapping profile** `resources/mappings/default.json` next to `Gazer.exe`. Each mapping name is a list of inject steps (`keyTap`, `gamepadButton`, …).
3. Unknown names fail (`Command failed: …`).
4. Successful builtins toast `Cmd <name>` except families `compose.*` / `speech.*` / `soundboard.*` / `history.*` / `settings.*` / `headPose.*` / `lookTo.*`.

Dwell: **rapid** for mapping keys and modifiers (`backspace`, `leftShift`, …). **Standard** for settings, theme, speech, history, soundboard, compose (except `compose.backspace` / `compose.deleteWord`), head-pose, look-to, `toggle*`, `mouse*`, `cycleMouse*`, `quitApp`, `openPageEditor`, `openPreview`, `suspendDwell`, `resumeDwell`, `stopAllActionLoops`.

Names, mapping JSON types, and `gamepad.*`: [Commands](commands.md).

---

## Mouse clicks at the cursor

`MouseLeftClick`, `MouseMiddleClick`, `MouseRightClick` act at the **current** pointer. They do not wait for a second dwell.

| `value` | Also accepted | What happens |
|---------|---------------|----------------|
| (empty) / `default` / `single` | | One click. Marks that button released in the mouse-pad hold state. |
| `double` | | Double-click. Marks released. |
| `down` | | Button down (hold). |
| `up` | | Button up. |
| `toggle` | | Flip hold: down if that button is up, up if it is down. `activeState="mouse.leftHold"` (or `.rightHold` / `.middleHold`) paints the cell on. |

```xml
<Cell label="Left" mouseLeftClick=""/>
<Cell label="L×2" mouseLeftClick="double"/>
<Cell label="L Hold" activeState="mouse.leftHold" mouseLeftClick="toggle"/>
```

Dwell: **standard**.

---

## Move to gaze and click-at-gaze

These **arm** dwell-move. The cell itself does not click the desktop. The next time you dwell a point **outside** Gazer chrome, the cursor warps there (and click-at-gaze then clicks).

Arming again while that purpose is already armed **cancels** it.

| Action | After the warp |
|--------|----------------|
| `MouseMoveToGaze` | Cursor sits there |
| `MouseLeftClickAtGaze` / `Middle` / `Right` | One click of that button |

### Zoom `value`

| Token | What happens |
|-------|----------------|
| (empty) / `default` | Follow Settings → Magnify (mag-pick on/off, foresight, zoom). |
| `0` | Dwell-move with **no** magnify window. |
| `2` … `6` (any positive integer) | Force that zoom factor for this arm. |
| `-1` | Foresight: remember a desktop dwell, then zoom that point when Move-to arms. |
| `-2` | Foresight plus the bonus second zoom. |

```xml
<Cell label="Move" mouseMoveToGaze=""/>
<Cell label="Move+L" activeState="mouseLeftClickAtGaze" mouseLeftClickAtGaze=""/>
<Cell label="Pick 4×" mouseLeftClickAtGaze="4"/>
<Cell label="No mag" mouseMoveToGaze="0"/>
```

`mouseMoveToGaze` as a **command** is the same arm (toggle). Click-at-gaze **commands** (`mouseLeftClickAtGaze`, …) exist too; XML actions let you pin zoom per cell.

Dwell: **standard**.

---

## Nudge and warp now

### `MouseMoveByDirection`

| `value` | What happens |
|---------|----------------|
| `n` `s` `e` `w` `ne` `nw` `se` `sw` | Move by the mouse-assist step (Settings / `cycleMouseMoveAmount`). |
| `se,40` | Same compass, **40 px** this time. |

Compass tokens are the same as grid anchors (`Top` = `n`, `BottomRight` = `se`, …).

### `MouseMoveToPoint`

| `value` | What happens |
|---------|----------------|
| `x,y` | Warp to that point. `x` and `y` are [dim tokens](../authoring.md#dimensions): pixels (`100`), proportions (`0.5`), `A_ScreenWidth` / `A_ScreenHeight` arithmetic, `clamp(...)`. |
| `relative,x,y` | Move **by** those amounts (same dim tokens, resolved against the virtual desktop / current screen). |

```xml
<Cell label="Up" mouseMoveByDirection="n"/>
<Cell label="Home" mouseMoveToPoint="0.5,0.5"/>
```

Dwell: **standard**.

---

## Page navigation

Attached pages stack newest in front on the same host window. The master root (`main`) never destroys itself.

### `OpenPage`

`value`: `catalogId` or `catalogId, true`.

- Loads that catalog id (filename stem; `%AppData%\Gazer\layouts` overrides shipped XML).
- Attaches it in front. Opening the **root** id raises the host instead of stacking a second root.
- Second field `true` / `false` (required if present) snapshots a **breadcrumb** of the current stack and layers so `GoBack` can restore it.

### `TogglePage`

Same `value` as `OpenPage`. If that id is already attached, it closes; otherwise it opens.

### `ClosePage`

No value. Closes the page that **owns** the cell. If that page is a fragment sitting in a `src` slot, the **host** board closes instead. Closing the root is rejected (the host is raised).

### `CloseAllPages` / `CloseOtherPages`

No value. Close All drops every attached page. Close Other keeps the source page. Both disable ComboMouse.

### `GoBack`

No value. Restores the last breadcrumb. Fails with a toast if the stack is empty.

### `HostPage`

`value`: `fragmentId` **or** `hostId, fragmentId`.

- Finds a grid with `src="…"` on the host (the source page when `hostId` is omitted; or that catalog page, attaching it if needed).
- Sets the slot to `fragmentId` and rebuilds. The fragment’s cells keep the fragment page id as their live target (`main_settings_speed/dwell_edit`).
- This is how Settings swaps Speed / Magnify / … into one frame. It does **not** stack a second opaque page.

```xml
<Cell label="Keyboard" openPage="qwerty_main"/>
<Cell label="Wide" openPage="uw_qwerty, true"/>
<Cell label="Speed" hostPage="main_settings_host, main_settings_speed"/>
<Cell label="Done" closePage="true"/>
<GoBack/>
```

`closePage="true"` is an attribute form of `ClosePage` (the token is ignored).

Dwell: **standard**.

---

## ShowLayers

`value`: comma-separated layer numbers (`1`, `1,2`, `1,3`). Replaces the **source page’s** visible set. A grid or zone is shown when **any** of its `layers` membership is in that set. Default both sides: `1`.

Typical master page: `1` dock chips, `2` drawer, `3` quit. Keyboard shift/symbol boards are extra layers on the **same** page.

Several `ShowLayers` in one cell are applied together, then the drawer animates: appear when a `drawerMotion` grid is shown, dismiss when it is the last master grid hidden, snap when another master grid remains.

If the source page just closed in this same activation (`ClosePage` then `ShowLayers`), layers apply to the **root**.

Inbound: after `OpenPage` in the same payload, `ShowLayers` applies to the page that just opened (the dispatcher updates the source to the new top).

```xml
<ShowLayers value="1,2"/>
<Cell label="Dismiss" showLayers="1"/>
```

Dwell: **standard**.

---

## Speak

`value` is the utterance. Always **Windows SAPI** (`SpeakKind::Canned`). Composer synthesis (ElevenLabs or SAPI from Settings → Speech) is `compose.speak`.

Empty text still calls speak; non-empty text toasts `Said: …`.

Dwell: **standard**.

---

## AHK (inline)

Element body only (`<AHK>…</AHK>`). The source is written to a temp `.ahk` and started with a **local** AutoHotkey install (v2 preferred). A first line `#Requires AutoHotkey v1` selects v1. `GAZER_AHK` points at an exe to override discovery. AutoHotkey is not bundled.

```xml
<Cell label="MID" icon="WindowMid">
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

Dwell: **standard**. Talk back with `Gazer.exe --action …`. The pipe is Qt duplex; `FileOpen` write-only is unreliable.

---

## Run (Python / AHK files)

`value`: `kind,file[,persist][,key]`

| Field | Options |
|-------|---------|
| `kind` | `python` / `py` or `ahk` / `autohotkey` |
| `file` | Path to a `.py` or `.ahk` file |
| `persist` | `persist` or `true` / `false`. Persist starts **once** (until Gazer quits). |
| `key` | Identity for persist (default: the file path) |

`args` is extra argv (`<Run value="python,scripts/predict.py" args="--once"/>`).

Relative `file` is resolved in order: the **page directory**, `%AppData%\Gazer`, the app `resources` folder. Absolute paths must stay under those roots.

Python is not bundled (`python` / `python3` / `py -3` on PATH, or `GAZER_PYTHON`). Working directory is the script folder. Children see `GAZER_EXE` and `GAZER_ACTION_PIPE`.

```xml
<Cell label="Predict">
  <Run value="python,scripts/predict.py,persist,predict"/>
</Cell>
<Cell label="Snap" run="ahk,scripts/snap.ahk"/>
```

Dwell: **standard**.

---

## Multiple actions and phases

```xml
<Cell label="Close All" icon="CloseAll">
  <CloseAllPages/>
  <ShowLayers value="1,2"/>
</Cell>

<Cell id="chip_0" row="0" col="0">
  <Phase command="compose.moveEndOfWord.0"/>
  <Phase command="compose.removeWord.0"/>
</Cell>
```

Composer word chips use phases: first dwell moves the caret to that word; a later dwell deletes it. After an activation, progress holds full through scan grace before the next step clocks.

---

## Composer capture

While the action’s **source page** is `compose`, `compose_voices_live`, `compose_history_live`, or `compose_item_edit_live`:

| Incoming | What happens |
|----------|----------------|
| `Send` of a single character | Inserted into the phrase (only when `compose` is top, or while renaming an item). |
| `Send` of `space` (named) | Inserts a space in those same cases. Other named Send keys are consumed and ignored. |
| `command="backspace"` / `compose.backspace` | Deletes in the buffer. |
| `command="space"` | Inserts a space. |
| `command="enter"` | Speaks (or saves, while renaming). |
| `command="escape"` | Stops speech if busy; otherwise cancels assign/edit, closes a live overlay, or closes composer. |
| `leftShift`, `tab`, arrows, other mapping keys | Consumed, no OS inject. |
| `compose.*` / `speech.*` / `soundboard.*` / `history.*` / `settings.speech.*` / dwell suspend commands | Run as builtins (not captured). |

`qwerty_main` (and any other non-compose page) still injects into Windows even if composer is open underneath. Caps on the composer keyboard is XML `ShowLayers`, not OS Shift.

---

## Inbound CLI and the pipe

The live process listens on a same-user named pipe `Gazer` (`\\.\pipe\Gazer`; override with `GAZER_ACTION_PIPE`). A second `Gazer.exe` with `--action` forwards to that pipe and exits. Several `--action` flags run in order (joined as lines).

Payload forms:

| Form | Example |
|------|---------|
| `name=value` | `command=toggleLookToScroll` |
| `name value` | `speak Hello there` |
| XML elements | `<OpenPage value="qwerty_main"/><ShowLayers value="2"/>` |
| `#` comment / empty lines | ignored |
| `raise` (or a second exe with **no** `--action`) | Bring the host to front |
| `--editor` on a second instance | Also queues `command=openPageEditor` |

```
Gazer.exe --action openPage=qwerty_main --action showLayers=2
Gazer.exe --action command=toggleLookToScroll
Gazer.exe --action "<Speak value=\"Hello\"/>"
Gazer.exe --action run=python,scripts/predict.py
```

Empty pipe reads are ignored (a liveness probe is not a command). `<Phase>` is not valid inbound.

After `OpenPage` in one payload, later `ShowLayers` apply to the opened page.

---

## Editor fields

Tray → Page editor → select a cell → **Action**. **Type** is the catalog above. Extra fields:

| Type | Fields |
|------|--------|
| Send | Key, Edge (`Down` / `Up` / empty), Hold ms |
| Command | Name (combo of builtins + mapping), Args |
| Click | default / double / down / up / toggle |
| Move to gaze / click-at-gaze | Zoom (default, 0, −1, −2, 2…6) |
| Move by direction | Compass, Amount px (−1 = Settings step) |
| Move to point | X, Y dims |
| Open / Toggle | Target catalog id, Save breadcrumb |
| HostPage | Host page (`(this page)` or an id), Body page |
| ShowLayers | Layer csv |
| Speak | Text |
| AHK | Script body |
| Run | Kind, File, Keep running, Key, Args |
| Loop | **Loop until activated again** (`actionLoop`) |

Empty actions warn before Save and F5.

Schema in the repo: [docs/page-xml.md](https://github.com/AdamRoden/Gazer/blob/master/docs/page-xml.md). Shipped examples: `resources/layouts/main.xml`, `example_keyboard.xml`, `example_mouse.xml`, `example_gamepad.xml`, `compose.xml`.
