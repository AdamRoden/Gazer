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
| `PageHit` / `PageDetector` | Gaze pick, overlap |
| `PageCatalog` | Shipped + `%AppData%\Gazer\layouts` |
| `PageSession*` | Attach, nav, drawer/quit chrome (`PageSession.h` is the API) |
| `DwellStateMachine` / `DwellRegionSpace` / `InvalidGazeGrace.h` | Dwell timing |
| `PageNav.h` | Open/close/show/hide target parse |

Schema: `docs/page-xml.md`. Runtime pages: `resources/layouts/*.xml`.
