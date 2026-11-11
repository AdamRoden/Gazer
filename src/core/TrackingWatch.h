#pragma once

#include "core/GazePoint.h"
#include "core/HeadPose.h"

#include <QString>

namespace gazer {

/// Distinguishes a blink from a stalled gaze or head stream.
///
/// Invalid gaze, a quiet gaze stream, and silence before the first sample all
/// wait `gazeAbsentMs`. Invalid head does not refresh the head clock; a brief
/// gap is not a stall, and a sustained run of invalid head (or no head samples)
/// is its own loss. A disconnect is immediate. A valid gaze sample clears gaze
/// loss. Head-only loss clears when a valid head pose returns, so the next
/// stall can neutralize again.
class TrackingWatch {
public:
    static constexpr int kDefaultBlinkGraceMs = 200;
    /// No head samples. Gaze absence uses `kDefaultGazeAbsentMs`.
    static constexpr int kDefaultStreamStaleMs = 10000;
    /// Invalid head poses. Gaze absence uses `kDefaultGazeAbsentMs`.
    static constexpr int kDefaultSustainedInvalidMs = 10000;
    /// Tracker is up and gaze is missing, including before the first sample.
    static constexpr int kDefaultGazeAbsentMs = 60000;

    enum class GazeLossReason { None, NoCallbacks, SustainedInvalid, Disconnected };

    struct Transition {
        bool enteredGazeLoss = false;
        bool enteredHeadLoss = false;
        bool resumed = false;
    };

    void setBlinkGraceMs(int ms);
    void setStreamStaleMs(int ms);
    void setSustainedInvalidMs(int ms);
    void setGazeAbsentMs(int ms);

    void noteStreamRunning(qint64 nowMs);
    void noteStreamStopped();

    [[nodiscard]] Transition onGaze(const GazePoint& point, qint64 nowMs);
    [[nodiscard]] Transition onHead(const HeadPose& pose, qint64 nowMs);
    [[nodiscard]] Transition onDisconnect(qint64 nowMs);
    [[nodiscard]] Transition poll(qint64 nowMs);
    /// Worker already decided the gaze or head stream is stale.
    [[nodiscard]] Transition noteGazeStale(GazeLossReason reason);
    [[nodiscard]] Transition noteHeadStale();
    [[nodiscard]] Transition resume();

    [[nodiscard]] bool blocksBoardGaze() const { return m_gazeHold; }
    [[nodiscard]] bool headHeld() const { return m_headHold; }
    [[nodiscard]] bool lossHold() const { return m_gazeHold || m_headHold; }
    /// False during a gaze hold, or during a head-only hold that has not yet
    /// seen a valid pose come back. A valid gaze sample clears gaze loss.
    [[nodiscard]] bool allowsActivation() const { return !m_gazeHold && !m_headHold; }
    [[nodiscard]] bool hadLiveGaze() const { return m_hadLiveGaze; }
    [[nodiscard]] bool hadLiveHead() const { return m_hadLiveHead; }
    [[nodiscard]] GazeLossReason gazeLossReason() const { return m_gazeReason; }
    /// The sample that cleared gaze loss must not be dispatched to boards.
    [[nodiscard]] bool consumeResumeLatch();
    [[nodiscard]] QString statusText() const;

private:
    [[nodiscard]] Transition enterGaze(GazeLossReason reason);
    [[nodiscard]] Transition enterHead();
    void keepPastBlink(int& ms);

    int m_blinkGraceMs = kDefaultBlinkGraceMs;
    int m_streamStaleMs = kDefaultStreamStaleMs;
    int m_sustainedInvalidMs = kDefaultSustainedInvalidMs;
    int m_gazeAbsentMs = kDefaultGazeAbsentMs;

    bool m_running = false;
    qint64 m_streamStartMs = 0;
    bool m_gazeSeen = false;
    qint64 m_lastGazeMs = 0;
    bool m_hadLiveGaze = false;
    bool m_invalidOpen = false;
    qint64 m_invalidSinceMs = 0;

    bool m_headSeen = false;
    /// Time of the last *valid* head sample. Invalid poses do not refresh it.
    qint64 m_lastHeadMs = 0;
    bool m_hadLiveHead = false;
    bool m_headInvalidOpen = false;
    qint64 m_headInvalidSinceMs = 0;

    bool m_gazeHold = false;
    bool m_headHold = false;
    GazeLossReason m_gazeReason = GazeLossReason::None;
    bool m_resumeLatch = false;
};

} // namespace gazer
