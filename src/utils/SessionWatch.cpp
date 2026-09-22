#include "utils/SessionWatch.h"

#include "utils/Log.h"

#include <QGuiApplication>
#include <QWindow>

#ifdef Q_OS_WIN
#  include <wtsapi32.h>
#endif

namespace gazer {

#ifdef Q_OS_WIN
SessionWatch* SessionWatch::s_self = nullptr;
#endif

SessionWatch::SessionWatch(QObject* parent)
    : QObject(parent)
{
#ifdef Q_OS_WIN
    s_self = this;
    m_probe = new QWindow();
    m_probe->setFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    m_probe->setGeometry(0, 0, 1, 1);
    m_probe->setOpacity(0);
    m_probe->create();
    m_hwnd = reinterpret_cast<HWND>(m_probe->winId());
    if (m_hwnd) {
        WTSRegisterSessionNotification(m_hwnd, NOTIFY_FOR_THIS_SESSION);
    }
    qGuiApp->installNativeEventFilter(this);
    m_hook = SetWinEventHook(EVENT_SYSTEM_DESKTOPSWITCH, EVENT_SYSTEM_DESKTOPSWITCH, nullptr,
                             &SessionWatch::desktopHook, 0, 0,
                             WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
#endif
}

SessionWatch::~SessionWatch()
{
#ifdef Q_OS_WIN
    if (qGuiApp) {
        qGuiApp->removeNativeEventFilter(this);
    }
    if (m_hook) {
        UnhookWinEvent(m_hook);
        m_hook = nullptr;
    }
    if (m_hwnd) {
        WTSUnRegisterSessionNotification(m_hwnd);
        m_hwnd = nullptr;
    }
    delete m_probe;
    m_probe = nullptr;
    if (s_self == this) {
        s_self = nullptr;
    }
#endif
    if (m_paused) {
        m_paused = false;
        emit injectPausedChanged(false);
    }
}

void SessionWatch::setPaused(bool on)
{
    if (m_paused == on) {
        return;
    }
    m_paused = on;
    GAZER_INFO << (on ? "Session locked — inject paused" : "Session unlocked — inject resumed");
    emit injectPausedChanged(on);
}

#ifdef Q_OS_WIN
bool SessionWatch::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
    Q_UNUSED(result);
    if (eventType != QByteArrayLiteral("windows_generic_MSG") &&
        eventType != QByteArrayLiteral("windows_dispatcher_MSG")) {
        return false;
    }
    const MSG* msg = static_cast<MSG*>(message);
    if (!msg || msg->message != WM_WTSSESSION_CHANGE) {
        return false;
    }
    switch (msg->wParam) {
    case WTS_SESSION_LOCK:
    case WTS_CONSOLE_DISCONNECT:
    case WTS_REMOTE_DISCONNECT:
        setPaused(true);
        break;
    case WTS_SESSION_UNLOCK:
    case WTS_CONSOLE_CONNECT:
    case WTS_REMOTE_CONNECT:
        setPaused(false);
        break;
    default:
        break;
    }
    return false;
}

void CALLBACK SessionWatch::desktopHook(HWINEVENTHOOK, DWORD event, HWND, LONG, LONG, DWORD,
                                        DWORD)
{
    if (!s_self || event != EVENT_SYSTEM_DESKTOPSWITCH) {
        return;
    }
    HDESK desk = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    if (!desk) {
        QMetaObject::invokeMethod(s_self, "setPaused", Qt::QueuedConnection, Q_ARG(bool, true));
        return;
    }
    wchar_t name[64]{};
    DWORD needed = 0;
    const BOOL ok = GetUserObjectInformationW(desk, UOI_NAME, name, sizeof(name), &needed);
    CloseDesktop(desk);
    if (!ok) {
        QMetaObject::invokeMethod(s_self, "setPaused", Qt::QueuedConnection, Q_ARG(bool, true));
        return;
    }
    const bool def = _wcsicmp(name, L"Default") == 0;
    QMetaObject::invokeMethod(s_self, "setPaused", Qt::QueuedConnection, Q_ARG(bool, !def));
}
#endif

} // namespace gazer
