#pragma once

#include <QVector>
#include <QtGlobal>

namespace gazer {

/// Shared-memory liveness block between the host and `Gazer.exe --guard`.
struct HeartbeatSnapshot {
    quint32 hostPid = 0;
    quint32 flags = 0;
    qint64 guiTickMs = 0;
    bool valid = false;
};

namespace HeartbeatFlag {
constexpr quint32 CleanShutdown = 1u;
constexpr quint32 ExclusiveOccluded = 2u;
constexpr quint32 HostReady = 4u;
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
    void setCleanShutdown();
    void setExclusiveOccluded(bool on);
    void setHostReady(bool on);

    [[nodiscard]] HeartbeatSnapshot read() const;
    [[nodiscard]] static qint64 nowMs();
    [[nodiscard]] static bool guiStale(const HeartbeatSnapshot& snap, qint64 staleMs = 3000);

    void close();

private:
    void* m_map = nullptr;
    void* m_view = nullptr;
    bool m_host = false;
};

} // namespace gazer
