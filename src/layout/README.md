# layout

Page XML model and the live dwell session.

| File | Role |
|------|------|
| `PageTypes.h` | AST: `PageDim`, chrome, dwell, actions, grid/zone/document |
| `PageDim.h` / `.cpp` | Token parse, `placeRect`, grid track mesh — **not** the `PageDim` struct |
| `PageBox.h`, `RoundBox.h`, `ChromeBlur.h`, `ProgressStyle.h` | Chrome bits |
| `PageActionParse*` | Action attributes / child elements |
| `PageLoader` / `PageWriter` / `PageEdit` | XML ↔ `PageDocument` |
| `PageResolve` | Dims → geometry |
| `PageHit` / `PageDetector` | Gaze pick, overlap. Each page is one layer (grids+cells together). Back-to-front: open pages oldest→newest, then master. Dwell hits only the unoccluded part of a cell. |
| `PageCatalog` | Shipped + `%AppData%\Gazer\layouts` |
| `PageSession*` | Live session. `PageSession.cpp` attach/rebuild; `Attach` placement; `Nav` open/close pages; `showLayers` + drawer reconcile; `Chrome` drawer motion animation; `Gaze` dwell/hit. Header does not include paint types |
| `DwellStateMachine` / `DwellPhase.h` / `DwellRegionSpace` / `InvalidGazeGrace.h` | Dwell timing. Unspecified cells use typing-boost vs standard from settings (`usesTypingBoostDwell`). Shared blink/scan grace via `setDwellTiming`. `<Phase>` cells: first activation enters phase 0, later steps wrap last→first; leave after blink grace commits. Progress pauses during blink grace |
| `PageNav.h` | Page lookup, drawer reconcile, dropLayers |

Schema: `docs/page-xml.md`. Runtime pages: `resources/layouts/*.xml`.
