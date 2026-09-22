#include "utils/Heartbeat.h"

#include "utils/Log.h"

#include <QByteArray>
#include <QDateTime>
#include <QString>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

#include <string>

namespace gazer {

namespace {
constexpr quint32 kMagic = 0x475A5248u; // 'GZRH'
constexpr quint32 kVersion = 1;
const wchar_t* mapName()
{
    static std::wstring stored;
    const QByteArray env = qgetenv("GAZER_HEARTBEAT_MAP");
    if (!env.isEmpty()) {
        stored = QString::fromUtf8(env).toStdWString();
        return stored.c_str();
    }
    return L"Local\\GazerHeartbeat";
}

struct Block {
    quint32 magic = 0;
    quint32 version = 0;
    quint32 hostPid = 0;
    quint32 flags = 0;
    qint64 guiTickMs = 0;
    qint64 workerTickMs = 0;
};

#ifdef Q_OS_WIN
Block* asBlock(void* view)
{
    return static_cast<Block*>(view);
}
#endif
} // namespace

bool crashLoopTripped(const QVector<qint64>& deathMs, qint64 nowMs, int maxDeaths,
                      qint64 windowMs)
{
    int n = 0;
    for (qint64 t : deathMs) {
        if (nowMs >= t && (nowMs - t) <= windowMs) {
            ++n;
        }
    }
    return n >= maxDeaths;
}

Heartbeat::Heartbeat() = default;

Heartbeat::~Heartbeat()
{
    close();
}

qint64 Heartbeat::nowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

bool Heartbeat::guiStale(const HeartbeatSnapshot& snap, qint64 staleMs)
{
    if (!snap.valid || snap.guiTickMs <= 0) {
        return true;
    }
    return (nowMs() - snap.guiTickMs) > staleMs;
}

bool Heartbeat::openAsHost()
{
#ifdef Q_OS_WIN
    close();
    m_map = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                               sizeof(Block), mapName());
    if (!m_map) {
        GAZER_WARN << "Heartbeat mapping failed" << quint32(GetLastError());
        return false;
    }
    m_view = MapViewOfFile(m_map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Block));
    if (!m_view) {
        CloseHandle(static_cast<HANDLE>(m_map));
        m_map = nullptr;
        return false;
    }
    Block* b = asBlock(m_view);
    ZeroMemory(b, sizeof(Block));
    b->magic = kMagic;
    b->version = kVersion;
    b->hostPid = GetCurrentProcessId();
    b->flags = HeartbeatFlag::HostReady;
    b->guiTickMs = nowMs();
    m_host = true;
    return true;
#else
    return false;
#endif
}

bool Heartbeat::openAsGuard()
{
#ifdef Q_OS_WIN
    close();
    m_map = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, mapName());
    if (!m_map) {
        m_map = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                   sizeof(Block), mapName());
        if (!m_map) {
            return false;
        }
        m_view = MapViewOfFile(m_map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Block));
        if (!m_view) {
            CloseHandle(static_cast<HANDLE>(m_map));
            m_map = nullptr;
            return false;
        }
        Block* b = asBlock(m_view);
        if (b->magic != kMagic) {
            ZeroMemory(b, sizeof(Block));
            b->magic = kMagic;
            b->version = kVersion;
        }
        m_host = false;
        return true;
    }
    m_view = MapViewOfFile(m_map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Block));
    if (!m_view) {
        CloseHandle(static_cast<HANDLE>(m_map));
        m_map = nullptr;
        return false;
    }
    m_host = false;
    return true;
#else
    return false;
#endif
}

void Heartbeat::pulseGui()
{
#ifdef Q_OS_WIN
    if (!m_view) {
        return;
    }
    Block* b = asBlock(m_view);
    b->guiTickMs = nowMs();
    if (m_host) {
        b->hostPid = GetCurrentProcessId();
        b->flags |= HeartbeatFlag::HostReady;
        b->flags &= ~HeartbeatFlag::CleanShutdown;
    }
#else
    Q_UNUSED(this);
#endif
}

void Heartbeat::setCleanShutdown()
{
#ifdef Q_OS_WIN
    if (!m_view) {
        return;
    }
    asBlock(m_view)->flags |= HeartbeatFlag::CleanShutdown;
    asBlock(m_view)->flags &= ~HeartbeatFlag::HostReady;
#endif
}

void Heartbeat::setExclusiveOccluded(bool on)
{
#ifdef Q_OS_WIN
    if (!m_view) {
        return;
    }
    Block* b = asBlock(m_view);
    if (on) {
        b->flags |= HeartbeatFlag::ExclusiveOccluded;
    } else {
        b->flags &= ~HeartbeatFlag::ExclusiveOccluded;
    }
#else
    Q_UNUSED(on);
#endif
}

void Heartbeat::setHostReady(bool on)
{
#ifdef Q_OS_WIN
    if (!m_view) {
        return;
    }
    Block* b = asBlock(m_view);
    if (on) {
        b->flags |= HeartbeatFlag::HostReady;
    } else {
        b->flags &= ~HeartbeatFlag::HostReady;
    }
#else
    Q_UNUSED(on);
#endif
}

HeartbeatSnapshot Heartbeat::read() const
{
    HeartbeatSnapshot s;
#ifdef Q_OS_WIN
    if (!m_view) {
        return s;
    }
    const Block* b = asBlock(m_view);
    if (b->magic != kMagic || b->version != kVersion) {
        return s;
    }
    s.valid = true;
    s.hostPid = b->hostPid;
    s.flags = b->flags;
    s.guiTickMs = b->guiTickMs;
#endif
    return s;
}

void Heartbeat::close()
{
#ifdef Q_OS_WIN
    if (m_view) {
        UnmapViewOfFile(m_view);
        m_view = nullptr;
    }
    if (m_map) {
        CloseHandle(static_cast<HANDLE>(m_map));
        m_map = nullptr;
    }
#endif
    m_host = false;
}

} // namespace gazer
