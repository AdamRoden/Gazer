# layout

Page XML model and the live dwell session.

| File | Role |
|------|------|
| `PageTypes.h` | AST: `PageDim`, chrome, dwell, actions, grid/zone/document |
| `PageDim.h` / `.cpp` | Token parse, `placeRect`, grid track mesh — **not** the `PageDim` struct |
| `PageBox.h`, `RoundBox.h`, `ChromeBlur.h`, `ProgressStyle.h` | Chrome bits |
| `PageActionParse*` | Action attributes / child elements |
| `PageLoader` / `PageWriter` / `PageEdit` | XML ↔ `PageDocument` |
| `PageCompose` | `src` slots: load fragment pages, find the body grid, cycle check. `PageHit::collect` inlines them. `HostPage` swaps the slot. |
| `PageResolve` | Chrome / dwell inherit (dims → geometry is `PageDimParse`) |
| `PageHit` / `PageDetector` | Gaze pick, overlap. Each page is one layer (grids+cells together). Back-to-front: open pages oldest→newest, then master. Dwell hits only the unoccluded part of a cell. |
| `PageCatalog` | Shipped + `%AppData%\Gazer\layouts` |
| `PageSession*` | Live session. `PageSession.cpp` attach/rebuild; `Nav` open/close pages; `showLayers` + drawer reconcile; `Chrome` drawer motion; `Gaze` dwell/hit/auto-close. Includes `PageHit.h` paint types. Quit confirm is master XML `ShowLayers`. |
| `DwellStateMachine` / `DwellPhase.h` / `InvalidGazeGrace.h` | Dwell timing. Unspecified cells use rapid vs standard from settings (`usesRapidDwell`). Shared blink/scan grace via `setDwellTiming`. `StartHold` is a post-action block (`armDwellStartHold`, 500 ms on suspend/resume). `<Phase>` cells: first activation enters phase 0, later steps wrap last→first; leave after blink grace commits. Progress pauses during blink grace |
| `PageNav.h` | Page lookup, drawer reconcile, dropLayers |

Schema: `docs/page-xml.md`. Action runtime catalog: `website/docs/reference/actions.md`. Command names: `website/docs/reference/commands.md` and `src/app/Commands.md`. Runtime pages: `resources/layouts/*.xml`.
