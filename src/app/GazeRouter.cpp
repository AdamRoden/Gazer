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
    const bool freeAim = m_session && m_session->freesScreenForAim();

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

    // Pause LTS / gaze-follow while over UI or while full-screen aiming.
    const bool pauseBackgroundAssist = overBoard || overDockReveal || freeAim;

    if (m_gazeReticle) {
        m_gazeReticle->onGaze(point);
    }
    if (m_gazeFollow) {
        m_gazeFollow->onGaze(point, pauseBackgroundAssist);
    }
    if (m_lookToScroll) {
        m_lookToScroll->onGaze(point, pauseBackgroundAssist);
    }
    if (m_mouseDwell) {
        m_mouseDwell->onGaze(point);
    }
    if (m_magnifier) {
        m_magnifier->onGaze(point);
    }
}

} // namespace gazer
