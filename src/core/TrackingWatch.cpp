#include "core/TrackingWatch.h"

#include <QtGlobal>

namespace gazer {

void TrackingWatch::keepPastBlink(int& ms)
{
    const int floor = m_blinkGraceMs + 1;
    if (ms < floor) {
        ms = floor;
    }
}

void TrackingWatch::setBlinkGraceMs(int ms)
{
    m_blinkGraceMs = qMax(0, ms);
    keepPastBlink(m_sustainedInvalidMs);
    keepPastBlink(m_gazeAbsentMs);
}

void TrackingWatch::setStreamStaleMs(int ms)
{
    m_streamStaleMs = qMax(1, ms);
}

void TrackingWatch::setSustainedInvalidMs(int ms)
{
    m_sustainedInvalidMs = qMax(1, ms);
    keepPastBlink(m_sustainedInvalidMs);
}

void TrackingWatch::setGazeAbsentMs(int ms)
{
    m_gazeAbsentMs = qMax(1, ms);
    keepPastBlink(m_gazeAbsentMs);
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
        // The sample that brings gaze back clears the hold. The resume latch
        // keeps that sample from activating a board.
        if (m_gazeHold) {
            return resume();
        }
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
    if (nowMs - m_invalidSinceMs >= m_gazeAbsentMs) {
        return enterGaze(GazeLossReason::SustainedInvalid);
    }
    return {};
}

TrackingWatch::Transition TrackingWatch::onHead(const HeadPose& pose, qint64 nowMs)
{
    if (!pose.valid()) {
        // Do not refresh m_lastHeadMs. A stream of invalid poses must age out
        // through poll(), and a long enough run is its own head-only loss.
        // Boards stay available: enterHead() does not set the gaze hold.
        if (!m_hadLiveHead || m_gazeHold || m_headHold) {
            return {};
        }
        if (!m_headInvalidOpen) {
            m_headInvalidOpen = true;
            m_headInvalidSinceMs = nowMs;
            return {};
        }
        if (nowMs - m_headInvalidSinceMs >= m_sustainedInvalidMs) {
            return enterHead();
        }
        return {};
    }

    m_headSeen = true;
    m_lastHeadMs = nowMs;
    m_hadLiveHead = true;
    m_headInvalidOpen = false;
    // Head-only hold ends on the first valid pose so the mapper can take its
    // paused recovery sample and a later stall can neutralize again.
    // Gaze loss is not cleared here.
    if (m_headHold && !m_gazeHold) {
        m_headHold = false;
    }
    return {};
}

TrackingWatch::Transition TrackingWatch::onDisconnect(qint64 /*nowMs*/)
{
    return enterGaze(GazeLossReason::Disconnected);
}

TrackingWatch::Transition TrackingWatch::poll(qint64 nowMs)
{
    Transition t;
    if (!m_running) {
        return t;
    }
    if (!m_gazeHold) {
        const qint64 ref = m_gazeSeen ? m_lastGazeMs : m_streamStartMs;
        if (nowMs - ref >= m_gazeAbsentMs) {
            t = enterGaze(GazeLossReason::NoCallbacks);
        }
    }
    if (!m_gazeHold && !m_headHold && m_headSeen && (nowMs - m_lastHeadMs) >= m_streamStaleMs) {
        t = enterHead();
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
    m_headInvalidOpen = false;
    if (!m_gazeHold && !m_headHold) {
        return t;
    }
    m_gazeHold = false;
    m_headHold = false;
    m_gazeReason = GazeLossReason::None;
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
    if (m_gazeHold) {
        return QStringLiteral("No gaze detected");
    }
    if (m_headHold) {
        return QStringLiteral("Head tracking paused. Stick and head commands released.");
    }
    return QStringLiteral("Tracking paused. Outputs released.");
}

} // namespace gazer
