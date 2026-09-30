#include "app/GuardApp.h"

#include "app/ActionChannel.h"
#include "core/GazePoint.h"
#include "core/ITracker.h"
#include "core/TrackerMouse.h"
#include "core/TrackerTobii.h"
#include "input/KeyboardInjector.h"
#include "ui/RescueOverlay.h"
#include "utils/CrashDump.h"
#include "utils/Heartbeat.h"
#include "utils/Log.h"
#include "utils/WinProcess.h"

#include <cmath>
#include <memory>

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
        m_aim.setInterval(50);
        connect(&m_aim, &QTimer::timeout, this, [this]() { refreshAim(); });
        m_aim.start();
    }

    ~GuardHost() override
    {
        stopRescueTracker();
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
            m_awaitingClaim = false;
            stopRescueTracker();
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

        if (m_awaitingClaim) {
            if (pid == m_pidAtSpawn) {
                m_deaths.push_back(Heartbeat::nowMs());
                if (pid != 0) {
                    m_lastDeadPid = pid;
                }
            } else {
                noteDeath(pid);
            }
            m_awaitingClaim = false;
        } else {
            noteDeath(pid);
        }
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
        stopRescueTracker();
        m_overlay->setMode(RescueOverlay::Mode::Hidden);
        const HeartbeatSnapshot snap = m_beat.read();
        m_pidAtSpawn = snap.valid ? snap.hostPid : 0;
        m_awaitingClaim = true;
        m_state = State::WaitSpawn;
        m_spawnAt.start();
        const QString exe = QCoreApplication::applicationFilePath();
        QStringList args;
        if (safe) {
            args << QStringLiteral("--safe");
        }
        if (!QProcess::startDetached(exe, args, QCoreApplication::applicationDirPath())) {
            GAZER_WARN << "Guard: failed to start host";
            m_awaitingClaim = false;
            enterCrashLoop();
        }
    }

    void enterCrashLoop()
    {
        m_state = State::CrashLoop;
        m_overlay->setMode(RescueOverlay::Mode::CrashLoop);
        startRescueTracker();
    }

    void refreshAim()
    {
        if (!m_overlay || m_overlay->mode() == RescueOverlay::Mode::Hidden) {
            return;
        }
        if (m_rescueTracker && m_rescueTracker->isRunning()) {
            return;
        }
        const HeartbeatSnapshot snap = m_beat.read();
        if (Heartbeat::gazeFresh(snap)) {
            m_overlay->setAimPoint(QPoint(snap.gazeX, snap.gazeY), true);
        } else {
            m_overlay->setAimPoint(QPoint(), false);
        }
    }

    void wireRescueAim(ITracker* tracker)
    {
        if (!tracker) {
            return;
        }
        connect(tracker, &ITracker::gazeUpdated, this, [this](const GazePoint& gp) {
            if (!m_overlay) {
                return;
            }
            m_overlay->setAimPoint(QPoint(int(std::lround(gp.x)), int(std::lround(gp.y))), gp.valid);
        });
        if (auto* tobii = qobject_cast<TrackerTobii*>(tracker)) {
            connect(tobii, &TrackerTobii::streamFailed, this, [this](const QString& reason) {
                GAZER_WARN << "Guard tracker:" << reason;
                if (m_state != State::CrashLoop) {
                    return;
                }
                releaseTracker(m_rescueTracker);
                auto mouse = std::make_unique<TrackerMouse>();
                if (!mouse->start()) {
                    return;
                }
                m_rescueTracker = std::move(mouse);
                wireRescueAim(m_rescueTracker.get());
            });
        }
    }

    void startRescueTracker()
    {
        if (m_rescueTracker) {
            return;
        }
        // openEyeTracker returns as soon as the Tobii worker is launched.
        // Setup failure arrives later on streamFailed and falls back to the mouse.
        m_rescueTracker = openEyeTracker();
        wireRescueAim(m_rescueTracker.get());
    }

    void stopRescueTracker()
    {
        releaseTracker(m_rescueTracker);
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
    std::unique_ptr<ITracker> m_rescueTracker;
    QTimer m_poll;
    QTimer m_aim;
    QElapsedTimer m_exclusiveSince;
    QElapsedTimer m_spawnAt;
    QVector<qint64> m_deaths;
    quint32 m_lastDeadPid = 0;
    quint32 m_pidAtSpawn = 0;
    bool m_awaitingClaim = false;
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
    CrashDump::capCrashDir();

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
