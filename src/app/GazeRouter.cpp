#include "app/GazeRouter.h"

#include "assist/AssistSession.h"
#include "assist/GazeMouseFollow.h"
#include "assist/GazeReticle.h"
#include "assist/ComboMouse.h"
#include "assist/HeadPoseMapper.h"
#include "assist/LookToMaps.h"
#include "assist/MouseDwellMove.h"
#include "layout/PageSession.h"
#include "ui/MagnifierOverlay.h"
#include "ui/PageHostWindow.h"
#include "ui/SplashOverlay.h"

namespace gazer {

void GazeRouter::dispatch(const GazePoint& point)
{
    const bool hostVisible =
        m_pages && m_pages->window() && m_pages->window()->isVisible();

    const bool overSplash = m_splash && m_splash->isActive();
    if (overSplash) {
        m_splash->onGaze(point);
    }

    PageSession::GazeHit hit;
    if (m_pages && hostVisible && !overSplash) {
        hit = m_pages->classifyGaze(point);
    }

    const bool freeAim = m_session && m_session->freesScreenForAim();
    // Combo / mag-pick / look-to pie / splash sit in front of PageHostWindow; the
    // overlay owns the sample even when gaze geometrically hits the dock.
    const bool overCombo = m_comboMouse && m_comboMouse->containsGaze(point);
    const bool overMagPick = m_mouseDwell && m_mouseDwell->containsGaze(point);
    const bool overLts = m_lookToMaps && m_lookToMaps->containsGaze(point);
    const bool overFrontOverlay =
        overSplash || overLts
        || (m_session && m_session->overlayHasGazePriority() && (overCombo || overMagPick));

    bool overBoard = false;
    if (overFrontOverlay) {
        if (m_pages) {
            m_pages->leaveGaze();
        }
    } else if (m_pages && m_pages->hasRoot() && hostVisible) {
        const auto scope = freeAim ? PageSession::GazeScope::MasterAndActivator
                                   : PageSession::GazeScope::All;
        overBoard = m_pages->feedGaze(point, hit, scope);
    }

    const bool dwellOff = m_pages && m_pages->isDwellSuspended();
    const bool pauseBackgroundAssist =
        overBoard || hit.overMaster || freeAim || dwellOff || overSplash;

    if (m_headPose) {
        m_headPose->setPaused(overBoard || hit.overMaster || dwellOff || overSplash
                              || (m_lookToMaps && m_lookToMaps->anyEnabled()));
    }
    GazePoint assist = point;
    if (m_headPose && !m_headPose->isPaused()) {
        const QPointF off = m_headPose->gazeOffset();
        assist.x += off.x();
        assist.y += off.y();
    }

    if (m_gazeReticle) {
        if (overSplash) {
            m_gazeReticle->onGaze(GazePoint{});
        } else {
            m_gazeReticle->onGaze(assist);
        }
    }
    if (m_gazeFollow) {
        const bool pauseFollow =
            dwellOff || overSplash || (m_session && m_session->pausesGazeFollow())
            || (m_lookToMaps && m_lookToMaps->anyEnabled());
        m_gazeFollow->onGaze(assist, /*pauseInput=*/pauseFollow);
    }
    if (m_lookToMaps) {
        m_lookToMaps->onGaze(assist, pauseBackgroundAssist && !overLts);
    }
    if (m_comboMouse) {
        m_comboMouse->onGaze(assist, dwellOff || (pauseBackgroundAssist && !overCombo));
    }
    if (m_mouseDwell) {
        m_mouseDwell->onBackgroundGaze(assist, overBoard || hit.overMaster || overSplash);
        const bool pauseAim =
            dwellOff || overSplash || (hit.overMaster && !overFrontOverlay);
        m_mouseDwell->setPaused(pauseAim);
        if (!pauseAim) {
            m_mouseDwell->onGaze(assist);
        }
    }
    if (m_magnifier && !overSplash) {
        m_magnifier->onGaze(assist);
    }
}

} // namespace gazer
