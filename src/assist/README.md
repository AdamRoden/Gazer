# assist

Gaze tools on top of the page session.

| File | Role |
|------|------|
| `AssistSession` / `AssistCommands` | Tool lifetime + look-to maps, mag, ComboMouse, dwell-move builtins |
| `LookToMaps` / `LookToScroll` / `LookToOverlay` / `LookToMap.h` / `LtsSpeed.h` / `LtsIndicator.h` / `LtsScrollMode.h` / `LtsMenu.h` | Four analog gaze maps (scroll, mouse, left stick, right stick). Each has its own origin, deadzone, 0–100% ramp, 100% outside the ramp until an optional outer deadzone, and an optional hub pie. Live HUD is the stretching orb; ring-editor Preview is a separate overlay owned by `LookToMaps`. All four can run at once. |
| `HeadPoseMapper` | Analog maps from tracker head pose (settings-backed). Pauses mouse/scroll/joy/commands while gaze is on Gazer boards. Gaze offset is not applied to board hit-tests. |
| `ComboMouse` / `ComboMouseHit.h` / `PieOverlay` | Directional mouse pad. Shared command pie (ComboMouse + LTS) |
| `MouseDwellMove` | Dwell to warp cursor. Overlays in `MouseDwellMove_p.h`; mag-pick in `MouseDwellMoveMag.cpp` |
| `MouseAssistState` | Shared pointer-assist flags |
| `GazeReticle` / `GazeMouseFollow` | Marker / cursor-follows-gaze |
| `GazeDwellTracker.h` / `GazeFollowProfile.h` / `GazeFollowStickiness.h` / `ForesightMemory.h` / `MagLayout.h` | Shared helpers |
| `ActionLoopService` | Sticky command series |
| `AhkLauncher` | Temp `.ahk` + local AutoHotkey |
| `SidecarHost` | `<Run>` Python / AHK files (one-shot or persist). Path jail: page dir, AppData, app `resources`. |
| `ChildProcess` | Shared spawn/kill for AHK and `<Run>` children. `killChildren` disconnects first so parent dtors are safe. |
| `TtsService` | SAPI voice |
| `SpeechEngine` | Canned = SAPI. Composed = Eleven v4 Turbo when model+key+voiceId, else SAPI. Turbo audio plays as PCM arrives. Abort + generation. Three Eleven failures latch SAPI until model, voice, or API key changes. Speak never types. |
| `ClipPlayer` | GUI-thread `QMediaPlayer` for a saved WAV or MPEG clip. `play()` true means the engine owns the clip; `failed()` is the SAPI fallback; `stopped()` always means finished. Gain > 1× is a `ClipBoost` preprocess, then the same play path. Manual: `tests/fixtures/speech/beep.mp3` |
| `ClipBoost` | Decode clip → Int16 WAV with `AudioGain`. Not a player. |
| `AudioGain` | Int16 PCM scale + clamp for clip boost (1–5×) |
| `SystemVolume` / `VolumeBoard` | WASAPI default-device master volume (0–100, step 10). The shell owns the endpoint. `VolumeBoard` stamps any `caption="volume"` slider and maps gaze on it, including passive sliders that `PageHit::at` skips. The label refresh waits until the current gaze sample has been classified. |
| `ComposeBuffer` | Composer phrase, caret, chips, undo (800 ms coalesce). `load()` replaces the phrase and drops history for name-edit. No UI. |
| `ComposeCommands` | `compose.*` / `speech.*` builtins and prefixes |
| `VoiceCatalog` | Parse/filter/sort ElevenLabs voice cache. No network. |
| `SoundboardStore` | `boards.json` + `clips/`. Starters, assign, clip import, eviction. |
| `PcmStreamPlayer` | GUI-thread `QAudioSink` for live int16 PCM. Speed resamples; boost uses `AudioGain`. |
| `SpeechHistory` | Last 50 composed utterances (`history.json` + optional WAV or MPEG). |
| `ElevenRequest` | ElevenLabs request policy (v4 Turbo, tags kept, local speed). No network. |
| `SpeechSecrets` | DPAPI ElevenLabs API key (`secrets/eleven.dpapi`) |
| `DialogueSocket` | v4 Turbo text-to-dialogue WebSocket. PCM chunks, abortable. Upgrade and server JSON parse without a socket. |
| `ElevenClient` | GUI-thread HTTP GET `/v1/voices` (15 s). Speech forwards to `DialogueSocket`. |
