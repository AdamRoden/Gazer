#include "app/GazeRouter.h"

#include "assist/AssistSession.h"
#include "assist/GazeMouseFollow.h"
#include "assist/GazeReticle.h"
#include "assist/LookToScroll.h"
#include "assist/MouseDwellMove.h"
#include "layout/LayoutInstanceManager.h"
#include "ui/DockRevealOverlay.h"
#include "ui/MagnifierOverlay.h"

namespace gazer {

void GazeRouter::dispatch(const GazePoint& point)
{
    // Policy lives on AssistSession only (no tool-flag special cases).
    // Gaze click loop stays armed over empty desktop, but must not steal board
    // dwell — otherwise other items cannot progress (or even turn the loop off).
    const bool magPick = m_mouseDwell && m_mouseDwell->isMagPointPhase();
    const bool clickLoopYields = m_mouseDwell && m_mouseDwell->isClickLoop() && !magPick;
    const bool freeAim = m_session && m_session->freesScreenForAim() && !clickLoopYields;

    bool overBoard = false;
    if (m_instances) {
        if (freeAim) {
            m_instances->leaveActiveGaze();
        } else {
            overBoard = m_instances->onGaze(point);
        }
    }

    bool overDockReveal = false;
    if (m_dockReveal && m_dockReveal->isEnabledReveal()) {
        if (overBoard || freeAim) {
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

    // Pause LTS while over UI or while full-screen aiming.
    // Gaze→mouse keeps tracking over boards (product intent); still pause during
    // free-aim mouse-dwell / mag-pick so those tools own the cursor.
    const bool pauseBackgroundAssist = overBoard || overDockReveal || freeAim;

    if (m_gazeReticle) {
        m_gazeReticle->onGaze(point);
    }
    if (m_gazeFollow) {
        m_gazeFollow->onGaze(point, /*pauseInput=*/freeAim);
    }
    if (m_lookToScroll) {
        m_lookToScroll->onGaze(point, pauseBackgroundAssist);
    }
    if (m_mouseDwell) {
        if (clickLoopYields && overBoard) {
            m_mouseDwell->setPaused(true);
        } else {
            m_mouseDwell->setPaused(false);
            m_mouseDwell->onGaze(point);
        }
    }
    if (m_magnifier) {
        m_magnifier->onGaze(point);
    }
}

} // namespace gazer
