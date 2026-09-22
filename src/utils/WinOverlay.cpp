#include "utils/WinOverlay.h"

#include "input/InjectGate.h"
#include "utils/WinProcess.h"

#include <QPointer>
#include <QSet>
#include <QString>
#include <QVector>
#include <QtMath>

#include <algorithm>

namespace gazer {

namespace {
OverlayStackWatch* g_watch = nullptr;
QWindow* g_stackHost = nullptr;
int g_passDepth = 0;
bool g_restacking = false;
ScreenCaptureMode g_captureMode = ScreenCaptureMode::Pages;
OccluderKind g_occluder = OccluderKind::None;

struct OverlayEntry {
    QPointer<QWindow> window;
    OverlayLayer layer = OverlayLayer::Assist;
};

QVector<OverlayEntry> g_overlays;
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
                                    [](const OverlayEntry& e) { return e.window.isNull(); }),
                     g_overlays.end());
}

#ifdef Q_OS_WIN
constexpr UINT kZFlags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER;

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
    if (pid == GetCurrentProcessId() || WinProcess::isGazerImage(pid)) {
        return true;
    }
    for (HWND owner = GetWindow(other, GW_OWNER); owner; owner = GetWindow(owner, GW_OWNER)) {
        if (owner == self) {
            return true;
        }
    }
    return false;
}

HWND firstForeignOccluder(HWND hwnd)
{
    RECT wr{};
    if (!GetWindowRect(hwnd, &wr) || wr.right <= wr.left || wr.bottom <= wr.top) {
        return nullptr;
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
            return cur;
        }
    }
    return nullptr;
}

bool coversMonitor(HWND hwnd)
{
    RECT wr{};
    if (!GetWindowRect(hwnd, &wr)) {
        return false;
    }
    HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(mon, &mi)) {
        return false;
    }
    const RECT& m = mi.rcMonitor;
    const int slop = 4;
    return wr.left <= m.left + slop && wr.top <= m.top + slop && wr.right >= m.right - slop
           && wr.bottom >= m.bottom - slop;
}

QString windowImageBase(HWND hwnd)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    return WinProcess::imageBase(pid);
}

OccluderKind classifyHwnd(HWND hwnd)
{
    if (!hwnd) {
        return OccluderKind::None;
    }
    wchar_t cls[256]{};
    GetClassNameW(hwnd, cls, 256);
    const QString image = windowImageBase(hwnd);
    if (_wcsicmp(cls, L"TaskManagerWindow") == 0
        || image.compare(QLatin1String("Taskmgr.exe"), Qt::CaseInsensitive) == 0) {
        return OccluderKind::TaskManager;
    }
    const LONG style = GetWindowLongW(hwnd, GWL_STYLE);
    const bool noCaption = (style & WS_CAPTION) == 0;
    if (coversMonitor(hwnd) && noCaption && GetForegroundWindow() == hwnd) {
        return OccluderKind::ExclusiveFullscreen;
    }
    const LONG ex = GetWindowLongW(hwnd, GWL_EXSTYLE);
    if (ex & WS_EX_TOPMOST) {
        return OccluderKind::TopmostForeign;
    }
    return OccluderKind::Other;
}

bool foreignWindowOccludes(HWND hwnd)
{
    return firstForeignOccluder(hwnd) != nullptr;
}

void flushHitTestCache()
{
    INPUT move{};
    move.type = INPUT_MOUSE;
    move.mi.dwFlags = MOUSEEVENTF_MOVE;
    (void)InjectGate::send(&move, 1);
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
        restackGazerBand();
    }
#endif
}

bool OverlayInputPassThrough::active()
{
    return g_passDepth > 0;
}

bool processHasUiAccess()
{
#ifdef Q_OS_WIN
    HANDLE tok = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) {
        return false;
    }
    DWORD uiAccess = 0;
    DWORD retLen = 0;
    const BOOL ok = GetTokenInformation(tok, TokenUIAccess, &uiAccess, sizeof(uiAccess), &retLen);
    CloseHandle(tok);
    return ok && uiAccess != 0;
#else
    return false;
#endif
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

#ifdef Q_OS_WIN
void ensureTopmostStyle(HWND hwnd)
{
    if (!hwnd) {
        return;
    }
    const LONG_PTR prevEx = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    LONG_PTR ex = prevEx;
    ex |= WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
    ex &= ~WS_EX_APPWINDOW;
    if (ex != prevEx) {
        applyExStyle(hwnd, ex, /*frameChanged=*/false);
    }
}

QVector<HWND> gazerBandBackToFront()
{
    QVector<HWND> out;
    auto push = [&](QWindow* w) {
        if (!w || !w->isVisible()) {
            return;
        }
        const HWND hwnd = hwndOf(w);
        if (hwnd && IsWindowVisible(hwnd)) {
            out.push_back(hwnd);
        }
    };
    push(g_stackHost);
    auto appendLayer = [&](OverlayLayer layer) {
        for (const OverlayEntry& e : g_overlays) {
            if (e.layer == layer) {
                push(e.window.data());
            }
        }
    };
    appendLayer(OverlayLayer::Assist);
    appendLayer(OverlayLayer::MagPick);
    appendLayer(OverlayLayer::Magnifier);
    appendLayer(OverlayLayer::Reticle);
    return out;
}

QVector<HWND> actualGazerFrontToBack(const QSet<HWND>& gazer)
{
    QVector<HWND> out;
    const HWND desktop = GetDesktopWindow();
    for (HWND h = GetWindow(desktop, GW_CHILD); h; h = GetWindow(h, GW_HWNDNEXT)) {
        if (!IsWindowVisible(h) || !gazer.contains(h)) {
            continue;
        }
        out.push_back(h);
    }
    return out;
}

bool gazerOrderMatches(const QVector<HWND>& backToFront)
{
    QSet<HWND> gazer;
    QVector<HWND> expectedFrontToBack;
    expectedFrontToBack.reserve(backToFront.size());
    for (int i = backToFront.size() - 1; i >= 0; --i) {
        expectedFrontToBack.push_back(backToFront.at(i));
        gazer.insert(backToFront.at(i));
    }
    return actualGazerFrontToBack(gazer) == expectedFrontToBack;
}

bool gazerBandNeedsRestack()
{
    const QVector<HWND> band = gazerBandBackToFront();
    if (band.isEmpty()) {
        return false;
    }
    for (HWND hwnd : band) {
        if (foreignWindowOccludes(hwnd)) {
            return true;
        }
    }
    return !gazerOrderMatches(band);
}

void applyGazerBandOrder()
{
    const QVector<HWND> band = gazerBandBackToFront();
    if (band.isEmpty()) {
        return;
    }
    for (HWND hwnd : band) {
        ensureTopmostStyle(hwnd);
    }
    // HWND_TOPMOST is a no-op among windows that already have WS_EX_TOPMOST.
    // HWND_TOP reorders inside the TOPMOST band without dropping the bit.
    // One DeferWindowPos so the host does not paint over mag-pick / overlays
    // between sequential raises. Last hwnd is the front of the Gazer band.
    HDWP hdwp = BeginDeferWindowPos(band.size());
    if (hdwp) {
        for (HWND hwnd : band) {
            hdwp = DeferWindowPos(hdwp, hwnd, HWND_TOP, 0, 0, 0, 0, kZFlags);
            if (!hdwp) {
                break;
            }
        }
        if (hdwp) {
            EndDeferWindowPos(hdwp);
            return;
        }
    }
    for (HWND hwnd : band) {
        SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, kZFlags);
    }
}
#endif

void refreshOccluderCache()
{
#ifdef Q_OS_WIN
    const QVector<HWND> band = gazerBandBackToFront();
    OccluderKind worst = OccluderKind::None;
    for (HWND hwnd : band) {
        const OccluderKind k = classifyHwnd(firstForeignOccluder(hwnd));
        if (k == OccluderKind::ExclusiveFullscreen) {
            g_occluder = k;
            return;
        }
        if (int(k) > int(worst)) {
            worst = k;
        }
    }
    g_occluder = worst;
#else
    g_occluder = OccluderKind::None;
#endif
}

void restackGazerBand()
{
    if (g_restacking) {
        return;
    }
    pruneOverlays();
    refreshOccluderCache();
#ifdef Q_OS_WIN
    if (!gazerBandNeedsRestack()) {
        return;
    }
    g_restacking = true;
    applyGazerBandOrder();
    g_restacking = false;
#else
    if (g_stackHost) {
        raiseInTopmostBand(g_stackHost);
    }
    for (const OverlayEntry& e : g_overlays) {
        raiseInTopmostBand(e.window.data());
    }
#endif
}

OccluderKind gazerBandOccluderKind()
{
    return g_occluder;
}

bool gazerBandExclusiveOccluded()
{
    return g_occluder == OccluderKind::ExclusiveFullscreen;
}

void setOverlayStackHost(QWindow* host)
{
    g_stackHost = host;
    pruneOverlays();
    for (const OverlayEntry& e : g_overlays) {
        attachOne(e.window.data());
    }
}

void registerOverlayWindow(QWindow* overlay, OverlayLayer layer)
{
    if (!overlay) {
        return;
    }
    pruneOverlays();
    for (OverlayEntry& e : g_overlays) {
        if (e.window.data() == overlay) {
            e.layer = layer;
            attachOne(overlay);
            return;
        }
    }
    OverlayEntry e;
    e.window = overlay;
    e.layer = layer;
    g_overlays.append(e);
    attachOne(overlay);
}

void unregisterOverlayWindow(QWindow* overlay)
{
    g_overlays.erase(std::remove_if(g_overlays.begin(), g_overlays.end(),
                                    [overlay](const OverlayEntry& e) {
                                        return e.window.isNull() || e.window.data() == overlay;
                                    }),
                     g_overlays.end());
}

void setScreenCaptureMode(ScreenCaptureMode mode)
{
    g_captureMode = mode;
}

void applyWindowCaptureAffinity(QWindow* w, bool overlay)
{
#ifdef Q_OS_WIN
    if (!w || !w->handle()) {
        return;
    }
    const HWND hwnd = reinterpret_cast<HWND>(w->winId());
    if (!hwnd) {
        return;
    }
    const bool exclude = excludeFromScreenCapture(g_captureMode, overlay);
    SetWindowDisplayAffinity(hwnd, exclude ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
#else
    Q_UNUSED(w);
    Q_UNUSED(overlay);
#endif
}

void applyScreenCapturePolicy()
{
    pruneOverlays();
    applyWindowCaptureAffinity(g_stackHost, /*overlay=*/false);
    for (const OverlayEntry& e : g_overlays) {
        applyWindowCaptureAffinity(e.window.data(), /*overlay=*/true);
    }
}

OverlayStackWatch::OverlayStackWatch(std::function<void()> restack, QObject* parent)
    : QObject(parent)
    , m_restack(std::move(restack))
{
    auto fire = [this]() {
        if (m_restack) {
            m_restack();
        } else {
            restackGazerBand();
        }
    };
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(180);
    QObject::connect(&m_debounce, &QTimer::timeout, this, fire);
    m_poll.setInterval(300);
    QObject::connect(&m_poll, &QTimer::timeout, this, fire);
    m_poll.start();
    g_watch = this;
#ifdef Q_OS_WIN
    // Foreground, movesize, minimize, switch — Explorer restacks Shell_TrayWnd
    // after these. Exclusive-fullscreen still wins (OS) until it yields; we
    // reassert TOPMOST. Task Manager needs UIAccess (processHasUiAccess).
    m_hook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_MINIMIZEEND, nullptr,
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
