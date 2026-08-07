#include "assist/LookToScroll.h"

#include "input/MouseInjector.h"
#include "ui/OverlaySurface.h"
#include "utils/Log.h"

#include <QCursor>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QScreen>
#include <QtMath>

namespace gazer {

namespace {
constexpr double kWheelDelta = 120.0;
constexpr double kMinEmitDelta = 8.0;
} // namespace

class LookToScroll::RingOverlay final : public OverlaySurface {
public:
    RingOverlay()
    {
        resize(m_box, m_box);
        hide();
    }

    void setState(int deadzonePx, int falloffPx, double activity, double centerProg, bool suspended,
                  double dirX, double dirY)
    {
        m_deadzone = deadzonePx;
        m_falloff = qMax(40, falloffPx);
        m_activity = qBound(0.0, activity, 1.0);
        m_centerProg = qBound(0.0, centerProg, 1.0);
        m_suspended = suspended;
        m_dirX = dirX;
        m_dirY = dirY;

        const int maxOuter = m_deadzone + qMax(48, m_falloff / 3);
        const int side = suspended ? (qMax(14, m_deadzone / 3) * 2 + 36) : (maxOuter * 2 + 24);
        if (side != m_box) {
            m_box = side;
            resize(m_box, m_box);
        }
        update();
    }

    void placeCenter(const QPoint& screenCenter)
    {
        move(screenCenter.x() - width() / 2, screenCenter.y() - height() / 2);
        showOverlay();
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QPointF c(rect().center());
        const int r = m_deadzone;
        const QColor cyan(0, 220, 255);
        const QColor amber(255, 160, 40);
        const QColor accent = m_suspended ? amber : cyan;

        // Active only: soft translucent deadzone disk — no border.
        if (!m_suspended && r > 0) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(cyan.red(), cyan.green(), cyan.blue(), 16));
            p.drawEllipse(c, double(r), double(r));
        }

        // Speed indicator: annular sector in the gaze/scroll direction only,
        // growing from the deadzone edge up to a circular falloff bound.
        if (!m_suspended && m_activity > 0.02) {
            const double len = qSqrt(m_dirX * m_dirX + m_dirY * m_dirY);
            if (len > 0.05) {
                const double nx = m_dirX / len;
                const double ny = m_dirY / len;
                const int maxGrow = qMax(48, m_falloff / 3);
                const int rOuter = r + int(maxGrow * m_activity);
                const int rInner = r;

                // Qt angles: 0° = east, positive = counter-clockwise; screen Y is down.
                const double midDeg = qRadiansToDegrees(qAtan2(-ny, nx));
                // Wider wedge as speed rises (easier to see direction).
                const double halfSpread = 22.0 + 28.0 * m_activity;
                const double startDeg = midDeg - halfSpread;
                const double spanDeg = halfSpread * 2.0;

                QPainterPath pie;
                pie.moveTo(c);
                pie.arcTo(QRectF(c.x() - rOuter, c.y() - rOuter, rOuter * 2.0, rOuter * 2.0),
                          startDeg, spanDeg);
                pie.closeSubpath();

                QPainterPath hole;
                hole.addEllipse(c, double(rInner), double(rInner));
                const QPainterPath wedge = pie.subtracted(hole);

                const int a = int(28 + 90 * m_activity);
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(cyan.red(), cyan.green(), cyan.blue(), a));
                p.drawPath(wedge);
            }
        }

        // Center activator (suspend / resume) — always visible while LTS is shown.
        const int cr = qMax(14, r / 3);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, m_suspended ? 70 : 40));
        p.drawEllipse(c, double(cr), double(cr));
        p.setBrush(QColor(accent.red(), accent.green(), accent.blue(), m_suspended ? 55 : 35));
        p.drawEllipse(c, double(cr), double(cr));

        // Soft ring on activator only (not the deadzone).
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(accent.red(), accent.green(), accent.blue(), 140), 2.0));
        p.drawEllipse(c, double(cr), double(cr));

        if (m_centerProg > 0.01) {
            p.setPen(QPen(accent, 3.5));
            p.drawArc(QRectF(c.x() - cr, c.y() - cr, cr * 2.0, cr * 2.0), 90 * 16,
                      int(-360 * 16 * m_centerProg));
        }
    }

private:
    int m_deadzone = 110;
    int m_falloff = 360;
    int m_box = 260;
    double m_activity = 0.0;
    double m_centerProg = 0.0;
    double m_dirX = 0.0;
    double m_dirY = 0.0;
    bool m_suspended = false;
};

LookToScroll::LookToScroll(QObject* parent)
    : QObject(parent)
{
    m_clock.start();
    m_overlay = std::make_unique<RingOverlay>();
}

LookToScroll::~LookToScroll()
{
    hideOverlay();
}

double LookToScroll::easeNearDeadzone(double t)
{
    t = qBound(0.0, t, 1.0);
    return t * t * t;
}

void LookToScroll::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    m_lastTickMs = -1;
    m_lastSampleMs = -1;
    m_accumV = 0.0;
    m_accumH = 0.0;
    m_outsideSec = 0.0;
    m_centerProgress = 0.0;
    m_scrollSuspended = false;
    if (!m_enabled) {
        hideOverlay();
    }
    GAZER_INFO << "LookToScroll" << (m_enabled ? "ON" : "OFF");
    emit enabledChanged(m_enabled);
    emit scrollSuspendedChanged(false);
}

void LookToScroll::toggle()
{
    setEnabled(!m_enabled);
}

void LookToScroll::setScrollSuspended(bool suspended)
{
    if (m_scrollSuspended == suspended) {
        return;
    }
    m_scrollSuspended = suspended;
    m_outsideSec = 0.0;
    m_accumV = 0.0;
    m_accumH = 0.0;
    m_centerProgress = 0.0;
    GAZER_INFO << "LookToScroll scroll" << (suspended ? "SUSPENDED" : "resumed");
    emit scrollSuspendedChanged(m_scrollSuspended);
}

void LookToScroll::setDeadzonePx(int px)
{
    m_deadzonePx = qMax(20, px);
}

void LookToScroll::setFalloffPx(int px)
{
    m_falloffPx = qMax(40, px);
}

void LookToScroll::setMaxNotchesPerSec(double n)
{
    m_maxNotchesPerSec = qBound(0.5, n, 30.0);
}

void LookToScroll::setAccelPerSec(double a)
{
    m_accelPerSec = qBound(0.0, a, 2.0);
}

void LookToScroll::setCenterDwellMs(int ms)
{
    m_centerDwellMs = qMax(200, ms);
}

void LookToScroll::setActiveWhenOverBoard(bool allow)
{
    m_allowOverBoard = allow;
}

void LookToScroll::hideOverlay()
{
    if (m_overlay) {
        m_overlay->hide();
    }
}

void LookToScroll::updateOverlay(const QPoint& center, double gazeDist, double dirX, double dirY,
                                 bool active, double centerProg, bool suspended)
{
    if (!m_overlay) {
        return;
    }
    double activity = 0.0;
    if (active && !suspended && gazeDist > m_deadzonePx) {
        const double t = qBound(0.0, (gazeDist - m_deadzonePx) / double(m_falloffPx), 1.0);
        activity = easeNearDeadzone(t);
    }
    m_overlay->setState(m_deadzonePx, m_falloffPx, activity, centerProg, suspended, dirX, dirY);
    m_overlay->placeCenter(center);
}

void LookToScroll::onGaze(const GazePoint& point, bool pauseInput)
{
    if (!m_enabled || !point.valid) {
        hideOverlay();
        return;
    }
    if (pauseInput && !m_allowOverBoard) {
        hideOverlay();
        m_centerProgress = 0.0;
        m_outsideSec = 0.0;
        return;
    }

    const QPoint origin = QCursor::pos();
    const QPointF gaze(point.x, point.y);
    const QPointF delta = gaze - QPointF(origin);
    const double dist = qSqrt(delta.x() * delta.x() + delta.y() * delta.y());
    const double dirX = dist > 1.0 ? delta.x() / dist : 0.0;
    const double dirY = dist > 1.0 ? delta.y() / dist : 0.0;
    const int centerR = qMax(14, m_deadzonePx / 3);

    const qint64 now = m_clock.elapsed();
    // Per-sample dt for center dwell / decay (always advance timestamp after use).
    const double sampleDt =
        m_lastSampleMs < 0 ? 0.016
                           : qBound(0.004, (now - m_lastSampleMs) / 1000.0, 0.05);
    m_lastSampleMs = now;

    // Center dwell: suspend scroll, or request a new scroll-point placement on resume.
    if (dist <= double(centerR)) {
        m_outsideSec = 0.0;
        m_accumV *= 0.5;
        m_accumH *= 0.5;
        m_centerProgress =
            qBound(0.0, m_centerProgress + sampleDt * 1000.0 / double(m_centerDwellMs), 1.0);
        updateOverlay(origin, dist, dirX, dirY, false, m_centerProgress, m_scrollSuspended);
        if (m_centerProgress >= 1.0) {
            m_centerProgress = 0.0;
            if (!m_scrollSuspended) {
                setScrollSuspended(true);
            } else {
                // Stay suspended until host places cursor via Move-to.
                emit placeScrollPointRequested();
            }
        }
        return;
    }

    m_centerProgress = qMax(0.0, m_centerProgress - sampleDt * 2.5);

    if (dist <= m_deadzonePx || dist < 1.0) {
        m_outsideSec = 0.0;
        m_accumV *= 0.5;
        m_accumH *= 0.5;
        if (qAbs(m_accumV) < kMinEmitDelta) {
            m_accumV = 0.0;
        }
        if (qAbs(m_accumH) < kMinEmitDelta) {
            m_accumH = 0.0;
        }
        updateOverlay(origin, dist, dirX, dirY, false, m_centerProgress, m_scrollSuspended);
        return;
    }

    // Outside deadzone.
    updateOverlay(origin, dist, dirX, dirY, !m_scrollSuspended, m_centerProgress,
                  m_scrollSuspended);

    if (m_scrollSuspended) {
        m_outsideSec = 0.0;
        return;
    }

    if (m_lastTickMs >= 0 && (now - m_lastTickMs) < m_intervalMs) {
        return;
    }
    const double tickDt =
        m_lastTickMs < 0 ? (m_intervalMs / 1000.0)
                         : qBound(0.008, (now - m_lastTickMs) / 1000.0, 0.08);
    m_lastTickMs = now;
    m_outsideSec += tickDt;

    const double tLin = qBound(0.0, (dist - m_deadzonePx) / double(m_falloffPx), 1.0);
    const double t = easeNearDeadzone(tLin);
    const double accel = qMin(m_accelMax, 1.0 + m_accelPerSec * m_outsideSec);

    const double nx = delta.x() / dist;
    const double ny = delta.y() / dist;
    const double rate = m_maxNotchesPerSec * kWheelDelta * t * accel;
    m_accumV += (-ny) * rate * tickDt;
    m_accumH += (nx)*rate * tickDt;

    int v = 0;
    int h = 0;
    if (qAbs(m_accumV) >= kMinEmitDelta) {
        v = int(m_accumV > 0 ? qFloor(m_accumV) : qCeil(m_accumV));
        m_accumV -= v;
    }
    if (qAbs(m_accumH) >= kMinEmitDelta) {
        h = int(m_accumH > 0 ? qFloor(m_accumH) : qCeil(m_accumH));
        m_accumH -= h;
    }

    QString err;
    bool any = false;
    if (v != 0) {
        any = MouseInjector::scrollDelta(v, &err) || any;
    }
    if (h != 0) {
        any = MouseInjector::scrollHorizontalDelta(h, &err) || any;
    }
    if (any) {
        emit scrolled(v, h);
    } else if (!err.isEmpty()) {
        GAZER_WARN << "LookToScroll:" << err;
    }
}

} // namespace gazer
