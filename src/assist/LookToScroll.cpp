#include "assist/LookToScroll.h"

#include "input/MouseInjector.h"
#include "utils/Log.h"
#include "utils/WinOverlay.h"

#include <QCursor>
#include <QGuiApplication>
#include <QPainter>
#include <QPaintEvent>
#include <QScreen>
#include <QtMath>

namespace gazer {

namespace {
// Windows WHEEL_DELTA — one classic notch.
constexpr double kWheelDelta = 120.0;
// Emit even small high-res deltas (apps that support precision wheel feel smoother).
constexpr double kMinEmitDelta = 8.0;
} // namespace

class LookToScroll::RingOverlay final : public QWidget {
public:
    explicit RingOverlay()
        : QWidget(nullptr)
    {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool
                       | Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_QuitOnClose, false);
        resize(m_box, m_box);
        hide();
    }

    void setRing(int deadzonePx, double activity /*0..1*/)
    {
        m_deadzone = deadzonePx;
        m_activity = qBound(0.0, activity, 1.0);
        const int side = deadzonePx * 2 + 24;
        if (side != m_box) {
            m_box = side;
            resize(m_box, m_box);
        }
        update();
    }

    void placeCenter(const QPoint& screenCenter)
    {
        move(screenCenter.x() - width() / 2, screenCenter.y() - height() / 2);
        if (!isVisible()) {
            show();
            applyOverlayWindowChrome(this);
        }
    }

protected:
    void showEvent(QShowEvent* e) override
    {
        QWidget::showEvent(e);
        applyOverlayWindowChrome(this);
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QPoint c = rect().center();
        const int r = m_deadzone;

        // Soft fill inside deadzone
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 180, 220, 25));
        p.drawEllipse(c, r, r);

        // Border: brighter when actively scrolling
        const int alpha = 120 + int(120 * m_activity);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(0, 220, 255, alpha), 2.5 + 2.0 * m_activity));
        p.drawEllipse(c, r, r);

        // Outer activity ring
        if (m_activity > 0.02) {
            const int outer = r + int(18 + 40 * m_activity);
            p.setPen(QPen(QColor(0, 220, 255, int(80 * m_activity)), 1.5));
            p.drawEllipse(c, outer, outer);
        }
    }

private:
    int m_deadzone = 110;
    int m_box = 260;
    double m_activity = 0.0;
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
    // Cubic ease-in: very soft just outside the ring, full speed only near falloff end.
    // t in [0,1] → t^3 keeps near-deadzone motion gentle.
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
    m_accumV = 0.0;
    m_accumH = 0.0;
    if (!m_enabled) {
        hideOverlay();
    }
    GAZER_INFO << "LookToScroll" << (m_enabled ? "ON" : "OFF") << "deadzone" << m_deadzonePx
               << "falloff" << m_falloffPx << "maxNotches/s" << m_maxNotchesPerSec;
    emit enabledChanged(m_enabled);
}

void LookToScroll::toggle()
{
    setEnabled(!m_enabled);
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

void LookToScroll::updateOverlay(const QPoint& center, double gazeDist, bool active)
{
    if (!m_overlay) {
        return;
    }
    double activity = 0.0;
    if (active && gazeDist > m_deadzonePx) {
        const double t = qBound(0.0, (gazeDist - m_deadzonePx) / double(m_falloffPx), 1.0);
        activity = easeNearDeadzone(t);
    }
    m_overlay->setRing(m_deadzonePx, activity);
    m_overlay->placeCenter(center);
}

void LookToScroll::onGaze(const GazePoint& point, bool overBoard)
{
    if (!m_enabled || !point.valid) {
        hideOverlay();
        return;
    }
    if (overBoard && !m_allowOverBoard) {
        hideOverlay();
        return;
    }

    // Deadzone is centered on the *mouse cursor* (user request: scalar from mouse).
    const QPoint origin = QCursor::pos();
    const QPointF gaze(point.x, point.y);
    const QPointF delta = gaze - QPointF(origin);
    const double dist = qSqrt(delta.x() * delta.x() + delta.y() * delta.y());

    updateOverlay(origin, dist, dist > m_deadzonePx);

    if (dist <= m_deadzonePx || dist < 1.0) {
        // Inside deadzone: decay residual so we don't "kick" when leaving the ring.
        m_accumV *= 0.5;
        m_accumH *= 0.5;
        if (qAbs(m_accumV) < kMinEmitDelta) {
            m_accumV = 0.0;
        }
        if (qAbs(m_accumH) < kMinEmitDelta) {
            m_accumH = 0.0;
        }
        return;
    }

    const qint64 now = m_clock.elapsed();
    if (m_lastTickMs >= 0 && (now - m_lastTickMs) < m_intervalMs) {
        return;
    }
    // Real dt in seconds for rate-based smooth scrolling.
    const double dtSec =
        m_lastTickMs < 0
            ? (m_intervalMs / 1000.0)
            : qBound(0.008, (now - m_lastTickMs) / 1000.0, 0.08);
    m_lastTickMs = now;

    // 0 at deadzone edge → 1 at deadzone+falloff; cubic ease keeps near-ring soft.
    const double tLin = qBound(0.0, (dist - m_deadzonePx) / double(m_falloffPx), 1.0);
    const double t = easeNearDeadzone(tLin);

    const double nx = delta.x() / dist;
    const double ny = delta.y() / dist;

    // Continuous rate: notches/sec * WHEEL_DELTA * ease * direction * dt.
    // Looking up (negative screen y relative to cursor) → scroll up (positive wheel).
    const double rate = m_maxNotchesPerSec * kWheelDelta * t;
    m_accumV += (-ny) * rate * dtSec;
    m_accumH += (nx)*rate * dtSec;

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
