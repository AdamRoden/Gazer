# Speech composer

Drawer **Speak** (`compose.open`) opens `compose.xml`. This is an internal phrase, not OS typing.

- Type on the composer keyboard into a buffer. Word chips: first dwell jumps the caret; later dwells delete the word (`<Phase>`).
- **Speak** synthesizes the buffer. Engine is **ElevenLabs** when a model, DPAPI-stored API key, and voice id are set; otherwise **Windows SAPI**. Three Eleven failures latch SAPI until model, voice, or key changes.
- XML `<Speak value="Hello"/>` is always SAPI (no cloud quota on canned cells).
- **Soundboard**: pin baked MPEG clips onto topic cells; replay without re-synthesis. Store: `%AppData%\Gazer\` (`boards.json`, `clips/`).
- **Freestyle**: saved voices + ElevenLabs v3 audio tags (`[laugh]`, accents, …).
- **History**: last 50 composed utterances; replay, restore, or delete.
- Settings → **Speech**: paste API key from the clipboard (never stored in `settings.json`), model, speed, volume.

While the composer is on top, `Send` and mapping keys (`backspace`, `space`, `enter`, `escape`, modifiers) are captured and never reach the OS. Caps uses XML `ShowLayers`. The full QWERTY board (`qwerty_main`) still types into Windows.

Composer commands (`compose.*`, `speech.*`, `soundboard.*`, `history.*`) are in the [command catalog](reference/commands.md).
