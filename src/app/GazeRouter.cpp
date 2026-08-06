#include "app/GazeRouter.h"

#include "assist/LookToScroll.h"
#include "assist/MouseDwellMove.h"
#include "layout/LayoutInstanceManager.h"
#include "ui/DockRevealOverlay.h"
#include "ui/MagnifierOverlay.h"

namespace gazer {

void GazeRouter::dispatch(const GazePoint& point)
{
    // Prefer visible boards over the dock reveal strip. Strip and dock chip share
    // the bottom-left corner; the strip must never cancel Main ▶ dwell via
    // leaveActiveGaze when the chip is already revealed and under gaze.
    bool overBoard = false;
    if (m_instances) {
        overBoard = m_instances->onGaze(point);
    }

    bool overDockReveal = false;
    if (m_dockReveal && m_dockReveal->isEnabledReveal()) {
        if (overBoard) {
            // Board wins — cancel strip dwell so it cannot fire while chip is under gaze.
            GazePoint away;
            away.valid = true;
            away.x = -1.0e6;
            away.y = -1.0e6;
            away.timestampMs = point.timestampMs;
            m_dockReveal->onGaze(away);
        } else {
            overDockReveal = m_dockReveal->containsGaze(point);
            m_dockReveal->onGaze(point);
        }
    }

    // LTS / mouse-move pause over boards so keys don't scroll or warp the cursor.
    // Magnifier keeps tracking — boards are capturable so the lens can show keys.
    const bool blockedAssist = overBoard || overDockReveal;

    if (m_lookToScroll) {
        m_lookToScroll->onGaze(point, blockedAssist);
    }
    if (m_mouseDwell) {
        m_mouseDwell->onGaze(point, blockedAssist);
    }
    // Magnifier always tracks (including over boards). Lens is visual-only —
    // dwell hit-tests stay in real screen coordinates under the lens.
    if (m_magnifier) {
        m_magnifier->onGaze(point);
    }
}

} // namespace gazer
