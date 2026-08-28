#include "utils/WinOverlay.h"

#include <QPointer>
#include <QVector>
#include <QtMath>

#include <algorithm>

namespace gazer {

namespace {
OverlayStackWatch* g_watch = nullptr;
QWindow* g_stackHost = nullptr;
QVector<QPointer<QWindow>> g_overlays;
int g_passDepth = 0;
#ifdef Q_OS_WIN
HWND g_punchedHost = nullptr;
LONG_PTR g_punchedEx = 0;
#endif

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
constexpr UINT kZFlags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE;

void applyExStyle(HWND hwnd, LONG_PTR ex, bool frameChanged)
{
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex);
    UINT flags = kZFlags | SWP_NOZORDER;
    if (frameChanged) {
        flags |= SWP_FRAMECHANGED;
    }
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, flags);
}

bool isOwnZWindow(HWND self, HWND other)
{
    if (!other || other == self) {
        return true;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(other, &pid);
    if (pid == GetCurrentProcessId()) {
        return true;
    }
    for (HWND owner = GetWindow(other, GW_OWNER); owner; owner = GetWindow(owner, GW_OWNER)) {
        if (owner == self) {
            return true;
        }
    }
    return false;
}

bool foreignWindowOccludes(HWND hwnd)
{
    RECT wr{};
    if (!GetWindowRect(hwnd, &wr) || wr.right <= wr.left || wr.bottom <= wr.top) {
        return false;
    }
    for (HWND cur = GetWindow(hwnd, GW_HWNDPREV); cur; cur = GetWindow(cur, GW_HWNDPREV)) {
        if (!IsWindowVisible(cur) || isOwnZWindow(hwnd, cur)) {
            continue;
        }
        RECT orc{};
        if (!GetWindowRect(cur, &orc)) {
            continue;
        }
        RECT hit{};
        if (IntersectRect(&hit, &wr, &orc)) {
            return true;
        }
    }
    return false;
}

void flushHitTestCache()
{
    INPUT move{};
    move.type = INPUT_MOUSE;
    move.mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInput(1, &move, sizeof(INPUT));
}

HWND hwndOf(QWindow* w)
{
    if (!w) {
        return nullptr;
    }
    return reinterpret_cast<HWND>(w->winId());
}

void punchHost()
{
    const HWND hwnd = hwndOf(g_stackHost);
    if (!hwnd || !IsWindowVisible(hwnd)) {
        return;
    }
    const LONG_PTR ex = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if ((ex & WS_EX_TRANSPARENT) != 0) {
        return;
    }
    g_punchedHost = hwnd;
    g_punchedEx = ex;
    applyExStyle(hwnd, ex | WS_EX_TRANSPARENT, /*frameChanged=*/false);
}

void restoreHost()
{
    if (g_punchedHost && IsWindow(g_punchedHost)) {
        applyExStyle(g_punchedHost, g_punchedEx, /*frameChanged=*/false);
    }
    g_punchedHost = nullptr;
    g_punchedEx = 0;
}
#endif
}

void applyOverlayClickThrough(QWindow* w)
{
    if (!w) {
        return;
    }
#ifdef Q_OS_WIN
    const HWND hwnd = hwndOf(w);
    if (!hwnd) {
        return;
    }
    const LONG_PTR ex = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if ((ex & WS_EX_TRANSPARENT) != 0) {
        return;
    }
    applyExStyle(hwnd, ex | WS_EX_TRANSPARENT, /*frameChanged=*/true);
#else
    Q_UNUSED(w);
#endif
}

void applyOverlayClickThrough(QWidget* w)
{
    if (!w) {
        return;
    }
#ifdef Q_OS_WIN
    (void)w->winId();
#endif
    applyOverlayClickThrough(w->windowHandle());
}

QPoint logicalGlobalFromNative(const QWindow* w, QPoint native)
{
#ifdef Q_OS_WIN
    if (!w) {
        return native;
    }
    const HWND hwnd = reinterpret_cast<HWND>(w->winId());
    RECT wr{};
    if (hwnd && GetWindowRect(hwnd, &wr) && wr.right > wr.left && wr.bottom > wr.top) {
        const QRect g = w->geometry();
        if (g.width() > 0 && g.height() > 0) {
            const double fx = double(native.x() - wr.left) / double(wr.right - wr.left);
            const double fy = double(native.y() - wr.top) / double(wr.bottom - wr.top);
            return QPoint(g.x() + qRound(fx * g.width()), g.y() + qRound(fy * g.height()));
        }
    }
    const qreal dpr = w->devicePixelRatio();
    if (dpr > 0.0) {
        return QPoint(qRound(native.x() / dpr), qRound(native.y() / dpr));
    }
#else
    Q_UNUSED(w);
#endif
    return native;
}

OverlayInputPassThrough::OverlayInputPassThrough()
{
#ifdef Q_OS_WIN
    if (g_passDepth++ == 0) {
        punchHost();
        flushHitTestCache();
    }
#endif
}

OverlayInputPassThrough::~OverlayInputPassThrough()
{
#ifdef Q_OS_WIN
    if (g_passDepth > 0) {
        --g_passDepth;
    }
    if (g_passDepth == 0) {
        restoreHost();
        flushHitTestCache();
        if (g_stackHost) {
            raiseInTopmostBand(g_stackHost);
        }
    }
#endif
}

bool OverlayInputPassThrough::active()
{
    return g_passDepth > 0;
}

bool raiseInTopmostBand(QWindow* w)
{
    if (!w) {
        return false;
    }
#ifdef Q_OS_WIN
    const HWND hwnd = hwndOf(w);
    if (!hwnd) {
        return false;
    }

    const LONG_PTR prevEx = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    const bool alreadyTopmost = (prevEx & WS_EX_TOPMOST) != 0;

    LONG_PTR ex = prevEx;
    ex |= WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
    ex &= ~WS_EX_APPWINDOW;
    if (ex != prevEx) {
        applyExStyle(hwnd, ex, /*frameChanged=*/true);
    }

    if (!alreadyTopmost) {
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, kZFlags);
        return true;
    }
    if (!foreignWindowOccludes(hwnd)) {
        return false;
    }
    // Stay in the TOPMOST band. HWND_TOPMOST is a no-op when already topmost;
    // HWND_NOTOPMOST would flash whatever is under a full-screen board.
    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, kZFlags);
    return true;
#else
    Q_UNUSED(w);
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
