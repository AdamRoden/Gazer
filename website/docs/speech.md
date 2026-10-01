# Speech composer

Drawer **Speak** (`compose.open`) opens `compose.xml`. This is an internal phrase, not OS typing.

- Type on the composer keyboard into a buffer. A row above the phrase offers the next word or a spelling fix. Dwell a suggestion to insert it. Word chips: first dwell jumps the caret; later dwells delete the word (`<Phase>`).
- Word predictions use a sentence-sized model shipped with Gazer (`resources/predict/model.bin`). A usage-ranked dictionary completes words that are not in the sample phrases. Words you accept are remembered under `%AppData%\Gazer\predict-user.bin`. Turn the row off under Settings → **Speak**.
- **Speak** synthesizes the buffer. Engine is **ElevenLabs v4 Turbo** when that model, a DPAPI-stored API key, and a voice id are set; otherwise **Windows SAPI**. Turbo plays as the audio arrives. Three Eleven failures latch SAPI until model, voice, or key changes.
- XML `<Speak value="Hello"/>` is always Windows SAPI (a canned cell utterance). Composer **Speak** is the command `compose.speak` and uses Settings → **Speak** (SAPI or v4 Turbo).
- **Soundboard**: pin the spoken clip onto topic cells; replay without re-synthesis. Store: `%AppData%\Gazer\` (`boards.json`, `clips/`).
- **Freestyle**: saved voices + ElevenLabs audio tags (`[laugh]`, accents, …).
- **History**: last 50 composed utterances; replay, restore, or delete.
- Settings → **Speak**: paste API key from the clipboard (never stored in `settings.json`), model, speed, boost, word predictions.

While the action’s **source page** is `compose` (or a compose live board), `Send` and mapping/modifier commands are captured into the phrase: letters insert, `backspace` deletes, `space` inserts a space, `enter` speaks, `escape` stops speech or closes. `leftShift` / `tab` / arrows are consumed with no OS inject. Caps uses XML `ShowLayers`. The full QWERTY board (`qwerty_main`) still types into Windows. Capture table: [Actions — Composer capture](reference/actions.md#composer-capture).

Composer commands (`compose.*`, `speech.*`, `soundboard.*`, `history.*`) are in the [command catalog](reference/commands.md). Word chips use `<Phase>`: first dwell moves the caret (`compose.moveEndOfWord.<i>`), a later dwell deletes (`compose.removeWord.<i>`).
