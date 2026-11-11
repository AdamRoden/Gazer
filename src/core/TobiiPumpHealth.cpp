#include "core/TobiiPumpHealth.h"

namespace gazer {

void TobiiPumpHealth::noteStreamStart(qint64 nowMs)
{
    m_started = true;
    m_streamStartMs = nowMs;
    m_gazeSeen = false;
    m_headSeen = false;
    m_gazeStaleLatched = false;
    m_headStaleLatched = false;
    m_disconnectLatched = false;
    m_reconnectLatched = false;
}

void TobiiPumpHealth::onGazeCallback(qint64 nowMs)
{
    m_gazeSeen = true;
    m_lastGazeMs = nowMs;
    m_gazeStaleLatched = false;
    m_reconnectLatched = false;
}

void TobiiPumpHealth::onHeadCallback(qint64 nowMs)
{
    m_headSeen = true;
    m_lastHeadMs = nowMs;
    m_headStaleLatched = false;
}

TobiiPumpHealth::Edges TobiiPumpHealth::checkAges(qint64 nowMs)
{
    Edges e;
    if (!m_started) {
        return e;
    }
    const qint64 gazeRef = m_gazeSeen ? m_lastGazeMs : m_streamStartMs;
    if (!m_gazeStaleLatched && nowMs - gazeRef >= TrackingWatch::kDefaultGazeAbsentMs) {
        m_gazeStaleLatched = true;
        e.gazeStale = true;
    }
    if (m_headSubscribed && m_headSeen && !m_headStaleLatched
        && nowMs - m_lastHeadMs >= TrackingWatch::kDefaultStreamStaleMs) {
        m_headStaleLatched = true;
        e.headStale = true;
    }
    return e;
}

TobiiPumpHealth::Edges TobiiPumpHealth::onWaitTimeout(qint64 nowMs)
{
    return checkAges(nowMs);
}

TobiiPumpHealth::Edges TobiiPumpHealth::onConnectionFailed(qint64 /*nowMs*/)
{
    Edges e;
    if (!m_disconnectLatched) {
        m_disconnectLatched = true;
        e.disconnected = true;
    }
    return e;
}

TobiiPumpHealth::Edges TobiiPumpHealth::onAfterCallbacks(qint64 nowMs)
{
    return checkAges(nowMs);
}

TobiiPumpHealth::Edges TobiiPumpHealth::onReconnectFinished(bool ok, qint64 /*nowMs*/,
                                                           const QString& reason)
{
    Edges e;
    if (!ok) {
        e.failed = true;
        e.reason = reason.isEmpty() ? QStringLiteral("reconnect failed") : reason;
        return e;
    }
    m_disconnectLatched = false;
    if (!m_reconnectLatched) {
        m_reconnectLatched = true;
        e.reconnected = true;
    }
    return e;
}

} // namespace gazer
