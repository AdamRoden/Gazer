#include "utils/Heartbeat.h"

#include "utils/Log.h"

#include <QByteArray>
#include <QDateTime>
#include <QString>

#include <atomic>
#include <cstddef>

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
constexpr quint32 kVersion = 2;
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
    qint32 gazeX = 0;
    qint32 gazeY = 0;
    quint32 gazeValid = 0;
    quint32 gazePad = 0;
    qint64 gazeTickMs = 0;
};

static_assert(offsetof(Block, gazeTickMs) % 8 == 0, "gazeTickMs must be 8-byte aligned");
static_assert(std::atomic_ref<qint64>::is_always_lock_free, "qint64 atomic_ref");

std::atomic<Block*> g_hostBlock{nullptr};

qint64 loadTick(const qint64& src)
{
    return std::atomic_ref<qint64>(const_cast<qint64&>(src)).load(std::memory_order_acquire);
}

void storeTick(qint64& dst, qint64 value)
{
    std::atomic_ref<qint64>(dst).store(value, std::memory_order_release);
}

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

bool Heartbeat::gazeFresh(const HeartbeatSnapshot& snap, qint64 maxAgeMs)
{
    if (!snap.valid || !snap.gazeValid || snap.gazeTickMs <= 0) {
        return false;
    }
    const qint64 age = nowMs() - snap.gazeTickMs;
    return age >= 0 && age <= maxAgeMs;
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
    g_hostBlock.store(b, std::memory_order_release);
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
    // A pulse after quit must not clear CleanShutdown or look alive to the guard.
    if (b->flags & HeartbeatFlag::CleanShutdown) {
        return;
    }
    b->guiTickMs = nowMs();
    if (m_host) {
        b->hostPid = GetCurrentProcessId();
        b->flags |= HeartbeatFlag::HostReady;
    }
#else
    Q_UNUSED(this);
#endif
}

void Heartbeat::publishGaze(int x, int y, bool valid)
{
#ifdef Q_OS_WIN
    Block* b = g_hostBlock.load(std::memory_order_acquire);
    if (!b || b->magic != kMagic) {
        return;
    }
    b->gazeX = qint32(x);
    b->gazeY = qint32(y);
    b->gazeValid = valid ? 1u : 0u;
    std::atomic_thread_fence(std::memory_order_release);
    storeTick(b->gazeTickMs, nowMs());
#else
    Q_UNUSED(x);
    Q_UNUSED(y);
    Q_UNUSED(valid);
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
    const qint64 tick1 = loadTick(b->gazeTickMs);
    std::atomic_thread_fence(std::memory_order_acquire);
    s.gazeX = b->gazeX;
    s.gazeY = b->gazeY;
    s.gazeValid = b->gazeValid != 0;
    s.gazeTickMs = tick1;
    const qint64 tick2 = loadTick(b->gazeTickMs);
    if (tick1 != tick2) {
        s.gazeValid = false;
        s.gazeTickMs = 0;
    }
#endif
    return s;
}

void Heartbeat::close()
{
#ifdef Q_OS_WIN
    if (m_host && m_view) {
        Block* cur = g_hostBlock.load(std::memory_order_acquire);
        if (cur == asBlock(m_view)) {
            g_hostBlock.store(nullptr, std::memory_order_release);
        }
    }
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
