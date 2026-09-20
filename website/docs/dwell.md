# How dwell works

Dwell is how you activate a cell or zone: look at it until progress completes, then look away (or at another cell). There is no click unless you ask for one.

1. Gaze lands on a cell or zone.
2. **Scan grace** waits until you are stably on-target (default 100 ms; Settings → Speed → Advanced).
3. **Activation** is a sequence of step times in milliseconds. Progress fills through each step. The **last** step repeats while gaze holds.
4. The cell fires when dwell **ends** and blink grace expires (look away, or look at another cell). Blink grace **pauses** progress; look-away does not start another fill until grace expires.

## Rapid vs standard

Two sequences share scan grace:

| Sequence | Used for |
|----------|----------|
| **Rapid** | `Send`, composer typing, modifiers, mapping keys (`backspace`, `space`, `enter`, …) |
| **Standard** | Settings, navigation, mouse, AHK, assist toggles, composer word chips |

Both live under Settings → **Speed** (Slow / Normal / Fast / Custom). Per-cell XML can override `scanGrace`, `dwellGrace`, and `activation`.

## Pause dwell

**Pause dwell** on the drawer, or the **Sleep** chip, suspends dwell everywhere except `suspendExempt` unlock targets (the Main chip also resumes). A dim screen border leaves a gap at those targets. Suspend and resume hold 500 ms before a new dwell can start.

`toggleDwellSuspend`, `suspendDwell`, and `resumeDwell` are different commands, not aliases of each other.

## Overlap

When pages overlap, the topmost page’s grid is opaque: gaze and paint do not fall through. Shell zones (dock chips) still win over everything. A cell on a buried grid does not come forward when dwelled; only the unoccluded part hit-tests.

Progress shape (radial, pie, fill directions) is Settings → **Indicators**.
