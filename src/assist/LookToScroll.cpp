#include "assist/LookToScroll.h"

#include "input/MouseInjector.h"
#include "ui/OverlaySurface.h"
#include "ui/Theme.h"
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
                  double dirX, double dirY, LtsIndicator style, CenterDwell dwell)
    {
        m_deadzone = deadzonePx;
        m_falloff = qMax(40, falloffPx);
        m_activity = qBound(0.0, activity, 1.0);
        m_centerProg = qBound(0.0, centerProg, 1.0);
        m_suspended = suspended;
        m_dirX = dirX;
        m_dirY = dirY;
        m_style = style;
        m_dwell = dwell;

        const int activator = qMax(14, m_deadzone / 3);
        const bool compact = m_suspended || m_style == LtsIndicator::PauseOnly;
        int side = activator * 2 + 36;
        if (!compact) {
            if (m_style == LtsIndicator::Orb) {
                const int stretch = qMax(48, m_falloff / 3);
                side = (m_deadzone + stretch) * 2 + 40;
            } else {
                const int maxOuter = m_deadzone + qMax(48, m_falloff / 3);
                side = maxOuter * 2 + 24;
            }
        }
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

    void setAccent(const QColor& c)
    {
        if (c.isValid()) {
            m_accent = c;
            update();
        }
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QPointF c(rect().center());
        const QColor cyan = m_accent.isValid() ? m_accent : ThemeColors::defaultProgressColor();
        if (!m_suspended) {
            switch (m_style) {
            case LtsIndicator::Fan:
                paintFan(p, c, cyan);
                break;
            case LtsIndicator::Orb:
                paintOrb(p, c, cyan);
                break;
            case LtsIndicator::PauseOnly:
                break;
            }
        }
        paintActivator(p, c, activatorColor(cyan));
    }

private:
    void paintFan(QPainter& p, const QPointF& c, const QColor& cyan)
    {
        const int r = m_deadzone;
        if (r > 0) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(cyan.red(), cyan.green(), cyan.blue(), 16));
            p.drawEllipse(c, double(r), double(r));
        }
        if (m_activity <= 0.02) {
            return;
        }
        const double len = qSqrt(m_dirX * m_dirX + m_dirY * m_dirY);
        if (len <= 0.05) {
            return;
        }
        const double nx = m_dirX / len;
        const double ny = m_dirY / len;
        const int maxGrow = qMax(48, m_falloff / 3);
        const int rOuter = r + int(maxGrow * m_activity);
        const double midDeg = qRadiansToDegrees(qAtan2(-ny, nx));
        const double halfSpread = 22.0 + 28.0 * m_activity;

        QPainterPath pie;
        pie.moveTo(c);
        pie.arcTo(QRectF(c.x() - rOuter, c.y() - rOuter, rOuter * 2.0, rOuter * 2.0),
                  midDeg - halfSpread, halfSpread * 2.0);
        pie.closeSubpath();
        QPainterPath hole;
        hole.addEllipse(c, double(r), double(r));

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(cyan.red(), cyan.green(), cyan.blue(), int(28 + 90 * m_activity)));
        p.drawPath(pie.subtracted(hole));
    }

    void paintOrb(QPainter& p, const QPointF& c, const QColor& cyan)
    {
        const double R = double(qMax(1, m_deadzone));
        const double thickness = qBound(8.0, R * 0.16, 22.0);
        double nx = 0.0;
        double ny = 0.0;
        double stretch = 0.0;
        const double len = qSqrt(m_dirX * m_dirX + m_dirY * m_dirY);
        if (len > 0.05 && m_activity > 0.02) {
            nx = m_dirX / len;
            ny = m_dirY / len;
            stretch = qMax(48.0, m_falloff / 3.0) * m_activity;
        }
        const double ang = qRadiansToDegrees(qAtan2(ny, nx));
        const double midR = qMax(4.0, R - thickness * 0.5);

        auto strokeRing = [&](double extraStretch, double width, int alpha) {
            const double s = stretch + extraStretch;
            const double shift = s * 0.5;
            QPainterPath midline;
            midline.addEllipse(QPointF(shift, 0), midR + shift, midR);
            QPainterPathStroker stroker;
            stroker.setWidth(width);
            stroker.setCapStyle(Qt::RoundCap);
            stroker.setJoinStyle(Qt::RoundJoin);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(cyan.red(), cyan.green(), cyan.blue(), alpha));
            p.drawPath(stroker.createStroke(midline));
        };

        p.save();
        p.translate(c);
        if (stretch > 0.5) {
            p.rotate(ang);
        }
        strokeRing(14.0, thickness + 16.0, int(18 + 28 * m_activity));
        strokeRing(6.0, thickness + 7.0, int(40 + 50 * m_activity));
        strokeRing(0.0, thickness, int(90 + 90 * m_activity));
        strokeRing(0.0, qMax(2.5, thickness * 0.32), int(140 + 80 * m_activity));
        p.restore();
    }

    [[nodiscard]] QColor activatorColor(const QColor& cyan) const
    {
        switch (m_dwell) {
        case CenterDwell::Pause:
            return QColor(255, 196, 40);
        case CenterDwell::Quit:
            return QColor(232, 56, 48);
        case CenterDwell::Resume:
            return QColor(48, 196, 88);
        case CenterDwell::Idle:
            return m_suspended ? QColor(255, 160, 40) : cyan;
        }
        return cyan;
    }

    void paintActivator(QPainter& p, const QPointF& c, const QColor& accent)
    {
        const int cr = qMax(14, m_deadzone / 3);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, m_suspended ? 70 : 40));
        p.drawEllipse(c, double(cr), double(cr));
        p.setBrush(QColor(accent.red(), accent.green(), accent.blue(), m_suspended ? 55 : 35));
        p.drawEllipse(c, double(cr), double(cr));
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(accent.red(), accent.green(), accent.blue(), 140), 2.0));
        p.drawEllipse(c, double(cr), double(cr));
        if (m_centerProg > 0.01) {
            p.setPen(QPen(accent, 3.5));
            p.drawArc(QRectF(c.x() - cr, c.y() - cr, cr * 2.0, cr * 2.0), 90 * 16,
                      int(-360 * 16 * m_centerProg));
        }
    }

    int m_deadzone = 110;
    int m_falloff = 360;
    int m_box = 260;
    double m_activity = 0.0;
    double m_centerProg = 0.0;
    double m_dirX = 0.0;
    double m_dirY = 0.0;
    bool m_suspended = false;
    LtsIndicator m_style = LtsIndicator::Fan;
    CenterDwell m_dwell = CenterDwell::Idle;
    QColor m_accent = ThemeColors::defaultProgressColor();
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
    m_holdToQuit = false;
    m_centerDwell = CenterDwell::Idle;
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
    if (!suspended) {
        m_holdToQuit = false;
    }
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

void LookToScroll::setAccent(const QColor& c)
{
    if (m_overlay) {
        m_overlay->setAccent(c);
    }
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

void LookToScroll::setIndicatorStyle(LtsIndicator style)
{
    m_indicatorStyle = style;
}

void LookToScroll::hideOverlay()
{
    if (m_overlay) {
        m_overlay->hide();
    }
}

void LookToScroll::updateOverlay(const QPoint& center, double gazeDist, double dirX, double dirY,
                                 bool active, double centerProg, bool suspended, CenterDwell dwell)
{
    if (!m_overlay) {
        return;
    }
    double activity = 0.0;
    if (active && !suspended && gazeDist > m_deadzonePx) {
        const double t = qBound(0.0, (gazeDist - m_deadzonePx) / double(m_falloffPx), 1.0);
        activity = easeNearDeadzone(t);
    }
    m_overlay->setState(m_deadzonePx, m_falloffPx, activity, centerProg, suspended, dirX, dirY,
                        m_indicatorStyle, dwell);
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
        m_holdToQuit = false;
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

    // Center dwell: yellow pause → hold for red quit, or leave and return for green resume.
    if (dist <= double(centerR)) {
        m_outsideSec = 0.0;
        m_accumV *= 0.5;
        m_accumH *= 0.5;
        m_centerProgress =
            qBound(0.0, m_centerProgress + sampleDt * 1000.0 / double(m_centerDwellMs), 1.0);
        m_centerDwell = CenterDwell::Pause;
        if (m_scrollSuspended) {
            m_centerDwell = m_holdToQuit ? CenterDwell::Quit : CenterDwell::Resume;
        }
        updateOverlay(origin, dist, dirX, dirY, false, m_centerProgress, m_scrollSuspended,
                      m_centerDwell);
        if (m_centerProgress >= 1.0) {
            m_centerProgress = 0.0;
            if (!m_scrollSuspended) {
                setScrollSuspended(true);
                m_holdToQuit = true;
            } else if (m_holdToQuit) {
                setEnabled(false);
            } else {
                emit placeScrollPointRequested();
            }
        }
        return;
    }

    m_holdToQuit = false;
    m_centerProgress = qMax(0.0, m_centerProgress - sampleDt * 2.5);
    if (m_centerProgress <= 0.01) {
        m_centerDwell = CenterDwell::Idle;
        m_centerProgress = 0.0;
    }

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
        updateOverlay(origin, dist, dirX, dirY, false, m_centerProgress, m_scrollSuspended,
                      m_centerDwell);
        return;
    }

    // Outside deadzone.
    updateOverlay(origin, dist, dirX, dirY, !m_scrollSuspended, m_centerProgress,
                  m_scrollSuspended, m_centerDwell);

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
