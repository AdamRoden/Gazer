#include "assist/LookToScroll.h"

#include "assist/LtsSpeed.h"
#include "input/MouseInjector.h"
#include "ui/KeySymbols.h"
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
constexpr double kMinEmitPx = 1.0;

void paintRoundProgress(QPainter& p, const QPointF& c, double radius, double prog, const QColor& color)
{
    if (prog <= 0.01 || radius < 4.0) {
        return;
    }
    p.setBrush(Qt::NoBrush);
    const qreal w = qBound(2.6, radius * 0.14, 4.8);
    p.setPen(QPen(color, w, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(QRectF(c.x() - radius, c.y() - radius, radius * 2.0, radius * 2.0), 90 * 16,
              int(-360 * 16 * prog));
}

} // namespace

class LookToScroll::RingOverlay final : public OverlaySurface {
public:
    RingOverlay()
    {
        resize(m_box, m_box);
        hide();
    }

    void setState(int deadzonePx, int falloffPx, double activity, double centerProg, double dirX,
                  double dirY, LtsIndicator style, double hubRadius, bool paused = false)
    {
        m_deadzone = deadzonePx;
        m_falloff = qMax(40, falloffPx);
        m_activity = qBound(0.0, activity, 1.0);
        m_centerProg = qBound(0.0, centerProg, 1.0);
        m_dirX = dirX;
        m_dirY = dirY;
        m_style = style;
        m_hubR = qMax(8.0, hubRadius);
        m_paused = paused;

        const int activator = qMax(8, qRound(m_hubR));
        const bool compact = m_paused || m_style == LtsIndicator::PauseOnly;
        int side = activator * 2 + 36;
        if (!compact) {
            if (m_style == LtsIndicator::Orb) {
                const int stretch = qMax(48, m_falloff / 3);
                side = (m_deadzone + stretch) * 2 + 40;
            } else {
                const int maxOuter = m_deadzone + qMax(48, m_falloff / 3);
                side = maxOuter * 2 + 24;
            }
            side = qMax(side, activator * 2 + 36);
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
        if (!m_paused) {
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
        paintActivator(p, c, cyan);
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

    void paintActivator(QPainter& p, const QPointF& c, const QColor& accent)
    {
        const double hubR = m_hubR;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 50));
        p.drawEllipse(c, hubR, hubR);
        p.setBrush(QColor(accent.red(), accent.green(), accent.blue(), 40));
        p.drawEllipse(c, hubR, hubR);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(accent.red(), accent.green(), accent.blue(), 150), 2.2, Qt::SolidLine,
                      Qt::RoundCap));
        p.drawEllipse(c, hubR, hubR);
        if (m_paused) {
            const double side = hubR * 1.2;
            const QRectF icon(c.x() - side * 0.5, c.y() - side * 0.5, side, side);
            const QColor fg(255, 255, 255, 230);
            KeySymbols::paint(p, QStringLiteral("Sleep"), icon, fg);
        } else {
            paintRoundProgress(p, c, qMax(4.0, hubR - 1.5), m_centerProg, accent);
        }
    }

    int m_deadzone = 110;
    int m_falloff = 360;
    int m_box = 260;
    double m_hubR = 27.0;
    double m_activity = 0.0;
    double m_centerProg = 0.0;
    double m_dirX = 0.0;
    double m_dirY = 0.0;
    LtsIndicator m_style = LtsIndicator::Fan;
    bool m_paused = false;
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
    m_scroller.reset();
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
    const bool was = m_enabled;
    m_enabled = enabled;
    m_lastTickMs = -1;
    m_lastSampleMs = -1;
    m_accumV = 0.0;
    m_accumH = 0.0;
    m_outsideSec = 0.0;
    m_centerProgress = 0.0;
    m_replacing = false;
    m_scrollSuspended = false;
    m_plusOpen = false;
    m_lookAwaySec = 0.0;
    m_scroller.reset();
    if (!m_enabled) {
        m_hasOrigin = false;
        hideOverlay();
        if (was) {
            emit menuCloseRequested();
        }
    } else if (!m_hasOrigin) {
        setScrollOrigin(QCursor::pos());
    } else {
        pinCursorToOrigin();
    }
    GAZER_INFO << "LookToScroll" << (m_enabled ? "ON" : "OFF");
    emit enabledChanged(m_enabled);
    emit scrollSuspendedChanged(false);
}

void LookToScroll::setScrollOrigin(const QPoint& pos)
{
    m_origin = pos;
    m_hasOrigin = true;
    m_scroller.reset();
    pinCursorToOrigin();
}

void LookToScroll::pinCursorToOrigin()
{
    if (!m_hasOrigin) {
        return;
    }
    const QPoint now = QCursor::pos();
    // Qt/Windows DPI can round setPos by a pixel. Warping every sample against
    // that error injects a 1px mouse oscillation that shakes the scroll target.
    if (qAbs(now.x() - m_origin.x()) <= 2 && qAbs(now.y() - m_origin.y()) <= 2) {
        return;
    }
    QString err;
    if (!MouseInjector::moveTo(m_origin.x(), m_origin.y(), &err) && !err.isEmpty()) {
        GAZER_WARN << "LookToScroll pin cursor:" << err;
    }
}

QPoint LookToScroll::originPoint() const
{
    return m_hasOrigin ? m_origin : QCursor::pos();
}

void LookToScroll::toggle()
{
    setEnabled(!m_enabled);
}

void LookToScroll::nudgeMaxSpeed(int dir)
{
    const double next = nudgeLtsSpeed(m_maxNotchesPerSec, dir);
    if (qAbs(next - m_maxNotchesPerSec) < 0.001) {
        return;
    }
    m_maxNotchesPerSec = next;
    GAZER_INFO << "LookToScroll speed" << m_maxNotchesPerSec;
    emit maxNotchesPerSecChanged(m_maxNotchesPerSec);
}

void LookToScroll::resumeScroll()
{
    if (!m_enabled || !m_scrollSuspended) {
        return;
    }
    setScrollSuspended(false);
}

void LookToScroll::requestReset()
{
    if (!m_enabled) {
        return;
    }
    m_replacing = true;
    closePlus();
    setScrollSuspended(true);
    hideOverlay();
    emit placeScrollPointRequested();
}

void LookToScroll::cancelOriginPlace()
{
    if (!m_enabled || !m_replacing) {
        return;
    }
    m_replacing = false;
    if (m_scrollSuspended) {
        setScrollSuspended(false);
    }
}

void LookToScroll::pauseAtHub()
{
    m_replacing = false;
    if (!m_scrollSuspended) {
        setScrollSuspended(true);
    }
    openPlus(true);
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
    m_scroller.lift();
    if (!suspended) {
        m_replacing = false;
        closePlus();
        hideOverlay();
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
    m_maxNotchesPerSec = snapLtsSpeed(n);
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

void LookToScroll::showPausedHub()
{
    if (!m_overlay) {
        return;
    }
    m_overlay->setState(m_deadzonePx, m_falloffPx, 0.0, 0.0, 0.0, 0.0, m_indicatorStyle,
                        hubVisualRadiusPx(), true);
    m_overlay->placeCenter(originPoint());
}

void LookToScroll::openPlus(bool leaveGate)
{
    m_lookAwaySec = 0.0;
    hideOverlay();
    m_plusOpen = true;
    emit menuOpenRequested(originPoint(), leaveGate);
}

void LookToScroll::closePlus()
{
    if (!m_plusOpen) {
        return;
    }
    m_plusOpen = false;
    m_lookAwaySec = 0.0;
    emit menuCloseRequested();
}

bool LookToScroll::gazeOnPlus(const GazePoint& point) const
{
    if (!point.valid) {
        return false;
    }
    if (!m_menuContains) {
        return true;
    }
    return m_menuContains(point.toPointF());
}

void LookToScroll::updateOverlay(const QPoint& center, double gazeDist, double dirX, double dirY,
                                 bool active, double centerProg)
{
    if (!m_overlay) {
        return;
    }
    double activity = 0.0;
    if (active && !m_scrollSuspended && gazeDist > m_deadzonePx) {
        const double t = qBound(0.0, (gazeDist - m_deadzonePx) / double(m_falloffPx), 1.0);
        activity = easeNearDeadzone(t);
    }
    m_overlay->setState(m_deadzonePx, m_falloffPx, activity, centerProg, dirX, dirY,
                        m_indicatorStyle, hubVisualRadiusPx());
    m_overlay->placeCenter(center);
}

double LookToScroll::screenHeightPx() const
{
    QScreen* s = QGuiApplication::screenAt(originPoint());
    if (!s) {
        s = QGuiApplication::primaryScreen();
    }
    return s ? double(s->geometry().height()) : 1080.0;
}

void LookToScroll::updatePausedMenu(const GazePoint& point)
{
    m_scroller.lift();

    const qint64 now = m_clock.elapsed();
    const double sampleDt =
        m_lastSampleMs < 0 ? 0.016
                           : qBound(0.004, (now - m_lastSampleMs) / 1000.0, 0.05);
    m_lastSampleMs = now;

    if (m_plusOpen) {
        hideOverlay();
        if (gazeOnPlus(point)) {
            m_lookAwaySec = 0.0;
            return;
        }
        m_lookAwaySec += sampleDt;
        if (m_lookAwaySec < kLtsPlusDismissGraceSec) {
            return;
        }
        closePlus();
    }

    m_lookAwaySec = 0.0;
    if (point.valid) {
        const QPointF delta = point.toPointF() - QPointF(originPoint());
        const double dist = qSqrt(delta.x() * delta.x() + delta.y() * delta.y());
        if (dist <= hubDwellRadiusPx()) {
            openPlus(false);
            return;
        }
    }
    showPausedHub();
}

double LookToScroll::hubVisualRadiusPx() const
{
    return screenHeightPx() * (kLtsHubVisualDiameterFrac * 0.5);
}

double LookToScroll::hubDwellRadiusPx() const
{
    return screenHeightPx() * (kLtsHubDwellDiameterFrac * 0.5);
}

void LookToScroll::onGaze(const GazePoint& point, bool pauseInput)
{
    if (!m_enabled) {
        m_scroller.lift();
        hideOverlay();
        return;
    }
    if (m_replacing) {
        m_scroller.lift();
        hideOverlay();
        return;
    }
    if (m_scrollSuspended) {
        updatePausedMenu(point);
        return;
    }
    if (!point.valid) {
        m_scroller.lift();
        hideOverlay();
        return;
    }
    if (pauseInput && !m_allowOverBoard) {
        m_scroller.lift();
        hideOverlay();
        m_centerProgress = 0.0;
        m_outsideSec = 0.0;
        return;
    }

    const QPoint origin = originPoint();
    pinCursorToOrigin();
    const QPointF gaze(point.x, point.y);
    const QPointF delta = gaze - QPointF(origin);
    const double dist = qSqrt(delta.x() * delta.x() + delta.y() * delta.y());
    const double dirX = dist > 1.0 ? delta.x() / dist : 0.0;
    const double dirY = dist > 1.0 ? delta.y() / dist : 0.0;
    const double hubR = hubDwellRadiusPx();

    const qint64 now = m_clock.elapsed();
    const double sampleDt =
        m_lastSampleMs < 0 ? 0.016
                           : qBound(0.004, (now - m_lastSampleMs) / 1000.0, 0.05);
    m_lastSampleMs = now;

    if (dist <= hubR) {
        m_scroller.lift();
        m_outsideSec = 0.0;
        m_accumV *= 0.5;
        m_accumH *= 0.5;
        m_centerProgress =
            qBound(0.0, m_centerProgress + sampleDt * 1000.0 / double(m_centerDwellMs), 1.0);
        updateOverlay(origin, dist, dirX, dirY, false, m_centerProgress);
        if (m_centerProgress >= 1.0) {
            m_centerProgress = 0.0;
            pauseAtHub();
        }
        return;
    }

    m_centerProgress = qMax(0.0, m_centerProgress - sampleDt * 2.5);
    if (m_centerProgress <= 0.01) {
        m_centerProgress = 0.0;
    }

    if (dist <= m_deadzonePx || dist < 1.0) {
        m_scroller.lift();
        m_outsideSec = 0.0;
        m_accumV *= 0.5;
        m_accumH *= 0.5;
        if (qAbs(m_accumV) < kMinEmitPx) {
            m_accumV = 0.0;
        }
        if (qAbs(m_accumH) < kMinEmitPx) {
            m_accumH = 0.0;
        }
        updateOverlay(origin, dist, dirX, dirY, false, m_centerProgress);
        return;
    }

    updateOverlay(origin, dist, dirX, dirY, true, m_centerProgress);

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
    const double rate = m_maxNotchesPerSec * PixelScroller::kPixelsPerNotch * t * accel;
    m_accumV += (-ny) * rate * tickDt;
    m_accumH += (nx)*rate * tickDt;

    int v = 0;
    int h = 0;
    if (qAbs(m_accumV) >= kMinEmitPx) {
        v = int(m_accumV > 0 ? qFloor(m_accumV) : qCeil(m_accumV));
        m_accumV -= v;
    }
    if (qAbs(m_accumH) >= kMinEmitPx) {
        h = int(m_accumH > 0 ? qFloor(m_accumH) : qCeil(m_accumH));
        m_accumH -= h;
    }

    if (v == 0 && h == 0) {
        return;
    }

    QString err;
    if (m_scroller.scrollBy(h, v, &err)) {
        emit scrolled(v, h);
    } else if (!err.isEmpty()) {
        GAZER_WARN << "LookToScroll:" << err;
    }
}

} // namespace gazer
