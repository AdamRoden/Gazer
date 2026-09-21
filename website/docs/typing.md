# Keyboards and typing

Shipped boards:

| Page | Role |
|------|------|
| `qwerty_main` | Full QWERTY plus edge mouse / look-to-scroll / window AHK strips |
| `example_keyboard` | Compact 3×12 letters / shift / symbols |
| `uw_qwerty` | Wide QWERTY (drawer **More**) |

Letter cells use **`Send`**: `send="q"` taps `q` into the focused OS window (Windows `SendInput`). Optional edge and hold: `send="a,Down"`, `send="a,500"`. A comma key is `send=","`. Named keys (`Enter`, `Tab`, `F1`…`F12`, OEM names) and US shifted punctuation (`!` injects `1` plus a transient Shift) are listed under [Actions — Send](reference/actions.md#send).

Editing keys on the shipped boards are **`Command`** mapping names, so the composer can intercept them: `backspace`, `tab`, `enter`, `space`, `escape`, `delete`, arrows, `home` / `end` / `pageUp` / `pageDown`. Those names fall through to `resources/mappings/default.json` (`keyTap` of the matching virtual-key). `clearPhrase` is Ctrl+A then Backspace; `speakPhrase` is Ctrl+Enter.

Modifier cells (`leftShift`, `leftCtrl`, `leftAlt`, `leftWin`, and the right-hand names) are **commands** that **cycle** OS hold: Up → Down → LockedDown → Up. After a standard `Send` tap, a one-shot Down modifier is released, so `leftShift` then `send="q"` types `Q`. `releaseModifiers` clears every slot. `activeState="mod.shift"` (also `.ctrl` / `.win` / `.alt`, and `.locked` variants) paints the key on.

Shift and symbol layers are extra grids on the same page (`layers="2"`, `layers="3"`). A Shift cell runs `ShowLayers` instead of holding a physical shift key for the letter grid. Caps on the composer keyboard is the same pattern.

While the action’s **source page** is the composer (`compose` or a compose live board), `Send` and mapping/modifier commands are captured into the internal phrase. `qwerty_main` still types into Windows. Details: [Actions — Composer capture](reference/actions.md#composer-capture).

See [Authoring pages](authoring.md) for XML, [Actions](reference/actions.md) for every `Send` / `Command` option, [Commands](reference/commands.md) for the name catalog, and [Speech](speech.md) for the composer keyboard.
