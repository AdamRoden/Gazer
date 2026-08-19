#include "assist/MouseDwellMove.h"

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

class MouseDwellMove::CursorOverlay final : public OverlaySurface {
public:
    CursorOverlay()
    {
        resize(200, 200);
        hide();
    }

    void setProgress(double p)
    {
        m_progress = qBound(0.0, p, 1.0);
        update();
    }

    void setVisuals(const ProgressVisuals& v)
    {
        m_visuals = v;
        update();
    }

    void setStyle(int flags)
    {
        m_style = flags;
        update();
    }

    void placeCenter(const QPoint& c)
    {
        move(c.x() - width() / 2, c.y() - height() / 2);
        showOverlay();
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        PickStyle::paint(p, QRectF(rect()).center(), m_style, m_progress, m_visuals);
    }

private:
    double m_progress = 0.0;
    int m_style = PickStyle::kDefaultMousePick;
    ProgressVisuals m_visuals;
};

class MouseDwellMove::MagPickOverlay final : public OverlaySurface {
public:
    MagPickOverlay() { hide(); }

    void showCapture(const QPixmap& pm, const QRect& destGlobal, const QString& hint, bool round)
    {
        m_pm = pm;
        m_hasPick = false;
        m_pick = {};
        m_progress = 0.0;
        m_hint = hint;
        m_round = round;
        setGeometry(destGlobal);
        showOverlay();
        update();
    }

    void setRound(bool on)
    {
        if (m_round == on) {
            return;
        }
        m_round = on;
        update();
    }

    void clearPick()
    {
        if (!m_hasPick) {
            return;
        }
        m_hasPick = false;
        m_pick = {};
        update();
    }

    void setProgress(double p)
    {
        m_progress = qBound(0.0, p, 1.0);
        update();
    }

    void setVisuals(const ProgressVisuals& v)
    {
        m_visuals = v;
        update();
    }

    void setStyle(int flags)
    {
        m_style = flags;
        update();
    }

    void setPickLocal(const QPoint& local)
    {
        m_pick = local;
        m_hasPick = true;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.fillRect(rect(), Qt::transparent);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);

        QPainterPath clip;
        if (m_round) {
            clip.addEllipse(QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5));
        } else {
            clip.addRect(QRectF(rect()));
        }
        p.save();
        p.setClipPath(clip);
        p.fillRect(rect(), QColor(0, 0, 0, 180));
        if (!m_pm.isNull()) {
            p.drawPixmap(rect(), m_pm);
        }
        p.restore();

        const QColor ring = m_visuals.progressColor;
        p.setPen(QPen(m_visuals.borderColor.isValid() ? m_visuals.borderColor : ring, 3.0));
        p.setBrush(Qt::NoBrush);
        const QRectF frame = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);
        if (m_round) {
            p.drawEllipse(frame);
        } else {
            p.drawRect(frame);
        }
        if (m_hasPick) {
            PickStyle::paint(p, QPointF(m_pick), m_style, m_progress, m_visuals);
        }
        p.setPen(Qt::white);
        p.setFont(QFont(QStringLiteral("Segoe UI"), 11, QFont::DemiBold));
        const QString hint = m_hint.isEmpty()
                                 ? QStringLiteral("Dwell to pick point (static zoom)")
                                 : m_hint;
        p.drawText(rect().adjusted(12, 10, -12, -10), Qt::AlignTop | Qt::AlignHCenter, hint);
    }

private:
    QPixmap m_pm;
    QPoint m_pick;
    bool m_hasPick = false;
    double m_progress = 0.0;
    int m_style = PickStyle::kDefaultMousePick;
    ProgressVisuals m_visuals;
    QString m_hint;
    bool m_round = false;
};

const char* MouseDwellMove::purposeName(ArmPurpose purpose)
{
    switch (purpose) {
    case ArmPurpose::LookToScrollPlace:
        return "ltsPlace";
    case ArmPurpose::CursorMoveClickLoop:
        return "clickLoop";
    case ArmPurpose::CursorMoveLeftClick:
        return "moveLeftClick";
    case ArmPurpose::CursorMoveRightClick:
        return "moveRightClick";
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

bool MouseDwellMove::useMagPickThisArm() const
{
    return m_magPickEnabled
           && (m_purpose == ArmPurpose::CursorMove
               || m_purpose == ArmPurpose::CursorMoveClickLoop
               || m_purpose == ArmPurpose::CursorMoveLeftClick
               || m_purpose == ArmPurpose::CursorMoveRightClick);
}

void MouseDwellMove::setArmed(bool armed, ArmPurpose purpose)
{
    if (m_armed == armed) {
        if (armed && m_purpose != purpose) {
            m_purpose = purpose;
            m_paused = false;
            resetDwell();
            if (m_magOverlay) {
                m_magOverlay->hide();
            }
            startAimPhase();
            markSelectDeadline();
            GAZER_INFO << "MouseDwellMove purpose →" << purposeName(purpose);
            emit armedChanged(true);
        }
        return;
    }
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
                                    && m_magPickEnabled
                                    && (!m_magGazeInside || m_foresightDoubleZoom));
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
    if (!m_armed || m_purpose == ArmPurpose::LookToScrollPlace || m_phase == Phase::MagPoint) {
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
    if (!m_foresight.isEnabled() || m_armed) {
        return;
    }
    m_foresight.sample(point, overUi, m_clock.elapsed());
}

int MouseDwellMove::destSideFor(QScreen* screen) const
{
    const QRect g = screen->geometry();
    const int maxSide = qMax(80, qMin(g.width(), g.height()));
    if (m_magPickFullScreen) {
        return maxSide;
    }
    return qMin(qMax(80, m_pickWindowPx), maxSide);
}

MagPresentation MouseDwellMove::makePreClickSpec(const QPoint& center) const
{
    MagPresentation spec;
    spec.srcCenter = center;
    spec.zoom = m_pickZoom;
    QScreen* screen = QGuiApplication::screenAt(center);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    spec.destSide = screen ? destSideFor(screen) : 80;
    spec.destCenter = (m_magPickCenterOnDwell || !screen) ? center : screen->geometry().center();
    return spec;
}

MagPresentation MouseDwellMove::makeForesightSpec(const QPoint& srcCenter, const QPoint& destCenter,
                                                 int destSide) const
{
    MagPresentation spec;
    spec.srcCenter = srcCenter;
    spec.destCenter = destCenter;
    spec.zoom = m_pickZoom;
    spec.destSide = destSide;
    return spec;
}

void MouseDwellMove::startAimPhase()
{
    m_outsideSelectsNewRegion = false;
    m_mag = {};
    if (m_purpose != ArmPurpose::LookToScrollPlace) {
        if (const auto fs = m_foresight.peek(m_clock.elapsed())) {
            QScreen* screen = QGuiApplication::screenAt(*fs);
            if (!screen) {
                screen = QGuiApplication::primaryScreen();
            }
            const int side = screen ? destSideFor(screen) : 0;
            if (side >= 80
                && beginMagPick(makeForesightSpec(*fs, *fs, side), /*outsideSelectsNewRegion=*/true)) {
                m_foresight.clear();
                return;
            }
            GAZER_WARN << "MouseDwellMove foresight zoom failed — falling back";
            m_foresight.clear();
        }
    }
    setPhase(useMagPickThisArm() ? Phase::MagRegion : Phase::Direct);
}

bool MouseDwellMove::beginMagPick(const MagPresentation& spec, bool outsideSelectsNewRegion)
{
    QScreen* screen = QGuiApplication::screenAt(spec.destCenter);
    if (!screen) {
        screen = QGuiApplication::screenAt(spec.srcCenter);
    }
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) {
        GAZER_WARN << "MouseDwellMove mag-pick: no screen";
        return false;
    }

    QRect src;
    QRect dest;
    if (!layoutMagWindow(screen, spec, &src, &dest)) {
        GAZER_WARN << "MouseDwellMove mag-pick: layout failed at" << spec.srcCenter;
        return false;
    }

    QPixmap grab = grabScreenRect(screen, src, QColor(12, 14, 18));
    if (grab.isNull() || grab.width() < 2 || grab.height() < 2) {
        const QRect local = src.translated(-screen->geometry().topLeft());
        grab = screen->grabWindow(0, local.x(), local.y(), local.width(), local.height());
    }
    if (grab.isNull() || grab.width() < 2 || grab.height() < 2) {
        GAZER_WARN << "MouseDwellMove mag-pick: grab failed" << src;
        return false;
    }

    m_mag = spec;
    m_mag.destSide = dest.width();
    m_outsideSelectsNewRegion = outsideSelectsNewRegion;
    m_magSourceRect = src;
    m_magDisplayRect = dest;
    m_magPixmap = grab.scaled(dest.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    m_magGazeInside = true;

    QString hint = QStringLiteral("Dwell to pick point (static zoom)");
    if (outsideSelectsNewRegion) {
        if (m_magPickEnabled && m_foresightDoubleZoom) {
            hint = QStringLiteral("Foresight — dwell inside to zoom again, outside for a new region");
        } else if (m_magPickEnabled) {
            hint = QStringLiteral("Foresight — dwell inside to place, outside for pre-click zoom");
        } else {
            hint = QStringLiteral("Foresight — dwell inside to place");
        }
    } else if (spec.zoom == m_pickZoom) {
        hint = QStringLiteral("Foresight — dwell to pick point");
    }

    if (m_cursor) {
        m_cursor->hide();
    }
    m_magOverlay->showCapture(m_magPixmap, dest, hint, m_pickWindowRound);
    setPhase(Phase::MagPoint);
    resetDwell();
    markSelectDeadline();
    GAZER_INFO << "MouseDwellMove mag-pick region" << src << "dest" << dest;
    return true;
}

bool MouseDwellMove::gazeInZoomWindow(const QPointF& gaze) const
{
    if (m_magDisplayRect.isEmpty()) {
        return false;
    }
    if (!m_pickWindowRound) {
        return m_magDisplayRect.contains(gaze.toPoint());
    }
    const QRectF r = m_magDisplayRect;
    const double rx = r.width() * 0.5;
    const double ry = r.height() * 0.5;
    if (rx <= 0.0 || ry <= 0.0) {
        return false;
    }
    const double nx = (gaze.x() - r.center().x()) / rx;
    const double ny = (gaze.y() - r.center().y()) / ry;
    return (nx * nx + ny * ny) <= 1.0;
}

QPoint MouseDwellMove::mapDisplayToSource(const QPointF& gaze) const
{
    if (m_magSourceRect.isEmpty() || m_magDisplayRect.width() < 1
        || m_magDisplayRect.height() < 1) {
        return gaze.toPoint();
    }
    const double gx = qBound(double(m_magDisplayRect.left()), gaze.x(),
                             double(m_magDisplayRect.right()));
    const double gy = qBound(double(m_magDisplayRect.top()), gaze.y(),
                             double(m_magDisplayRect.bottom()));
    const double lx =
        (gx - m_magDisplayRect.left()) * m_magSourceRect.width() / double(m_magDisplayRect.width());
    const double ly = (gy - m_magDisplayRect.top()) * m_magSourceRect.height()
                      / double(m_magDisplayRect.height());
    const int sx = qBound(m_magSourceRect.left(), m_magSourceRect.left() + qRound(lx),
                          m_magSourceRect.right());
    const int sy = qBound(m_magSourceRect.top(), m_magSourceRect.top() + qRound(ly),
                          m_magSourceRect.bottom());
    return {sx, sy};
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

void MouseDwellMove::finishMagPoint(const QPointF& gaze)
{
    if (m_magSourceRect.isEmpty() || m_magDisplayRect.width() < 1
        || m_magDisplayRect.height() < 1) {
        GAZER_WARN << "MouseDwellMove mag pick: empty geometry";
        setArmed(false);
        return;
    }
    placeCursor(mapDisplayToSource(gaze));
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

void MouseDwellMove::onGazeInZoom(const QPointF& g, double dtSec)
{
    if (m_magOverlay && !m_magOverlay->isVisible() && !m_magPixmap.isNull()) {
        m_magOverlay->showCapture(m_magPixmap, m_magDisplayRect, {}, m_pickWindowRound);
    }

    const bool inside = gazeInZoomWindow(g);
    if (inside != m_magGazeInside) {
        m_magGazeInside = inside;
        resetDwell();
        applyDwellForPhase();
        if (m_magOverlay) {
            m_magOverlay->setProgress(0.0);
            if (!inside) {
                m_magOverlay->clearPick();
            }
        }
        if (inside && m_cursor) {
            m_cursor->hide();
        }
    }

    if (!inside) {
        if (!m_outsideSelectsNewRegion) {
            resetDwell();
            if (m_magOverlay) {
                m_magOverlay->setProgress(0.0);
                m_magOverlay->clearPick();
            }
            return;
        }
        const bool done = m_dwell.sample(g, dtSec);
        emit progressChanged(m_dwell.progress());
        if (m_cursor) {
            m_cursor->setProgress(m_dwell.progress());
            m_cursor->placeCenter(
                QPoint(qRound(m_dwell.smoothPos().x()), qRound(m_dwell.smoothPos().y())));
        }
        if (!done) {
            return;
        }
        const QPoint outside(qRound(m_dwell.smoothPos().x()), qRound(m_dwell.smoothPos().y()));
        if (m_magPickEnabled) {
            if (!beginMagPick(makePreClickSpec(outside), /*outsideSelectsNewRegion=*/false)) {
                GAZER_WARN << "MouseDwellMove outside pre-click zoom failed";
            }
        } else {
            placeCursor(outside);
        }
        return;
    }

    m_lastMagGaze = g;
    const bool done = m_dwell.sample(g, dtSec);
    const QPoint local(qRound(g.x() - m_magDisplayRect.left()),
                       qRound(g.y() - m_magDisplayRect.top()));
    m_magOverlay->setPickLocal(local);
    m_magOverlay->setProgress(m_dwell.progress());
    emit progressChanged(m_dwell.progress());
    if (!done) {
        return;
    }

    QPointF fire = m_lastMagGaze;
    if (!m_magDisplayRect.contains(fire.toPoint())) {
        fire = m_dwell.smoothPos();
    }
    if (m_outsideSelectsNewRegion && m_magPickEnabled && m_foresightDoubleZoom) {
        const int side = m_mag.destSide > 0 ? m_mag.destSide
                                            : destSideFor(QGuiApplication::primaryScreen());
        if (!beginMagPick(makeForesightSpec(mapDisplayToSource(fire),
                                            QPoint(qRound(fire.x()), qRound(fire.y())), side),
                          /*outsideSelectsNewRegion=*/false)) {
            finishMagPoint(fire);
        }
    } else {
        finishMagPoint(fire);
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
    if (loop || leftOnce || rightOnce) {
        QString err;
        const QString button = rightOnce ? QStringLiteral("right") : QStringLiteral("left");
        if (!MouseInjector::click(button, &err)) {
            GAZER_WARN << "MouseDwellMove click failed:" << err;
        }
        if (!loop) {
            setArmed(false);
            return;
        }
        if (m_magOverlay) {
            m_magOverlay->hide();
        }
        if (m_cursor) {
            m_cursor->hide();
        }
        resetDwell();
        startAimPhase();
        markSelectDeadline();
        return;
    }
    setArmed(false);
}

} // namespace gazer
