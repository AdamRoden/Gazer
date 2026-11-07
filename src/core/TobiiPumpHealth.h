#pragma once

#include "core/TrackingWatch.h"

#include <QString>

namespace gazer {

/// Edge detector for the Tobii worker loop. No SDK types.
/// Timeouts with no gaze callback become one gaze-stale edge. Connection
/// failure is immediate. Reconnect does not invent a gaze sample.
class TobiiPumpHealth {
public:
    struct Edges {
        bool gazeStale = false;
        bool headStale = false;
        bool disconnected = false;
        bool reconnected = false;
        bool failed = false;
        QString reason;
    };

    void setHeadSubscribed(bool on) { m_headSubscribed = on; }
    void noteStreamStart(qint64 nowMs);
    void onGazeCallback(qint64 nowMs);
    void onHeadCallback(qint64 nowMs);

    [[nodiscard]] Edges onWaitTimeout(qint64 nowMs);
    [[nodiscard]] Edges onConnectionFailed(qint64 nowMs);
    [[nodiscard]] Edges onAfterCallbacks(qint64 nowMs);
    [[nodiscard]] Edges onReconnectFinished(bool ok, qint64 nowMs, const QString& reason);

private:
    [[nodiscard]] Edges checkAges(qint64 nowMs);

    bool m_headSubscribed = false;
    bool m_started = false;
    qint64 m_streamStartMs = 0;
    bool m_gazeSeen = false;
    qint64 m_lastGazeMs = 0;
    bool m_headSeen = false;
    qint64 m_lastHeadMs = 0;
    bool m_gazeStaleLatched = false;
    bool m_headStaleLatched = false;
    bool m_disconnectLatched = false;
    bool m_reconnectLatched = false;
};

} // namespace gazer
