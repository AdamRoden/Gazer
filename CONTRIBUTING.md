# Contributing

Gazer is GPL-3.0. Build instructions are in the [README](README.md). User-facing behavior is in [website/docs](website/docs/).

## Tests

Configure with Qt 6 (Core, Gui, Widgets, OpenGL, Quick, Svg, Network, Multimedia, Test), then:

```powershell
cmake --build build --target GazerPageTests GazerDwellTests GazerSpeechTests GazerPredictTests
ctest --test-dir build --output-on-failure
```

Windows CI runs that on every pull request. It does not need a Tobii device or `tobii_stream_engine.dll`. `-DGAZER_USE_TOBII=OFF` builds without the Stream Engine loader.

## Reporting a bug

From Settings → **Setup** → **Open log folder**, or the tray **Open log folder**:

- `gazer.log` for this session
- the newest `gazer-*.log` if you already restarted
- the newest file in `%LOCALAPPDATA%\Gazer\crashes` after a crash or a hung-window takeover

Do not paste an ElevenLabs API key. Say what you were dwelling, whether gaze or head tracking had stalled, and whether an upgrade had just run. Settings live in `%AppData%\Gazer\settings.json` (backups: `settings.json.bak`, `settings.vN.json.bak`, `settings.bad-*.json`).

## MSI smoke (not in CI)

Needs a Windows machine with WiX and the ability to install a per-machine MSI. The hosted CI job builds and tests the app; it does not install the MSI.

1. Install the MSI. Set a non-default dwell time in Gazer and quit.
2. Install the same MSI again (repair / same version) and install a newer build (upgrade). `settings.json` must still contain that dwell time.
3. Install with `RESETUSERSETTINGS=1`. The next launch should be factory defaults, and an in-app reset should leave `settings.json.bak`.
4. Confirm the installed `Gazer.exe` is still `uiAccess=true` (the build-tree exe stays `false`).
