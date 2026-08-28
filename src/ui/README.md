# ui

Host surface, board paint, overlays, theme.

| File | Role |
|------|------|
| `PageHostWindow` | Single frameless `QQuickWindow` + painted chrome |
| `BoardPaint` / `ProgressPaint.h` / `ProgressVisuals.h` | Cells, zones, dwell progress |
| `SliderTrack` | Settings color/opacity slider geometry |
| `Theme` / `ThemeScheme` / `PickStyle` | Palettes and pick-window flags |
| `KeySymbols` | OptiKey icons from `resources/icons/key_symbols.json` (do not open that JSON) |
| `GlassBackdrop` | Frosted blur (no WDA toggle on the live host; recapture on geometry/underlay only) |
| `MagnifierOverlay` / `DwellSuspendOverlay` / `OverlaySurface.h` | Overlay HWNDs (owned by the host; always click-through). SendInput: `OverlayInputPassThrough` punches the board host for the gesture. |
| `PreviewWindow` / `PreviewGeometry` / `StlMesh` | Head-pose preview (geometry/shaders in PreviewGeometry) |
| `TrayIcon` / `AppIcon.h` | Tray + brand |
