# assist

Gaze tools on top of the page session.

| File | Role |
|------|------|
| `AssistSession` / `AssistCommands` | Tool lifetime + `toggleLookToScroll` and related builtins |
| `LookToScroll` / `LtsSpeed.h` / `LtsIndicator.h` | Gaze-driven scroll |
| `ComboMouse` / `ComboMouseHit.h` | Directional mouse pad |
| `MouseDwellMove` | Dwell to warp cursor. Overlays in `MouseDwellMove_p.h`; mag-pick in `MouseDwellMoveMag.cpp` |
| `MouseAssistState` | Shared pointer-assist flags |
| `GazeReticle` / `GazeMouseFollow` | Marker / cursor-follows-gaze |
| `GazeDwellTracker.h` / `GazeFollowStickiness.h` / `ForesightMemory.h` / `MagLayout.h` | Shared helpers |
| `ActionLoopService` | Sticky command series |
| `ScriptHost` | `gazer.*` QJS |
| `AhkLauncher` | Temp `.ahk` + local AutoHotkey |
| `TtsService` / `PhraseService` | Speech |
