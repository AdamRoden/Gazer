#include "utils/WinOverlay.h"

#include <QPointer>
#include <QVector>

#include <algorithm>

#ifdef Q_OS_WIN
#  include <cwchar>
#endif

namespace gazer {

namespace {
OverlayStackWatch* g_watch = nullptr;
QWindow* g_stackHost = nullptr;
QVector<QPointer<QWindow>> g_overlays;

void attachOne(QWindow* overlay)
{
    if (!overlay || !g_stackHost || overlay == g_stackHost) {
        return;
    }
    if (overlay->transientParent() != g_stackHost) {
        overlay->setTransientParent(g_stackHost);
    }
}

void pruneOverlays()
{
    g_overlays.erase(std::remove_if(g_overlays.begin(), g_overlays.end(),
                                    [](const QPointer<QWindow>& o) { return o.isNull(); }),
                     g_overlays.end());
}

#ifdef Q_OS_WIN
bool isTaskbarHwnd(HWND hwnd)
{
    wchar_t cls[64] = {};
    if (GetClassNameW(hwnd, cls, 64) <= 0) {
        return false;
    }
    return wcscmp(cls, L"Shell_TrayWnd") == 0
           || wcscmp(cls, L"Shell_SecondaryTrayWnd") == 0;
}

bool taskbarOccludes(HWND hwnd)
{
    RECT wr{};
    if (!GetWindowRect(hwnd, &wr)) {
        return false;
    }
    for (HWND cur = GetWindow(hwnd, GW_HWNDPREV); cur; cur = GetWindow(cur, GW_HWNDPREV)) {
        if (!IsWindowVisible(cur) || !isTaskbarHwnd(cur)) {
            continue;
        }
        RECT tr{};
        if (!GetWindowRect(cur, &tr)) {
            continue;
        }
        RECT hit{};
        if (IntersectRect(&hit, &wr, &tr)) {
            return true;
        }
    }
    return false;
}

bool hasVisibleWindowAbove(HWND hwnd)
{
    for (HWND cur = GetWindow(hwnd, GW_HWNDPREV); cur; cur = GetWindow(cur, GW_HWNDPREV)) {
        if (IsWindowVisible(cur)) {
            return true;
        }
    }
    return false;
}
#endif
}

bool raiseAboveTaskbar(QWindow* w, bool onlyIfTaskbarOccludes)
{
    if (!w) {
        return false;
    }
#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(w->winId());
    if (!hwnd) {
        return false;
    }

    const bool alreadyTopmost =
        (GetWindowLongPtr(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;

    LONG_PTR ex = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    ex |= WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW;
    ex &= ~WS_EX_APPWINDOW;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex);

    constexpr UINT flags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED;
    if (!alreadyTopmost) {
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, flags);
        return true;
    }
    if (onlyIfTaskbarOccludes) {
        if (!taskbarOccludes(hwnd)) {
            return false;
        }
    } else if (!hasVisibleWindowAbove(hwnd)) {
        return false;
    }
    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, flags);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, flags);
    return true;
#else
    Q_UNUSED(w);
    Q_UNUSED(onlyIfTaskbarOccludes);
    return false;
#endif
}

void setOverlayStackHost(QWindow* host)
{
    g_stackHost = host;
    pruneOverlays();
    for (const QPointer<QWindow>& o : g_overlays) {
        attachOne(o);
    }
}

void registerOverlayWindow(QWindow* overlay)
{
    if (!overlay) {
        return;
    }
    pruneOverlays();
    bool found = false;
    for (const QPointer<QWindow>& o : g_overlays) {
        if (o.data() == overlay) {
            found = true;
            break;
        }
    }
    if (!found) {
        g_overlays.append(overlay);
    }
    attachOne(overlay);
}

void unregisterOverlayWindow(QWindow* overlay)
{
    g_overlays.erase(std::remove_if(g_overlays.begin(), g_overlays.end(),
                                    [overlay](const QPointer<QWindow>& o) {
                                        return o.isNull() || o.data() == overlay;
                                    }),
                     g_overlays.end());
}

OverlayStackWatch::OverlayStackWatch(std::function<void()> restack, QObject* parent)
    : QObject(parent)
    , m_restack(std::move(restack))
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(180);
    QObject::connect(&m_debounce, &QTimer::timeout, this, [this]() {
        if (m_restack) {
            m_restack();
        }
    });
    g_watch = this;
#ifdef Q_OS_WIN
    m_hook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr,
                             &OverlayStackWatch::hookProc, 0, 0,
                             WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
#endif
}

OverlayStackWatch::~OverlayStackWatch()
{
#ifdef Q_OS_WIN
    if (m_hook) {
        UnhookWinEvent(m_hook);
        m_hook = nullptr;
    }
#endif
    if (g_watch == this) {
        g_watch = nullptr;
    }
}

#ifdef Q_OS_WIN
void CALLBACK OverlayStackWatch::hookProc(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD)
{
    if (g_watch) {
        g_watch->m_debounce.start();
    }
}
#endif

} // namespace gazer
