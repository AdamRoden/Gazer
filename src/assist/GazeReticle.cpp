#include "assist/GazeReticle.h"

#include "ui/OverlaySurface.h"

#include <QPainter>
#include <QPaintEvent>
#include <QtMath>

namespace gazer {

class GazeReticle::Overlay final : public OverlaySurface {
public:
    Overlay()
    {
        resize(156, 156);
        hide();
    }

    void place(const QPoint& c, double opacity)
    {
        m_opacity = qBound(0.0, opacity, 1.0);
        move(c.x() - width() / 2, c.y() - height() / 2);
        showOverlay();
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QPointF c = QRectF(rect()).center();
        const int a = int(255.0 * m_opacity);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 220, 255, a));
        p.drawEllipse(c, 75.0, 75.0);
    }

private:
    double m_opacity = 0.25;
};

GazeReticle::GazeReticle(QObject* parent)
    : QObject(parent)
{
    m_clock.start();
    m_overlay = std::make_unique<Overlay>();
    m_stickiness = GazeFollowStickiness::fromProfile(1);
}

GazeReticle::~GazeReticle() = default;

void GazeReticle::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    m_lastMs = -1;
    m_havePos = false;
    m_smoothValid = false;
    m_opacity = 0.25;
    if (!m_enabled && m_overlay) {
        m_overlay->hide();
    }
    emit enabledChanged(m_enabled);
}

void GazeReticle::toggle()
{
    setEnabled(!m_enabled);
}

void GazeReticle::setFollowProfile(int profile)
{
    m_stickiness = GazeFollowStickiness::fromProfile(profile);
}

void GazeReticle::onGaze(const GazePoint& point)
{
    if (!m_enabled || !point.valid || !m_overlay) {
        if (m_overlay && m_enabled) {
            m_overlay->hide();
        }
        m_havePos = false;
        m_smoothValid = false;
        m_lastMs = -1;
        return;
    }

    const QPointF raw(point.x, point.y);
    m_stickiness.smoothPoint(m_smooth, m_smoothValid, raw);
    const QPointF pos = m_smooth;
    const qint64 now = m_clock.elapsed();

    constexpr double kMinOpacity = 0.05;
    constexpr double kMaxOpacity = 0.50;
    constexpr double kFastPxPerSec = 900.0;

    double target = kMinOpacity;
    if (m_havePos && m_lastMs >= 0) {
        const double dt = qBound(0.004, (now - m_lastMs) / 1000.0, 0.08);
        const double dx = pos.x() - m_lastPos.x();
        const double dy = pos.y() - m_lastPos.y();
        const double speed = qSqrt(dx * dx + dy * dy) / dt;
        const double t = qBound(0.0, speed / kFastPxPerSec, 1.0);
        const double s = t * t * (3.0 - 2.0 * t);
        target = kMinOpacity + (kMaxOpacity - kMinOpacity) * s;
    }

    constexpr double kAlpha = 0.08;
    m_opacity = m_opacity * (1.0 - kAlpha) + target * kAlpha;

    m_lastPos = pos;
    m_lastMs = now;
    m_havePos = true;

    m_overlay->place(QPoint(qRound(pos.x()), qRound(pos.y())), m_opacity);
}

} // namespace gazer
