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
    // Combo / mag-pick / LTS pie HWNDs sit in front of PageHostWindow; gaze on
    // them still geometrically hits the dock. The overlay owns the sample.
    const bool overCombo = m_comboMouse && m_comboMouse->containsGaze(point);
    const bool overMagPick = m_mouseDwell && m_mouseDwell->containsGaze(point);
    const bool overLts = m_lookToScroll && m_lookToScroll->containsGaze(point);
    const bool overFrontOverlay =
        m_session && m_session->overlayHasGazePriority()
        && (overCombo || overMagPick || overLts);

    bool overBoard = false;
    if (overFrontOverlay) {
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
        m_lookToScroll->onGaze(point, pauseBackgroundAssist && !overLts);
    }
    if (m_comboMouse) {
        m_comboMouse->onGaze(point, dwellOff || (pauseBackgroundAssist && !overCombo));
    }
    if (m_mouseDwell) {
        m_mouseDwell->onBackgroundGaze(point, overBoard || hit.overMaster);
        const bool pauseAim = dwellOff || (hit.overMaster && !overFrontOverlay);
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
