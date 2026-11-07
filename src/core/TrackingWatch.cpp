#include "core/TrackingWatch.h"

#include <QtGlobal>

namespace gazer {

void TrackingWatch::keepSustainedPastBlink()
{
    const int floor = m_blinkGraceMs + 1;
    if (m_sustainedInvalidMs < floor) {
        m_sustainedInvalidMs = floor;
    }
}

void TrackingWatch::setBlinkGraceMs(int ms)
{
    m_blinkGraceMs = qMax(0, ms);
    keepSustainedPastBlink();
}

void TrackingWatch::setStreamStaleMs(int ms)
{
    m_streamStaleMs = qMax(1, ms);
}

void TrackingWatch::setSustainedInvalidMs(int ms)
{
    m_sustainedInvalidMs = qMax(1, ms);
    keepSustainedPastBlink();
}

void TrackingWatch::setStartupSilenceMs(int ms)
{
    m_startupSilenceMs = qMax(1, ms);
}

void TrackingWatch::noteStreamRunning(qint64 nowMs)
{
    if (m_running) {
        return;
    }
    m_running = true;
    m_streamStartMs = nowMs;
}

void TrackingWatch::noteStreamStopped()
{
    m_running = false;
}

TrackingWatch::Transition TrackingWatch::enterGaze(GazeLossReason reason)
{
    Transition t;
    if (!m_gazeHold) {
        m_gazeHold = true;
        m_gazeReason = reason;
        m_reconnected = false;
        t.enteredGazeLoss = true;
    }
    if (!m_headHold) {
        m_headHold = true;
        t.enteredHeadLoss = true;
    }
    return t;
}

TrackingWatch::Transition TrackingWatch::enterHead()
{
    Transition t;
    if (m_gazeHold || m_headHold) {
        return t;
    }
    m_headHold = true;
    t.enteredHeadLoss = true;
    return t;
}

TrackingWatch::Transition TrackingWatch::onGaze(const GazePoint& point, qint64 nowMs)
{
    m_gazeSeen = true;
    m_lastGazeMs = nowMs;
    if (point.valid) {
        m_hadLiveGaze = true;
        m_invalidOpen = false;
        // A sample after loss does not release the hold and does not activate.
        return {};
    }
    if (!m_hadLiveGaze || m_gazeHold) {
        return {};
    }
    if (!m_invalidOpen) {
        m_invalidOpen = true;
        m_invalidSinceMs = nowMs;
        return {};
    }
    if (nowMs - m_invalidSinceMs >= m_sustainedInvalidMs) {
        return enterGaze(GazeLossReason::SustainedInvalid);
    }
    return {};
}

TrackingWatch::Transition TrackingWatch::onHead(const HeadPose& pose, qint64 nowMs)
{
    m_headSeen = true;
    m_lastHeadMs = nowMs;
    if (pose.valid()) {
        m_hadLiveHead = true;
    }
    return {};
}

TrackingWatch::Transition TrackingWatch::onDisconnect(qint64 /*nowMs*/)
{
    return enterGaze(GazeLossReason::Disconnected);
}

void TrackingWatch::onReconnect(qint64 /*nowMs*/)
{
    if (m_gazeHold || m_headHold) {
        m_reconnected = true;
    }
}

TrackingWatch::Transition TrackingWatch::poll(qint64 nowMs)
{
    Transition t;
    if (!m_running) {
        return t;
    }
    if (!m_gazeHold) {
        const qint64 ref = m_gazeSeen ? m_lastGazeMs : m_streamStartMs;
        const int limit = m_gazeSeen ? m_streamStaleMs : m_startupSilenceMs;
        if (nowMs - ref >= limit) {
            t = enterGaze(GazeLossReason::NoCallbacks);
        }
    }
    if (!m_gazeHold && !m_headHold && m_hadLiveHead && m_gazeSeen) {
        const bool gazeFresh = (nowMs - m_lastGazeMs) < m_streamStaleMs;
        if (gazeFresh && (nowMs - m_lastHeadMs) >= m_streamStaleMs) {
            t = enterHead();
        }
    }
    return t;
}

TrackingWatch::Transition TrackingWatch::noteGazeStale(GazeLossReason reason)
{
    if (m_gazeHold) {
        return {};
    }
    if (reason == GazeLossReason::None) {
        reason = GazeLossReason::NoCallbacks;
    }
    return enterGaze(reason);
}

TrackingWatch::Transition TrackingWatch::noteHeadStale()
{
    m_hadLiveHead = true;
    return enterHead();
}

TrackingWatch::Transition TrackingWatch::resume()
{
    Transition t;
    if (!m_gazeHold && !m_headHold) {
        return t;
    }
    m_gazeHold = false;
    m_headHold = false;
    m_gazeReason = GazeLossReason::None;
    m_reconnected = false;
    m_invalidOpen = false;
    m_resumeLatch = true;
    t.resumed = true;
    return t;
}

bool TrackingWatch::consumeResumeLatch()
{
    const bool latched = m_resumeLatch;
    m_resumeLatch = false;
    return latched;
}

QString TrackingWatch::statusText() const
{
    if (m_gazeHold && m_reconnected) {
        return QStringLiteral(
            "Eye tracker reconnected. Dwell Resume. Nothing clicks until you do.");
    }
    switch (m_gazeReason) {
    case GazeLossReason::NoCallbacks:
        return QStringLiteral("Eye tracking paused. No gaze data. Outputs released.");
    case GazeLossReason::SustainedInvalid:
        return QStringLiteral("Eye tracking paused. Gaze lost. Outputs released.");
    case GazeLossReason::Disconnected:
        return QStringLiteral("Eye tracker disconnected. Outputs released.");
    case GazeLossReason::None:
        break;
    }
    if (m_headHold) {
        return QStringLiteral("Head tracking paused. Stick and head commands released.");
    }
    return QStringLiteral("Tracking paused. Outputs released.");
}

} // namespace gazer
