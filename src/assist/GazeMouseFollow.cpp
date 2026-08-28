#include "assist/GazeMouseFollow.h"

#include "input/MouseInjector.h"
#include "utils/Log.h"

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

void GazeMouseFollow::setFollowProfile(GazeFollowProfile profile)
{
    m_stickiness = GazeFollowStickiness::fromProfile(profile);
}

void GazeMouseFollow::onGaze(const GazePoint& point, bool pauseInput)
{
    if (!m_enabled || !point.valid || pauseInput) {
        return;
    }
    const QPointF raw(point.x, point.y);
    m_stickiness.smoothPoint(m_smooth, m_hasPos, raw);

    const QPoint pixel(qRound(m_smooth.x()), qRound(m_smooth.y()));
    if (pixel == m_lastInjected) {
        return;
    }

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
