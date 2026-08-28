# layout

Page XML model and the live dwell session.

| File | Role |
|------|------|
| `PageTypes.h` | AST: `PageDim`, chrome, dwell, actions, grid/zone/document |
| `PageDim.h` / `.cpp` | Token parse, `placeRect` — **not** the `PageDim` struct |
| `PageBox.h`, `RoundBox.h`, `ChromeBlur.h`, `ProgressStyle.h` | Chrome bits |
| `PageActionParse*` | Action attributes / child elements |
| `PageLoader` / `PageWriter` / `PageEdit` | XML ↔ `PageDocument` |
| `PageResolve` | Dims → geometry |
| `PageHit` / `PageDetector` | Gaze pick, overlap. Each page is one layer (grids+cells together). Back-to-front: open pages oldest→newest, then master. Dwell hits only the unoccluded part of a cell. |
| `PageCatalog` | Shipped + `%AppData%\Gazer\layouts` |
| `PageSession*` | Live session. `PageSession.cpp` attach/rebuild; `Attach` placement; `Nav` show/hide; `Chrome` drawer/quit; `Gaze` dwell/hit. Header does not include paint types |
| `DwellStateMachine` / `DwellRegionSpace` / `InvalidGazeGrace.h` | Dwell timing |
| `PageNav.h` | Open/close/show/hide target parse |

Schema: `docs/page-xml.md`. Runtime pages: `resources/layouts/*.xml`.
