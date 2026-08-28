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
    // One page hit-test per sample. Master page stays dwellable during aim;
    // attached layouts yield so mag-pick / place-cursor can run over them.
    PageSession::GazeHit hit;
    if (m_pages) {
        hit = m_pages->classifyGaze(point);
    }

    const bool freeAim = m_session && m_session->freesScreenForAim();
    // Combo's HWND sits in front of PageHostWindow; a pie over the dock still wins.
    const bool overCombo = m_session && m_session->overlayHasGazePriority() && m_comboMouse
                           && m_comboMouse->containsGaze(point);

    bool overBoard = false;
    if (overCombo) {
        if (m_pages) {
            m_pages->leaveGaze();
        }
    } else if (m_pages && m_pages->hasRoot()) {
        const auto scope = freeAim ? PageSession::GazeScope::MasterAndActivator
                                   : PageSession::GazeScope::All;
        overBoard = m_pages->feedGaze(point, hit, scope);
    }

    const bool dwellOff = m_pages && m_pages->isDwellSuspended();
    const bool pauseBackgroundAssist = overBoard || hit.overMaster || freeAim || dwellOff;

    if (m_gazeReticle) {
        m_gazeReticle->onGaze(point);
    }
    if (m_gazeFollow) {
        const bool pauseFollow = dwellOff || (m_session && m_session->pausesGazeFollow());
        m_gazeFollow->onGaze(point, /*pauseInput=*/pauseFollow);
    }
    if (m_lookToScroll) {
        m_lookToScroll->onGaze(point, pauseBackgroundAssist);
    }
    if (m_comboMouse) {
        m_comboMouse->onGaze(point, dwellOff || (pauseBackgroundAssist && !overCombo));
    }
    if (m_mouseDwell) {
        m_mouseDwell->onBackgroundGaze(point, overBoard || hit.overMaster);
        const bool pauseAim = dwellOff || (hit.overMaster && !overCombo);
        m_mouseDwell->setPaused(pauseAim);
        if (!pauseAim) {
            m_mouseDwell->onGaze(point);
        }
    }
    if (m_magnifier) {
        m_magnifier->onGaze(point);
    }
}

} // namespace gazer
