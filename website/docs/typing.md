# Keyboards and typing

Shipped boards:

| Page | Role |
|------|------|
| `qwerty_main` | Full QWERTY plus edge mouse / look-to-scroll / window AHK strips |
| `example_keyboard` | Compact 3×12 letters / shift / symbols |
| `uw_qwerty` | Wide QWERTY (drawer **More**) |

Letter cells use `send="q"`. Mapping-profile names (`backspace`, `tab`, `enter`, `space`, `escape`) inject through the mapping JSON. Modifier cells (`leftShift`, `leftCtrl`, `leftAlt`, `leftWin`) **cycle** OS hold: Up → Down → LockedDown. `releaseModifiers` clears them.

Shift and symbol layers are extra grids on the same page (`layers="2"`, `layers="3"`). A Shift cell runs `ShowLayers` instead of holding a physical shift key for the letter grid.

While the **composer** is on top, `Send` and mapping keys go into the internal phrase — they never type into the focused OS app. `qwerty_main` is unchanged and still types into Windows.

See [Authoring pages](authoring.md) for `Send`, layers, and modifiers, and [Speech](speech.md) for the composer keyboard.
