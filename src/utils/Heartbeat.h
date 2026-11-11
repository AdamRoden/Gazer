#pragma once

#include <QVector>
#include <QtGlobal>

namespace gazer {

/// Shared-memory liveness block between the host and `Gazer.exe --guard`.
struct HeartbeatSnapshot {
    quint32 hostPid = 0;
    quint32 flags = 0;
    qint64 guiTickMs = 0;
    qint32 gazeX = 0;
    qint32 gazeY = 0;
    bool gazeValid = false;
    qint64 gazeTickMs = 0;
    bool valid = false;
};

namespace HeartbeatFlag {
constexpr quint32 CleanShutdown = 1u;
constexpr quint32 ExclusiveOccluded = 2u;
constexpr quint32 HostReady = 4u;
/// Host asked the guard to start a new copy, then exited. Not a crash.
constexpr quint32 Relaunch = 8u;
constexpr quint32 RestartSafe = 16u;
}

/// 3 deaths inside 2 minutes → stop auto-relaunch (crash-loop).
[[nodiscard]] bool crashLoopTripped(const QVector<qint64>& deathMs, qint64 nowMs,
                                    int maxDeaths = 3, qint64 windowMs = 120000);

class Heartbeat {
public:
    Heartbeat();
    ~Heartbeat();

    Heartbeat(const Heartbeat&) = delete;
    Heartbeat& operator=(const Heartbeat&) = delete;

    /// Create or open `Local\GazerHeartbeat` and claim the host pid.
    bool openAsHost();
    /// Open existing mapping (guard). False if nobody created it yet.
    bool openAsGuard();

    void pulseGui();
    /// Worker-safe. Writes the latest gaze into the host mapping. No-op unless
    /// this process has an open host block. `gazeTickMs` is stored last.
    static void publishGaze(int x, int y, bool valid);
    void setCleanShutdown();
    /// Ask the guard to spawn a new host. A later pulse does not clear this.
    /// False when this process has no heartbeat view.
    [[nodiscard]] bool requestRelaunch(bool safe);
    void clearRelaunch();
    void setExclusiveOccluded(bool on);
    void setHostReady(bool on);

    [[nodiscard]] HeartbeatSnapshot read() const;
    [[nodiscard]] static qint64 nowMs();
    [[nodiscard]] static bool guiStale(const HeartbeatSnapshot& snap, qint64 staleMs = 3000);
    /// True when the snapshot carries a gaze sample newer than @p maxAgeMs.
    [[nodiscard]] static bool gazeFresh(const HeartbeatSnapshot& snap, qint64 maxAgeMs = 450);

    void close();

private:
    void* m_map = nullptr;
    void* m_view = nullptr;
    bool m_host = false;
};

} // namespace gazer
