#include "assist/GazeMouseFollow.h"

#include "input/MouseInjector.h"
#include "utils/Log.h"

#include <QElapsedTimer>
#include <QtMath>

namespace gazer {

GazeMouseFollow::GazeMouseFollow(QObject* parent)
    : QObject(parent)
{
    m_clock.start();
}

void GazeMouseFollow::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    m_hasPos = false;
    m_lastInjectMs = -1;
    m_lastInjected = {};
    GAZER_INFO << "GazeMouseFollow" << (m_enabled ? "ON" : "OFF");
    emit enabledChanged(m_enabled);
}

void GazeMouseFollow::toggle()
{
    setEnabled(!m_enabled);
}

void GazeMouseFollow::setSmoothAlpha(double a)
{
    m_alpha = qBound(0.05, a, 1.0);
}

void GazeMouseFollow::onGaze(const GazePoint& point, bool pauseInput)
{
    if (!m_enabled || !point.valid || pauseInput) {
        return;
    }
    const QPointF g(point.x, point.y);
    if (!m_hasPos) {
        m_smooth = g;
        m_hasPos = true;
    } else {
        m_smooth.setX(m_smooth.x() * (1.0 - m_alpha) + g.x() * m_alpha);
        m_smooth.setY(m_smooth.y() * (1.0 - m_alpha) + g.y() * m_alpha);
    }

    const QPoint pixel(qRound(m_smooth.x()), qRound(m_smooth.y()));
    if (pixel == m_lastInjected) {
        return;
    }

    // Cap injection rate (~60 Hz max).
    const qint64 now = m_clock.elapsed();
    if (m_lastInjectMs >= 0 && (now - m_lastInjectMs) < 16) {
        return;
    }
    m_lastInjectMs = now;
    m_lastInjected = pixel;

    QString err;
    MouseInjector::moveTo(pixel.x(), pixel.y(), &err);
}

} // namespace gazer
