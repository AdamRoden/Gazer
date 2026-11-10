# assist

Gaze tools on top of the page session.

| File | Role |
|------|------|
| `AssistSession` / `AssistCommands` | Tool lifetime + look-to maps, mag, ComboMouse, dwell-move builtins |
| `LookToMaps` / `LookToScroll` / `LookToOverlay` / `LookToMap.h` / `LtsSpeed.h` / `LtsIndicator.h` / `LtsScrollMode.h` / `LtsMenu.h` | Four analog gaze maps (scroll, mouse, left stick, right stick). Each has its own origin, deadzone, 0–100% ramp, 100% outside the ramp until an optional outer deadzone, and an optional hub pie. Live HUD is the stretching orb; ring-editor Preview is a separate overlay owned by `LookToMaps`. All four can run at once. |
| `GazeTrack.h` | Shared slider and scrollbar drag, plus `GazeTrack::Lock`, the arm clock those tracks share. After the lock-on ring (`kTrackLockMs`, or the cell's `scanGrace` when set), the thumb stays put until gaze leaves it by 40 px. Speed ramps from the thumb to the end of the track in that direction, so either end is full speed (3 notches/s) and a look partway is slower. The rest band shrinks when the thumb is already near that end. Volume, list scrollbars, and the color hue/opacity tracks use it. Look-to-scroll defaults stay put. The head-pose curve and the color field stay absolute. |
| `HeadPoseMapper` | Analog maps from tracker head pose (settings-backed). Pauses mouse/scroll/joy/commands while gaze is on Gazer boards. Gaze offset is not applied to board hit-tests. |
| `ComboMouse` / `ComboMouseHit.h` / `PieOverlay` | Directional mouse pad. Shared command pie (ComboMouse + LTS) |
| `MouseDwellMove` | Dwell to warp cursor. Overlays in `MouseDwellMove_p.h`; mag-pick in `MouseDwellMoveMag.cpp` |
| `MouseAssistState` | Shared pointer-assist flags |
| `GazeReticle` / `GazeMouseFollow` | Marker / cursor-follows-gaze |
| `GazeDwellTracker.h` / `GazeFollowProfile.h` / `GazeFollowStickiness.h` / `MagLayout.h` | Shared helpers |
| `ActionLoopService` | Sticky command series |
| `AhkLauncher` | Temp `.ahk` + local AutoHotkey |
| `SidecarHost` | `<Run>` Python / AHK files (one-shot or persist). Path jail: page dir, AppData, app `resources`. |
| `ChildProcess` | Shared spawn/kill for AHK and `<Run>` children. `killChildren` disconnects first so parent dtors are safe. |
| `TtsService` | SAPI voice |
| `SpeechEngine` | Canned = SAPI. Composed = Eleven v4 Turbo when model+key+voiceId, else SAPI. Turbo audio plays as PCM arrives. Abort + generation. Three Eleven failures latch SAPI until model, voice, or API key changes. Speak never types. |
| `ClipPlayer` | GUI-thread `QMediaPlayer` for a saved WAV or MPEG clip. `play()` true means the engine owns the clip; `failed()` is the SAPI fallback; `stopped()` always means finished. Gain > 1× is a `ClipBoost` preprocess, then the same play path. Manual: `tests/fixtures/speech/beep.mp3` |
| `ClipBoost` | Decode clip → Int16 WAV with `AudioGain`. Not a player. |
| `AudioGain` | Int16 PCM scale + clamp for clip boost (1–5×) |
| `AudioOutput` | Playback-device list and the saved `speechOutputId`. Empty id is the Windows default. SAPI matches the endpoint id on the classic audio-output token |
| `SystemVolume` / `VolumeBoard` | WASAPI master volume of the Speak playback device (0–100, step 10). The device is `AudioOutput::resolveDevice` (`speechOutputId`). Empty id is the Windows default. A missing device follows that default until `QMediaDevices::audioOutputsChanged`. The shell owns the endpoint. `VolumeBoard` stamps any `caption="volume"` slider and maps gaze on it. The slider stays passive. `GazeTrack::Lock` waits `kTrackLockMs` (or the cell's `scanGrace` when set) before `GazeTrack` may drag the level; the arming ring is the slider scrub. The label refresh waits until the current gaze sample has been classified. |
| `ComposeBuffer` | Composer phrase, caret, chips, undo (800 ms coalesce). `load()` replaces the phrase and drops history for name-edit. No UI. |
| `ComposeCommands` | `compose.*` / `speech.*` builtins and prefixes |
| `VoiceCatalog` | Parse/filter/sort ElevenLabs voice cache. No network. |
| `SoundboardStore` | `boards.json` + `clips/`. Starters, assign, clip import, eviction. |
| `PcmStreamPlayer` | GUI-thread `QAudioSink` for live int16 PCM. Identity playback is rate 1. Boost uses `AudioGain`. |
| `PcmFx` | Pitch resample, then pitch-preserving speed. Used when either is not 1, before History stores the clip. |
| `SpeechHistory` | Last 50 composed utterances (`history.json` + optional WAV or MPEG). |
| `ElevenRequest` | ElevenLabs request policy (v4 Turbo, breath tags, local speed and pitch). No network. |
| `SpeechSecrets` | DPAPI ElevenLabs API key (`secrets/eleven.dpapi`) |
| `DialogueSocket` | v4 Turbo text-to-dialogue WebSocket for one untagged identity phrase. PCM chunks, abortable. |
| `ElevenHttp` | One HTTP PCM stream per breath, prefetching the next. Used for tagged phrases and for baked speed or pitch. |
| `ElevenClient` | GUI-thread `GET /v2/voices` (account, then defaults, 15 s per page). Cache field `catalog` is `v2`. Speech goes to `DialogueSocket` or `ElevenHttp`. |
