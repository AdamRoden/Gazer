#pragma once

#include "core/GazePoint.h"
#include "core/HeadPose.h"

#include <QString>

namespace gazer {

/// Distinguishes a blink from a stalled gaze or head stream.
///
/// Invalid samples that keep arriving are a blink until `sustainedInvalidMs`.
/// No samples at all is a stall: `streamStaleMs` after the last sample, or
/// `startupSilenceMs` if the stream has never delivered one. A disconnect is
/// immediate. Loss holds outputs until `resume()`; a later sample does not
/// clear the hold and does not count as permission to activate.
class TrackingWatch {
public:
    static constexpr int kDefaultBlinkGraceMs = 200;
    static constexpr int kDefaultStreamStaleMs = 800;
    static constexpr int kDefaultSustainedInvalidMs = 1200;
    /// Silence before the first sample. Longer than a mid-session stall so
    /// device startup does not immediately cover the boards.
    static constexpr int kDefaultStartupSilenceMs = 3000;

    enum class GazeLossReason { None, NoCallbacks, SustainedInvalid, Disconnected };

    struct Transition {
        bool enteredGazeLoss = false;
        bool enteredHeadLoss = false;
        bool resumed = false;
    };

    void setBlinkGraceMs(int ms);
    void setStreamStaleMs(int ms);
    void setSustainedInvalidMs(int ms);
    void setStartupSilenceMs(int ms);

    void noteStreamRunning(qint64 nowMs);
    void noteStreamStopped();

    [[nodiscard]] Transition onGaze(const GazePoint& point, qint64 nowMs);
    [[nodiscard]] Transition onHead(const HeadPose& pose, qint64 nowMs);
    [[nodiscard]] Transition onDisconnect(qint64 nowMs);
    void onReconnect(qint64 nowMs);
    [[nodiscard]] Transition poll(qint64 nowMs);
    /// Worker already decided the gaze or head stream is stale.
    [[nodiscard]] Transition noteGazeStale(GazeLossReason reason);
    [[nodiscard]] Transition noteHeadStale();
    [[nodiscard]] Transition resume();

    [[nodiscard]] bool blocksBoardGaze() const { return m_gazeHold; }
    [[nodiscard]] bool headHeld() const { return m_headHold; }
    [[nodiscard]] bool lossHold() const { return m_gazeHold || m_headHold; }
    /// False while a gaze or head hold is active, including after samples return.
    [[nodiscard]] bool allowsActivation() const { return !m_gazeHold && !m_headHold; }
    [[nodiscard]] bool hadLiveGaze() const { return m_hadLiveGaze; }
    [[nodiscard]] bool hadLiveHead() const { return m_hadLiveHead; }
    [[nodiscard]] GazeLossReason gazeLossReason() const { return m_gazeReason; }
    [[nodiscard]] bool reconnectedPendingResume() const { return m_reconnected; }
    /// The sample that acknowledged Resume must not be dispatched to boards.
    [[nodiscard]] bool consumeResumeLatch();
    [[nodiscard]] QString statusText() const;

private:
    [[nodiscard]] Transition enterGaze(GazeLossReason reason);
    [[nodiscard]] Transition enterHead();
    void keepSustainedPastBlink();

    int m_blinkGraceMs = kDefaultBlinkGraceMs;
    int m_streamStaleMs = kDefaultStreamStaleMs;
    int m_sustainedInvalidMs = kDefaultSustainedInvalidMs;
    int m_startupSilenceMs = kDefaultStartupSilenceMs;

    bool m_running = false;
    qint64 m_streamStartMs = 0;
    bool m_gazeSeen = false;
    qint64 m_lastGazeMs = 0;
    bool m_hadLiveGaze = false;
    bool m_invalidOpen = false;
    qint64 m_invalidSinceMs = 0;

    bool m_headSeen = false;
    qint64 m_lastHeadMs = 0;
    bool m_hadLiveHead = false;

    bool m_gazeHold = false;
    bool m_headHold = false;
    GazeLossReason m_gazeReason = GazeLossReason::None;
    bool m_reconnected = false;
    bool m_resumeLatch = false;
};

} // namespace gazer
