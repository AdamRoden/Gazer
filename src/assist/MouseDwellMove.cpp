#include "assist/MouseDwellMove.h"

#include "input/MouseInjector.h"
#include "ui/OverlaySurface.h"
#include "utils/Log.h"

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
        resize(72, 72);
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

    void placeTip(const QPoint& tip)
    {
        move(tip.x() - 8, tip.y() - 6);
        showOverlay();
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        QPainterPath path;
        path.moveTo(6, 4);
        path.lineTo(6, 48);
        path.lineTo(18, 36);
        path.lineTo(26, 54);
        path.lineTo(34, 50);
        path.lineTo(26, 32);
        path.lineTo(42, 32);
        path.closeSubpath();
        p.setPen(QPen(Qt::black, 1.5));
        p.setBrush(QColor(255, 255, 255, 230));
        p.drawPath(path);
        if (m_progress > 0.01 && m_visuals.radial) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(m_visuals.progressColor, 3.0));
            p.drawArc(QRect(2, 2, 28, 28), 90 * 16, int(-360 * 16 * m_progress));
        }
    }

private:
    double m_progress = 0.0;
    ProgressVisuals m_visuals;
};

class MouseDwellMove::MagPickOverlay final : public OverlaySurface {
public:
    MagPickOverlay() { hide(); }

    void showCapture(const QPixmap& pm, const QRect& screenGeom)
    {
        m_pm = pm;
        m_hasPick = false;
        m_pick = {};
        m_progress = 0.0;
        setGeometry(screenGeom);
        showOverlay();
        update();
    }

    void setProgress(double p)
    {
        m_progress = qBound(0.0, p, 1.0);
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
        p.fillRect(rect(), QColor(0, 0, 0, 180));
        if (!m_pm.isNull()) {
            p.drawPixmap(rect(), m_pm);
        }
        p.setPen(QPen(QColor(0, 220, 255), 3.0));
        p.setBrush(Qt::NoBrush);
        p.drawRect(rect().adjusted(1, 1, -1, -1));
        if (m_hasPick) {
            p.setPen(QPen(QColor(255, 200, 40), 2.0));
            p.drawLine(m_pick.x() - 14, m_pick.y(), m_pick.x() + 14, m_pick.y());
            p.drawLine(m_pick.x(), m_pick.y() - 14, m_pick.x(), m_pick.y() + 14);
            if (m_progress > 0.01) {
                p.setPen(QPen(QColor(255, 200, 40), 3.0));
                p.drawArc(QRect(m_pick.x() - 18, m_pick.y() - 18, 36, 36), 90 * 16,
                          int(-360 * 16 * m_progress));
            }
        }
        p.setPen(QColor(240, 248, 255));
        p.setFont(QFont(QStringLiteral("Segoe UI"), 11, QFont::DemiBold));
        p.drawText(rect().adjusted(12, 10, -12, -10), Qt::AlignTop | Qt::AlignHCenter,
                   QStringLiteral("Dwell to pick point (static zoom)"));
    }

private:
    QPixmap m_pm;
    QPoint m_pick;
    bool m_hasPick = false;
    double m_progress = 0.0;
};

MouseDwellMove::MouseDwellMove(QObject* parent)
    : QObject(parent)
{
    m_clock.start();
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
    const bool isMag = (m_phase == Phase::MagPoint);
    if (wasMag != isMag) {
        emit magPointPhaseChanged(isMag);
    }
}

bool MouseDwellMove::useMagPickThisArm() const
{
    // Look↕Scroll placement is always direct — mag-pick is for Move-to / click-loop.
    return m_magPickEnabled
           && (m_purpose == ArmPurpose::CursorMove
               || m_purpose == ArmPurpose::CursorMoveClickLoop);
}

void MouseDwellMove::setArmed(bool armed, ArmPurpose purpose)
{
    if (m_armed == armed) {
        // Re-arm with a different purpose while already armed (e.g. LTS place).
        if (armed && m_purpose != purpose) {
            m_purpose = purpose;
            resetDwell();
            if (m_magOverlay) {
                m_magOverlay->hide();
            }
            setPhase(useMagPickThisArm() ? Phase::MagRegion : Phase::Direct);
            markSelectDeadline();
            const char* purposeName = "move";
            if (purpose == ArmPurpose::LookToScrollPlace) {
                purposeName = "ltsPlace";
            } else if (purpose == ArmPurpose::CursorMoveClickLoop) {
                purposeName = "clickLoop";
            }
            GAZER_INFO << "MouseDwellMove purpose →" << purposeName;
            emit armedChanged(true);
        }
        return;
    }
    m_armed = armed;
    m_purpose = armed ? purpose : ArmPurpose::CursorMove;
    resetDwell();
    if (!m_armed) {
        hideUi();
        setPhase(Phase::Idle);
        m_selectDeadlineMs = -1;
    } else {
        setPhase(useMagPickThisArm() ? Phase::MagRegion : Phase::Direct);
        markSelectDeadline();
    }
    const char* purposeName = "move";
    if (m_purpose == ArmPurpose::LookToScrollPlace) {
        purposeName = "ltsPlace";
    } else if (m_purpose == ArmPurpose::CursorMoveClickLoop) {
        purposeName = "clickLoop";
    }
    GAZER_INFO << "MouseDwellMove" << (m_armed ? "ARMED" : "off")
               << (useMagPickThisArm() ? "magPick" : "direct") << purposeName;
    emit armedChanged(m_armed);
}

void MouseDwellMove::toggle()
{
    if (m_armed) {
        setArmed(false);
    } else {
        setArmed(true, ArmPurpose::CursorMove);
    }
}

void MouseDwellMove::setDwellMs(int ms)
{
    m_dwell.setDwellMs(ms);
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
}

void MouseDwellMove::setMagPickEnabled(bool enabled)
{
    if (m_magPickEnabled == enabled) {
        return;
    }
    m_magPickEnabled = enabled;
    if (!m_armed) {
        return;
    }
    // LTS place ignores mag-pick setting.
    if (m_purpose == ArmPurpose::LookToScrollPlace) {
        return;
    }
    if (m_magOverlay) {
        m_magOverlay->hide();
    }
    resetDwell();
    setPhase(useMagPickThisArm() ? Phase::MagRegion : Phase::Direct);
}

void MouseDwellMove::setMagPickZoom(double z)
{
    m_magZoom = qBound(1.5, z, 6.0);
}

void MouseDwellMove::setMagPickSourcePx(int px)
{
    m_magSourcePx = qBound(80, px, 600);
}

void MouseDwellMove::setMagPickCenterOnDwell(bool on)
{
    m_magPickCenterOnDwell = on;
}

void MouseDwellMove::resetDwell()
{
    m_dwell.reset();
    m_lastSampleMs = -1;
    m_invalidSinceMs = -1;
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

void MouseDwellMove::beginMagPick(const QPoint& center)
{
    QScreen* screen = QGuiApplication::screenAt(center);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) {
        setArmed(false);
        return;
    }

    const int half = m_magSourcePx / 2;
    QRect src(center.x() - half, center.y() - half, m_magSourcePx, m_magSourcePx);
    src = src.intersected(screen->geometry());
    if (src.width() < 40 || src.height() < 40) {
        setArmed(false);
        return;
    }

    const QPixmap grab = screen->grabWindow(0, src.x() - screen->geometry().x(),
                                            src.y() - screen->geometry().y(), src.width(),
                                            src.height());
    m_magSourceRect = src;
    m_magPixmap = grab.scaled(int(src.width() * m_magZoom), int(src.height() * m_magZoom),
                              Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    const QRect avail = screen->availableGeometry();
    const int dw = m_magPixmap.width();
    const int dh = m_magPixmap.height();
    QPoint topLeft;
    if (m_magPickCenterOnDwell) {
        // Center static magnifier on the first dwell point (where the region was chosen).
        topLeft = QPoint(center.x() - dw / 2, center.y() - dh / 2);
        topLeft.setX(qBound(avail.left(), topLeft.x(), avail.right() - dw + 1));
        topLeft.setY(qBound(avail.top(), topLeft.y(), avail.bottom() - dh + 1));
    } else {
        topLeft = QPoint(avail.center().x() - dw / 2, avail.center().y() - dh / 2);
    }
    m_magDisplayRect = QRect(topLeft, QSize(dw, dh));

    if (m_cursor) {
        m_cursor->hide();
    }
    m_magOverlay->showCapture(m_magPixmap, m_magDisplayRect);
    setPhase(Phase::MagPoint);
    resetDwell();
    // Region chosen — give a fresh window for the refined point dwell.
    markSelectDeadline();
    GAZER_INFO << "MouseDwellMove mag-pick region" << src;
}

void MouseDwellMove::finishMagPoint(const QPointF& gaze)
{
    if (m_magSourceRect.isEmpty() || m_magDisplayRect.width() < 1
        || m_magDisplayRect.height() < 1) {
        GAZER_WARN << "MouseDwellMove mag pick: empty geometry";
        setArmed(false);
        return;
    }
    // Clamp into display (EMA smooth can lag a few px outside).
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
    const QPoint target(sx, sy);

    QString err;
    if (MouseInjector::moveTo(target.x(), target.y(), &err)) {
        GAZER_INFO << "MouseDwellMove mag →" << target;
        emit movedTo(target);
        completeMoveCycle(target);
    } else {
        GAZER_WARN << "MouseDwellMove mag failed:" << err;
        setArmed(false);
    }
}

void MouseDwellMove::onGaze(const GazePoint& point)
{
    if (!m_armed) {
        return;
    }

    const qint64 now = m_clock.elapsed();

    if (selectTimedOut(now)) {
        GAZER_INFO << "MouseDwellMove select timeout — disarming";
        setArmed(false);
        return;
    }

    // Brief invalid samples (tracker dropouts) must not wipe dwell; layout dwell uses
    // the same pattern. Mag-point keeps the static capture visible either way.
    if (!point.valid) {
        if (m_invalidSinceMs < 0) {
            m_invalidSinceMs = now;
        }
        if ((now - m_invalidSinceMs) < m_invalidGraceMs) {
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
    m_invalidSinceMs = -1;

    const QPointF g(point.x, point.y);
    const double dtSec =
        m_lastSampleMs < 0 ? 0.016
                           : qBound(0.004, (now - m_lastSampleMs) / 1000.0, 0.08);
    m_lastSampleMs = now;

    if (m_phase == Phase::MagPoint) {
        // Ensure overlay stays shown after invalid-sample dwell reset.
        if (m_magOverlay && !m_magOverlay->isVisible() && !m_magPixmap.isNull()) {
            m_magOverlay->showCapture(m_magPixmap, m_magDisplayRect);
        }
        if (!m_magDisplayRect.contains(g.toPoint())) {
            resetDwell();
            if (m_magOverlay) {
                m_magOverlay->setProgress(0.0);
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
        if (done) {
            // Prefer last raw in-rect sample; clamp smooth as fallback.
            QPointF fire = m_lastMagGaze;
            if (!m_magDisplayRect.contains(fire.toPoint())) {
                fire = m_dwell.smoothPos();
            }
            finishMagPoint(fire);
        }
        return;
    }

    const bool done = m_dwell.sample(g, dtSec);
    emit progressChanged(m_dwell.progress());
    if (m_cursor) {
        m_cursor->setProgress(m_dwell.progress());
        m_cursor->placeTip(QPoint(qRound(m_dwell.smoothPos().x()), qRound(m_dwell.smoothPos().y())));
    }
    if (!done) {
        return;
    }

    if (m_phase == Phase::MagRegion) {
        beginMagPick(QPoint(qRound(m_dwell.smoothPos().x()), qRound(m_dwell.smoothPos().y())));
        return;
    }

    const QPoint target(qRound(m_dwell.smoothPos().x()), qRound(m_dwell.smoothPos().y()));
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

void MouseDwellMove::completeMoveCycle(const QPoint& target)
{
    Q_UNUSED(target);
    if (m_purpose == ArmPurpose::CursorMoveClickLoop) {
        QString err;
        if (!MouseInjector::click(QStringLiteral("left"), &err)) {
            GAZER_WARN << "MouseDwellMove click-loop click failed:" << err;
        }
        // Stay armed: re-run same move/mag pipeline for the next cycle.
        if (m_magOverlay) {
            m_magOverlay->hide();
        }
        if (m_cursor) {
            m_cursor->hide();
        }
        resetDwell();
        setPhase(useMagPickThisArm() ? Phase::MagRegion : Phase::Direct);
        // Fresh window to pick the next target; cancel if none selected in time.
        markSelectDeadline();
        return;
    }
    setArmed(false);
}

} // namespace gazer
