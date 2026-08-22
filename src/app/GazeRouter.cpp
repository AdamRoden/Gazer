#include "app/GazeRouter.h"

#include "assist/AssistSession.h"
#include "assist/GazeMouseFollow.h"
#include "assist/GazeReticle.h"
#include "assist/LookToScroll.h"
#include "assist/MouseDwellMove.h"
#include "layout/LayoutInstanceManager.h"
#include "layout/PageSession.h"
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
    if (freeAim) {
        if (m_pages) {
            m_pages->leaveGaze();
        }
        if (m_instances) {
            m_instances->leaveActiveGaze();
        }
    } else {
        const bool overPages = m_pages && m_pages->hasRoot() && m_pages->onGaze(point);
        if (overPages) {
            overBoard = true;
            if (m_instances) {
                m_instances->leaveActiveGaze();
            }
        } else if (m_instances && m_instances->onGaze(point)) {
            overBoard = true;
            if (m_pages) {
                m_pages->leaveGaze();
            }
        }
    }

    const bool pauseBackgroundAssist = overBoard || freeAim;

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
        m_mouseDwell->onBackgroundGaze(point, overBoard);
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
