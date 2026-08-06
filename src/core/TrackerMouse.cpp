#include "core/TrackerMouse.h"

#include "utils/Log.h"

#include <QCursor>

namespace gazer {

TrackerMouse::TrackerMouse(QObject* parent)
    : ITracker(parent)
{
    m_timer.setInterval(16); // ~60 Hz
    connect(&m_timer, &QTimer::timeout, this, &TrackerMouse::onTick);
}

bool TrackerMouse::start()
{
    if (m_running) {
        return true;
    }
    m_elapsedMs = 0;
    m_running = true;
    m_timer.start();
    GAZER_INFO << "TrackerMouse started (~60 Hz cursor → gaze)";
    emit trackingRestored();
    return true;
}

void TrackerMouse::stop()
{
    if (!m_running) {
        return;
    }
    m_timer.stop();
    m_running = false;
    emit trackingLost();
}

bool TrackerMouse::isRunning() const
{
    return m_running;
}

QString TrackerMouse::name() const
{
    return QStringLiteral("Mouse");
}

void TrackerMouse::onTick()
{
    m_elapsedMs += m_timer.interval();
    const QPoint pos = QCursor::pos();

    GazePoint gp;
    gp.x = pos.x();
    gp.y = pos.y();
    gp.timestampMs = m_elapsedMs;
    gp.valid = true;
    emit gazeUpdated(gp);
}

} // namespace gazer
