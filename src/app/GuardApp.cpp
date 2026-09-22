#include "app/GuardApp.h"

#include "app/ActionChannel.h"
#include "input/KeyboardInjector.h"
#include "ui/RescueOverlay.h"
#include "utils/CrashDump.h"
#include "utils/Heartbeat.h"
#include "utils/Log.h"
#include "utils/WinProcess.h"

#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QProcess>
#include <QTimer>
#include <QVector>
#include <QWindow>
#include <functional>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

namespace gazer {

namespace {
constexpr auto kGuardMutex = L"Local\\GazerGuard";
constexpr int kPollMs = 500;
constexpr int kHungMs = 3000;
constexpr int kYieldHoldMs = 8000;
constexpr int kSpawnWaitMs = 8000;
constexpr int kHotkeyId = 1;

class GuardHotkey final : public QAbstractNativeEventFilter {
public:
    explicit GuardHotkey(HWND hwnd, std::function<void()> onPause)
        : m_hwnd(hwnd)
        , m_onPause(std::move(onPause))
    {
#ifdef Q_OS_WIN
        if (m_hwnd) {
            RegisterHotKey(m_hwnd, kHotkeyId, 0, VK_PAUSE);
        }
#endif
    }
    ~GuardHotkey() override
    {
#ifdef Q_OS_WIN
        if (m_hwnd) {
            UnregisterHotKey(m_hwnd, kHotkeyId);
        }
#endif
    }
    bool nativeEventFilter(const QByteArray& type, void* message, qintptr*) override
    {
#ifdef Q_OS_WIN
        if (type != QByteArrayLiteral("windows_generic_MSG")
            && type != QByteArrayLiteral("windows_dispatcher_MSG")) {
            return false;
        }
        const MSG* msg = static_cast<MSG*>(message);
        if (msg && msg->message == WM_HOTKEY && int(msg->wParam) == kHotkeyId) {
            if (m_onPause) {
                m_onPause();
            }
            return true;
        }
#else
        Q_UNUSED(type);
        Q_UNUSED(message);
#endif
        return false;
    }

private:
    HWND m_hwnd = nullptr;
    std::function<void()> m_onPause;
};

class GuardHost final : public QObject {
public:
    enum class State { Watch, WaitSpawn, CrashLoop };

    explicit GuardHost(QObject* parent = nullptr)
        : QObject(parent)
    {
        m_overlay = new RescueOverlay();
        connect(m_overlay, &RescueOverlay::restartHost, this, [this]() { spawnHost(false); });
        connect(m_overlay, &RescueOverlay::restartSafe, this, [this]() { spawnHost(true); });
        connect(m_overlay, &RescueOverlay::quitGuard, this, []() { QApplication::quit(); });
        connect(m_overlay, &RescueOverlay::yieldDesktop, this, [this]() { injectShowDesktop(); });
        m_beat.openAsGuard();
        m_poll.setInterval(kPollMs);
        connect(&m_poll, &QTimer::timeout, this, [this]() { tick(); });
        m_poll.start();
    }

    ~GuardHost() override
    {
        delete m_overlay;
        m_overlay = nullptr;
    }

    Heartbeat m_beat;

    void ensureHost()
    {
        const HeartbeatSnapshot snap = m_beat.read();
        if (snap.valid && WinProcess::alive(snap.hostPid)) {
            m_state = State::Watch;
            return;
        }
        spawnHost(false);
    }

    void onPauseHotkey()
    {
        const HeartbeatSnapshot snap = m_beat.read();
        const bool alive = snap.valid && WinProcess::alive(snap.hostPid);
        if (alive && !Heartbeat::guiStale(snap, kHungMs)) {
            QString err;
            if (ActionChannel::sendToPeer(QStringLiteral("command=rescue.reset"), &err)
                == PeerResult::Ok) {
                return;
            }
        }
        if (alive) {
            onHung(snap.hostPid);
            return;
        }
        enterCrashLoop();
    }

private:
    void tick()
    {
        if (m_state == State::CrashLoop) {
            return;
        }

        const HeartbeatSnapshot snap = m_beat.read();
        const quint32 pid = snap.valid ? snap.hostPid : 0;
        const bool alive = pid != 0 && WinProcess::alive(pid);
        const bool clean = snap.valid && (snap.flags & HeartbeatFlag::CleanShutdown);

        if (alive) {
            m_state = State::Watch;
            if (Heartbeat::guiStale(snap, kHungMs)) {
                onHung(pid);
                return;
            }
            updateYield(snap.flags & HeartbeatFlag::ExclusiveOccluded);
            return;
        }

        m_exclusiveSince.invalidate();
        if (m_overlay->mode() == RescueOverlay::Mode::Yield) {
            m_overlay->setMode(RescueOverlay::Mode::Hidden);
        }

        if (clean) {
            GAZER_INFO << "Guard: host quit cleanly";
            QApplication::quit();
            return;
        }

        if (m_state == State::WaitSpawn && m_spawnAt.isValid()
            && m_spawnAt.elapsed() < kSpawnWaitMs) {
            return;
        }

        noteDeath(pid);
        if (crashLoopTripped(m_deaths, Heartbeat::nowMs())) {
            enterCrashLoop();
            return;
        }
        spawnHost(false);
    }

    void onHung(quint32 pid)
    {
        QString err;
        if (!WinProcess::terminate(pid, &err)) {
            GAZER_WARN << "Guard terminate:" << err;
        }
        noteDeath(pid);
        if (crashLoopTripped(m_deaths, Heartbeat::nowMs())) {
            enterCrashLoop();
            return;
        }
        spawnHost(false);
    }

    void spawnHost(bool safe)
    {
        m_overlay->setMode(RescueOverlay::Mode::Hidden);
        m_state = State::WaitSpawn;
        m_spawnAt.start();
        const QString exe = QCoreApplication::applicationFilePath();
        QStringList args;
        if (safe) {
            args << QStringLiteral("--safe");
        }
        if (!QProcess::startDetached(exe, args, QCoreApplication::applicationDirPath())) {
            GAZER_WARN << "Guard: failed to start host";
            enterCrashLoop();
        }
    }

    void enterCrashLoop()
    {
        m_state = State::CrashLoop;
        m_overlay->setMode(RescueOverlay::Mode::CrashLoop);
    }

    void noteDeath(quint32 pid)
    {
        if (pid == 0 || pid == m_lastDeadPid) {
            return;
        }
        m_deaths.push_back(Heartbeat::nowMs());
        m_lastDeadPid = pid;
    }

    void updateYield(bool exclusive)
    {
        if (!exclusive) {
            m_exclusiveSince.invalidate();
            if (m_overlay->mode() == RescueOverlay::Mode::Yield) {
                m_overlay->setMode(RescueOverlay::Mode::Hidden);
            }
            return;
        }
        if (!m_exclusiveSince.isValid()) {
            m_exclusiveSince.start();
        }
        if (m_exclusiveSince.elapsed() >= kYieldHoldMs) {
            m_overlay->setMode(RescueOverlay::Mode::Yield);
        }
    }

    void injectShowDesktop()
    {
        QString ignored;
        (void)KeyboardInjector::keyDown(QStringLiteral("LWin"), &ignored);
        (void)KeyboardInjector::keyDown(QStringLiteral("D"), &ignored);
        (void)KeyboardInjector::keyUp(QStringLiteral("D"), &ignored);
        (void)KeyboardInjector::keyUp(QStringLiteral("LWin"), &ignored);
        m_overlay->setMode(RescueOverlay::Mode::Hidden);
    }

    RescueOverlay* m_overlay = nullptr;
    QTimer m_poll;
    QElapsedTimer m_exclusiveSince;
    QElapsedTimer m_spawnAt;
    QVector<qint64> m_deaths;
    quint32 m_lastDeadPid = 0;
    State m_state = State::Watch;
};

} // namespace

bool spawnGuardDetached()
{
#ifdef Q_OS_WIN
    HANDLE existing = OpenMutexW(SYNCHRONIZE, FALSE, kGuardMutex);
    if (existing) {
        CloseHandle(existing);
        return true;
    }
#endif
    const QString exe = QCoreApplication::applicationFilePath();
    return QProcess::startDetached(exe, {QStringLiteral("--guard")},
                                   QCoreApplication::applicationDirPath());
}

int runGuard(int argc, char** argv)
{
#ifdef Q_OS_WIN
    HANDLE mutex = CreateMutexW(nullptr, TRUE, kGuardMutex);
    if (!mutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (mutex) {
            CloseHandle(mutex);
        }
        return 0;
    }
#else
    HANDLE mutex = nullptr;
    Q_UNUSED(mutex);
#endif

    QApplication qapp(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);
    QApplication::setApplicationName(QStringLiteral("Gazer"));
    QApplication::setOrganizationName(QString());

    GAZER_INFO << "Guard starting";
    CrashDump::installHandlers();

    GuardHost host;
    QWindow probe;
    probe.setFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    probe.setGeometry(0, 0, 1, 1);
    probe.create();
#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(probe.winId());
#else
    const HWND hwnd = nullptr;
#endif
    GuardHotkey hot(hwnd, [&host]() { host.onPauseHotkey(); });
    qapp.installNativeEventFilter(&hot);
    host.ensureHost();

    const int rc = qapp.exec();
#ifdef Q_OS_WIN
    ReleaseMutex(mutex);
    CloseHandle(mutex);
#endif
    return rc;
}

} // namespace gazer
