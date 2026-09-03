# ui

Host surface, board paint, overlays, theme.

| File | Role |
|------|------|
| `PageHostWindow` | Single frameless `QQuickWindow`. Each page is one layer (grids+cells together). Front→back: master, then open pages newest first. HWND band (front→back): GazeReticle, MagnifierOverlay, mag-pick, other assist overlays, this window. Always TOPMOST vs taskbar/other apps. `WinOverlay::restackGazerBand` |
| `BoardPaint` / `ProgressPaint.h` / `ProgressVisuals.h` | Cells, zones, dwell progress |
| `SliderTrack` | Settings color/opacity slider geometry |
| `Theme` / `ThemeScheme` / `PickStyle` | Palettes and pick-window flags |
| `KeySymbols` | Cell/zone icons from `resources/icons/svg/*.svg` (stem = `icon` name) |
| `GlassBackdrop` | Frosted blur (no WDA toggle on the live host; recapture on geometry/underlay only) |
| `MagnifierOverlay` / `DwellSuspendOverlay` / `OverlaySurface.h` | Overlay HWNDs (owned by the board; click-through). Layered in `OverlayLayer`. SendInput: `OverlayInputPassThrough` punches the board host for the gesture. |
| `PreviewWindow` / `PreviewGeometry` / `StlMesh` | Head-pose preview (geometry/shaders in PreviewGeometry) |
| `TrayIcon` / `AppIcon.h` | Tray + brand |
