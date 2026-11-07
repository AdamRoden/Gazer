#include "core/FakeTracker.h"

namespace gazer {

FakeTracker::FakeTracker(QObject* parent)
    : ITracker(parent)
{
}

bool FakeTracker::start()
{
    m_running = true;
    return true;
}

void FakeTracker::stop()
{
    m_running = false;
    emit trackingLost();
}

bool FakeTracker::isRunning() const
{
    return m_running;
}

QString FakeTracker::name() const
{
    return QStringLiteral("Fake");
}

void FakeTracker::emitGaze(const GazePoint& point)
{
    emit gazeUpdated(point);
}

void FakeTracker::emitHead(const HeadPose& pose)
{
    emit headPoseUpdated(pose);
}

void FakeTracker::emitGazeStale()
{
    emit gazeStreamStale();
}

void FakeTracker::emitHeadStale()
{
    emit headStreamStale();
}

void FakeTracker::emitDisconnected()
{
    emit deviceDisconnected();
}

void FakeTracker::emitReconnected()
{
    emit deviceReconnected();
}

} // namespace gazer
