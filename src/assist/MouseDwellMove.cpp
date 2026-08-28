#include "assist/MouseDwellMove.h"
#include "assist/MouseDwellMove_p.h"

#include "input/MouseInjector.h"
#include "ui/OverlaySurface.h"
#include "ui/PickStyle.h"
#include "utils/Log.h"
#include "utils/ScreenGrab.h"

#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QScreen>
#include <QtMath>

namespace gazer {
const char* MouseDwellMove::purposeName(ArmPurpose purpose)
{
    switch (purpose) {
    case ArmPurpose::LookToScrollPlace:
        return "ltsPlace";
    case ArmPurpose::ComboMousePlace:
        return "comboMousePlace";
    case ArmPurpose::CursorMoveClickLoop:
        return "clickLoop";
    case ArmPurpose::CursorMoveLeftClick:
        return "moveLeftClick";
    case ArmPurpose::CursorMoveRightClick:
        return "moveRightClick";
    case ArmPurpose::CursorMoveMiddleClick:
        return "moveMiddleClick";
    case ArmPurpose::CursorMove:
    default:
        return "move";
    }
}

MouseDwellMove::MouseDwellMove(QObject* parent)
    : QObject(parent)
{
    m_clock.start();
    m_invalidGrace.graceMs = 220;
    m_cursor = std::make_unique<CursorOverlay>();
    m_magOverlay = std::make_unique<MagPickOverlay>();
}

MouseDwellMove::~MouseDwellMove() = default;

bool MouseDwellMove::isMagPointPhase() const
{
    return m_armed && m_phase == Phase::MagPoint;
}

void MouseDwellMove::setPhase(Phase phase)
{
    const bool wasMag = (m_phase == Phase::MagPoint);
    m_phase = phase;
    applyDwellForPhase();
    syncPickOverlayStyle();
    const bool isMag = (m_phase == Phase::MagPoint);
    if (wasMag != isMag) {
        emit magPointPhaseChanged(isMag);
    }
}

bool MouseDwellMove::armWantsForesight() const
{
    if (m_purpose == ArmPurpose::LookToScrollPlace
        || m_purpose == ArmPurpose::ComboMousePlace) {
        return false;
    }
    return m_armZoom.wantsForesight(m_foresight.isEnabled());
}

bool MouseDwellMove::armWantsBonusZoom() const
{
    return m_armZoom.wantsBonus(m_foresightDoubleZoom);
}

bool MouseDwellMove::useMagPickThisArm() const
{
    if (m_purpose == ArmPurpose::LookToScrollPlace
        || m_purpose == ArmPurpose::ComboMousePlace) {
        return false;
    }
    const bool cursorMove = m_purpose == ArmPurpose::CursorMove
                            || m_purpose == ArmPurpose::CursorMoveClickLoop
                            || m_purpose == ArmPurpose::CursorMoveLeftClick
                            || m_purpose == ArmPurpose::CursorMoveRightClick
                            || m_purpose == ArmPurpose::CursorMoveMiddleClick;
    if (!cursorMove) {
        return false;
    }
    return m_armZoom.useMagPick(m_magPickEnabled);
}

void MouseDwellMove::setArmed(bool armed, ArmPurpose purpose)
{
    setArmed(armed, purpose, ArmZoom::settings());
}

void MouseDwellMove::setArmed(bool armed, ArmPurpose purpose, ArmZoom zoom)
{
    const ArmZoom next = armed ? zoom : ArmZoom::settings();
    if (m_armed == armed) {
        if (armed && (m_purpose != purpose || !(m_armZoom == next))) {
            m_purpose = purpose;
            m_armZoom = next;
            m_paused = false;
            resetDwell();
            if (m_magOverlay) {
                m_magOverlay->hide();
            }
            startAimPhase();
            markSelectDeadline();
            GAZER_INFO << "MouseDwellMove purpose →" << purposeName(purpose)
                       << (useMagPickThisArm() ? "zoom" : "direct");
            emit armedChanged(true);
        }
        return;
    }
    m_armZoom = next;
    m_armed = armed;
    m_paused = false;
    m_gateRect = {};
    m_purpose = armed ? purpose : ArmPurpose::CursorMove;
    resetDwell();
    if (!m_armed) {
        hideUi();
        setPhase(Phase::Idle);
        m_selectDeadlineMs = -1;
    } else {
        startAimPhase();
        markSelectDeadline();
    }
    GAZER_INFO << "MouseDwellMove" << (m_armed ? "ARMED" : "off")
               << (useMagPickThisArm() ? "magPick" : "direct") << purposeName(m_purpose);
    emit armedChanged(m_armed);
}

void MouseDwellMove::toggle()
{
    setArmed(!m_armed, ArmPurpose::CursorMove);
}

void MouseDwellMove::gateUntilGazeLeaves(const QRect& screenRect)
{
    if (!m_armed || screenRect.isEmpty()) {
        return;
    }
    m_gateRect = screenRect.adjusted(-16, -16, 16, 16);
    if (m_phase != Phase::MagPoint) {
        hideUi();
        resetDwell();
    }
    m_selectDeadlineMs = -1;
}

void MouseDwellMove::setPaused(bool paused)
{
    if (m_paused == paused) {
        return;
    }
    m_paused = paused;
    if (m_paused) {
        hideUi();
        resetDwell();
        return;
    }
    if (m_armed) {
        resetDwell();
        markSelectDeadline();
    }
}

void MouseDwellMove::setDwellMs(int ms)
{
    m_moveDwellMs = qMax(50, ms);
    applyDwellForPhase();
}

void MouseDwellMove::setMagPickDwellMs(int ms)
{
    m_magPickDwellMs = qMax(50, ms);
    applyDwellForPhase();
}

void MouseDwellMove::setMagPickStyle(int flags)
{
    m_magPickStyle = PickStyle::sanitizeMag(flags);
    syncPickOverlayStyle();
}

void MouseDwellMove::setMousePickStyle(int flags)
{
    m_mousePickStyle = PickStyle::sanitizeMouse(flags);
    syncPickOverlayStyle();
}

void MouseDwellMove::applyDwellForPhase()
{
    const bool choosingRegion = m_phase == Phase::MagRegion
                                || (m_phase == Phase::MagPoint && m_outsideSelectsNewRegion
                                    && useMagPickThisArm()
                                    && (!m_magGazeInside || armWantsBonusZoom()));
    m_dwell.setDwellMs(choosingRegion ? m_magPickDwellMs : m_moveDwellMs);
}

void MouseDwellMove::syncPickOverlayStyle()
{
    if (m_cursor) {
        m_cursor->setStyle(styleForPhase());
    }
    if (m_magOverlay) {
        m_magOverlay->setStyle(PickStyle::sanitizeMouse(m_mousePickStyle));
    }
}

int MouseDwellMove::styleForPhase() const
{
    if (m_phase == Phase::MagRegion) {
        return PickStyle::sanitizeMag(m_magPickStyle);
    }
    return PickStyle::sanitizeMouse(m_mousePickStyle);
}

void MouseDwellMove::setSelectTimeoutMs(int ms)
{
    m_selectTimeoutMs = qMax(0, ms);
    if (m_armed) {
        markSelectDeadline();
    }
}

void MouseDwellMove::markSelectDeadline()
{
    if (!m_armed || m_selectTimeoutMs <= 0) {
        m_selectDeadlineMs = -1;
        return;
    }
    m_selectDeadlineMs = m_clock.elapsed() + m_selectTimeoutMs;
}

bool MouseDwellMove::selectTimedOut(qint64 nowMs) const
{
    return m_selectDeadlineMs >= 0 && nowMs >= m_selectDeadlineMs;
}

void MouseDwellMove::setStableRadiusPx(int px)
{
    m_dwell.setStableRadiusPx(px);
}

void MouseDwellMove::setFreezeRadiusPx(int px)
{
    m_dwell.setFreezeRadiusPx(px);
}

void MouseDwellMove::setCancelRadiusPx(int px)
{
    m_dwell.setCancelRadiusPx(px);
}

void MouseDwellMove::setProgressVisuals(const ProgressVisuals& visuals)
{
    m_progressVisuals = visuals;
    if (m_cursor) {
        m_cursor->setVisuals(visuals);
    }
    if (m_magOverlay) {
        m_magOverlay->setVisuals(visuals);
    }
}

void MouseDwellMove::setMagPickEnabled(bool enabled)
{
    if (m_magPickEnabled == enabled) {
        return;
    }
    m_magPickEnabled = enabled;
    if (!m_armed || m_purpose == ArmPurpose::LookToScrollPlace
        || m_purpose == ArmPurpose::ComboMousePlace || m_phase == Phase::MagPoint) {
        return;
    }
    if (m_magOverlay) {
        m_magOverlay->hide();
    }
    resetDwell();
    setPhase(useMagPickThisArm() ? Phase::MagRegion : Phase::Direct);
}

void MouseDwellMove::setPickZoom(double z)
{
    m_pickZoom = qBound(1.25, z, 8.0);
}

void MouseDwellMove::setPickWindowPx(int px)
{
    m_pickWindowPx = qBound(200, px, 1600);
}

void MouseDwellMove::setPickWindowRound(bool on)
{
    m_pickWindowRound = on;
    if (m_magOverlay) {
        m_magOverlay->setRound(on);
    }
}

void MouseDwellMove::setMagPickCenterOnDwell(bool on)
{
    m_magPickCenterOnDwell = on;
}

void MouseDwellMove::setMagPickFullScreen(bool on)
{
    m_magPickFullScreen = on;
}

void MouseDwellMove::setForesightEnabled(bool enabled)
{
    m_foresight.setEnabled(enabled);
}

void MouseDwellMove::setForesightDwellMs(int ms)
{
    m_foresight.setDwellMs(ms);
}

void MouseDwellMove::setForesightDoubleZoom(bool on)
{
    m_foresightDoubleZoom = on;
}

void MouseDwellMove::resetDwell()
{
    m_dwell.reset();
    m_lastSampleMs = -1;
    m_invalidGrace.reset();
    emit progressChanged(0.0);
}

void MouseDwellMove::hideUi()
{
    if (m_cursor) {
        m_cursor->hide();
    }
    if (m_magOverlay) {
        m_magOverlay->hide();
    }
}

void MouseDwellMove::onBackgroundGaze(const GazePoint& point, bool overUi)
{
    if (m_armed) {
        return;
    }
    m_foresight.sample(point, overUi, m_clock.elapsed());
}

void MouseDwellMove::placeCursor(const QPoint& target)
{
    QString err;
    if (MouseInjector::moveTo(target.x(), target.y(), &err)) {
        GAZER_INFO << "MouseDwellMove →" << target;
        emit movedTo(target);
        completeMoveCycle(target);
    } else {
        GAZER_WARN << "MouseDwellMove failed:" << err;
        setArmed(false);
    }
}

void MouseDwellMove::onGaze(const GazePoint& point)
{
    if (!m_armed || m_paused) {
        return;
    }

    if (!m_gateRect.isEmpty()) {
        if (!point.valid) {
            return;
        }
        if (m_gateRect.contains(QPoint(qRound(point.x), qRound(point.y)))) {
            if (m_phase != Phase::MagPoint) {
                hideUi();
            }
            return;
        }
        m_gateRect = {};
        resetDwell();
        markSelectDeadline();
    }

    const qint64 now = m_clock.elapsed();
    if (selectTimedOut(now)) {
        GAZER_INFO << "MouseDwellMove select timeout — disarming";
        setArmed(false);
        return;
    }

    if (!point.valid) {
        if (m_invalidGrace.onInvalid() == InvalidGazeGrace::Result::Holding) {
            return;
        }
        if (m_phase == Phase::MagPoint) {
            resetDwell();
            if (m_magOverlay) {
                m_magOverlay->setProgress(0.0);
            }
        } else {
            hideUi();
            resetDwell();
        }
        return;
    }
    m_invalidGrace.onValid();

    const QPointF g(point.x, point.y);
    const double dtSec =
        m_lastSampleMs < 0 ? 0.016
                           : qBound(0.004, (now - m_lastSampleMs) / 1000.0, 0.08);
    m_lastSampleMs = now;

    if (m_phase == Phase::MagPoint) {
        onGazeInZoom(g, dtSec);
    } else {
        onGazeAim(g, dtSec);
    }
}

void MouseDwellMove::onGazeAim(const QPointF& g, double dtSec)
{
    const bool done = m_dwell.sample(g, dtSec);
    emit progressChanged(m_dwell.progress());
    if (m_cursor) {
        m_cursor->setProgress(m_dwell.progress());
        m_cursor->placeCenter(QPoint(qRound(m_dwell.smoothPos().x()), qRound(m_dwell.smoothPos().y())));
    }
    if (!done) {
        return;
    }

    const QPoint target(qRound(m_dwell.smoothPos().x()), qRound(m_dwell.smoothPos().y()));
    if (m_phase == Phase::MagRegion) {
        if (!beginMagPick(makePreClickSpec(target), /*outsideSelectsNewRegion=*/false)) {
            GAZER_WARN << "MouseDwellMove pre-click zoom failed — placing directly";
            placeCursor(target);
        }
        return;
    }
    placeCursor(target);
}

void MouseDwellMove::completeMoveCycle(const QPoint& target)
{
    Q_UNUSED(target);
    const bool loop = m_purpose == ArmPurpose::CursorMoveClickLoop;
    const bool leftOnce = m_purpose == ArmPurpose::CursorMoveLeftClick;
    const bool rightOnce = m_purpose == ArmPurpose::CursorMoveRightClick;
    const bool middleOnce = m_purpose == ArmPurpose::CursorMoveMiddleClick;
    if (loop || leftOnce || rightOnce || middleOnce) {
        QString err;
        QString button = QStringLiteral("left");
        if (rightOnce) {
            button = QStringLiteral("right");
        } else if (middleOnce) {
            button = QStringLiteral("middle");
        }
        if (!MouseInjector::click(button, &err)) {
            GAZER_WARN << "MouseDwellMove click failed:" << err;
        }
        if (!loop) {
            setArmed(false);
            return;
        }
        resetDwell();
        startAimPhase();
        markSelectDeadline();
        return;
    }
    setArmed(false);
}

} // namespace gazer
