#include "app/GazeRouter.h"

#include "assist/AssistSession.h"
#include "assist/GazeMouseFollow.h"
#include "assist/GazeReticle.h"
#include "assist/ComboMouse.h"
#include "assist/LookToScroll.h"
#include "assist/MouseDwellMove.h"
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

    const bool overCombo = m_session && m_session->overlayHasGazePriority() && m_comboMouse
                           && m_comboMouse->containsGaze(point);

    bool overBoard = false;
    if (overCombo) {
        if (m_pages) {
            m_pages->leaveGaze();
        }
    } else if (freeAim) {
        if (m_pages) {
            m_pages->leaveGaze();
            // Still hit-test chrome so Move-to / LTS place do not complete on the
            // board cell that just armed them.
            overBoard = m_pages->hitsChrome(point);
        }
    } else if (m_pages && m_pages->hasRoot()) {
        overBoard = m_pages->onGaze(point);
    }

    const bool dwellOff = m_pages && m_pages->isDwellSuspended();
    const bool pauseBackgroundAssist = overBoard || freeAim || dwellOff;

    if (m_gazeReticle) {
        m_gazeReticle->onGaze(point);
    }
    if (m_gazeFollow) {
        const bool pauseFollow =
            dwellOff || (m_session && m_session->pausesGazeFollow() && !clickLoopYields);
        m_gazeFollow->onGaze(point, /*pauseInput=*/pauseFollow);
    }
    if (m_lookToScroll) {
        m_lookToScroll->onGaze(point, pauseBackgroundAssist);
    }
    if (m_comboMouse) {
        m_comboMouse->onGaze(point, dwellOff || (pauseBackgroundAssist && !overCombo));
    }
    if (m_mouseDwell) {
        m_mouseDwell->onBackgroundGaze(point, overBoard);
        // Pause while gaze is still on Gazer chrome so dwell-to-place cannot
        // fire on the activation cell (LTS / Move-to). Mag-pick's zoom window
        // may overlap a board; keep sampling there.
        const bool pauseAimOnBoard =
            dwellOff || (overBoard && !magPick && (clickLoopYields || freeAim));
        if (pauseAimOnBoard) {
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
