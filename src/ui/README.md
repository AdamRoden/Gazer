# ui

Host surface, board paint, overlays, theme.

| File | Role |
|------|------|
| `PageHostWindow` | Single frameless `QQuickWindow`. Each page is one layer (grids+cells together). Front→back: master, then open pages newest first. HWND band (front→back): GazeReticle, MagnifierOverlay, mag-pick, other assist overlays, this window. Always TOPMOST vs taskbar/other apps. Task Manager needs UIAccess (installed/signed MSI). `WinOverlay::restackGazerBand` |
| `BoardPaint` / `ProgressPaint.h` / `ProgressVisuals.h` | Cells, zones, dwell progress |
| `SliderTrack` | Settings color/opacity slider geometry. Channel `volume` is a compact master-volume bar (percent in the cell label). |
| `ColorField` | HSV saturation×value square (`role="colorfield"`). Handle from `previewColor`. |
| `PoseChart` | Input→output transfer curve (`role="curvefield"`). `role="headpreview"` is a blit of `HeadPreviewRenderer`, not PoseChart. |
| `ScrollBar` | Vertical gaze scrollbar (`role="scrollbar"`, caption `offset,visible,total`). Composer history / voices lists. |
| `Theme` / `ThemeScheme` / `MaterialPalette` / `PickStyle` | Palettes, Material shade generator, pick-window flags |
| `KeySymbols` | Cell/zone icons from `resources/icons/svg/*.svg` (stem = `icon` name). Catalog source: Material Symbols Rounded (`resources/icons/README.md`) |
| `GlassBackdrop` | Frosted blur (no WDA toggle on the live host; recapture on geometry/underlay only) |
| `MagnifierOverlay` / `DwellSuspendOverlay` / `OverlaySurface.h` | Overlay HWNDs (owned by the board; click-through). Layered in `OverlayLayer`. SendInput: `OverlayInputPassThrough` punches the board host for the gesture. |
| `PreviewWindow` / `PreviewGeometry` / `StlMesh` / `HeadPreviewRenderer` | One GL path (`HeadPreviewRenderer`). Tray window paints HUD over it; Head settings cell blits the same renderer. |
| `TrayIcon` / `AppIcon.h` | Tray + brand |
