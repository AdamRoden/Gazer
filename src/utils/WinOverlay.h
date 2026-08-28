#pragma once

#include <QObject>
#include <QPoint>
#include <QTimer>
#include <QWidget>
#include <QWindow>

#include <functional>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  ifndef WDA_EXCLUDEFROMCAPTURE
#    define WDA_EXCLUDEFROMCAPTURE 0x00000011
#  endif
#  ifndef WDA_NONE
#    define WDA_NONE 0x00000000
#  endif
#endif

namespace gazer {

/// Windows overlay hygiene for Gazer tool windows / boards.
///
/// IMPORTANT: Do NOT force WS_EX_LAYERED. Setting LAYERED without
/// SetLayeredWindowAttributes makes opaque Qt widgets invisible.
///
/// @param excludeFromCapture  true for tool overlays (magnifier self-exclude);
///                            false for AAC boards so magnifier can show keys.
inline void applyOverlayWindowChrome(QWindow* w, bool excludeFromCapture = true)
{
    if (!w) {
        return;
    }
#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(w->winId());
    if (!hwnd) {
        return;
    }

    LONG_PTR ex = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    ex |= WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW;
    ex &= ~WS_EX_APPWINDOW;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex);

    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    if (excludeFromCapture) {
        SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
    } else {
        SetWindowDisplayAffinity(hwnd, WDA_NONE);
    }
#else
    Q_UNUSED(w);
    Q_UNUSED(excludeFromCapture);
#endif
}

inline void applyOverlayWindowChrome(QWidget* w, bool excludeFromCapture = true)
{
    if (!w) {
        return;
    }
#ifdef Q_OS_WIN
    // winId() creates the HWND; windowHandle() is then valid.
    (void)w->winId();
#endif
    applyOverlayWindowChrome(w->windowHandle(), excludeFromCapture);
}

/// WS_EX_TRANSPARENT so hit-testing (and SendInput) skips this HWND.
/// Tool overlays always want this; the board does not (WM_NCHITTEST hits cells).
void applyOverlayClickThrough(QWindow* w);
void applyOverlayClickThrough(QWidget* w);

/// Map a WM_NCHITTEST lParam (physical virtual-desktop pixels) to Qt logical
/// global coordinates. Falls back to devicePixelRatio if GetWindowRect fails.
QPoint logicalGlobalFromNative(const QWindow* w, QPoint native);

/// While alive, the board host is click-through so SendInput button/wheel
/// reaches the OS. Tool overlays are permanently click-through. Nested scopes
/// share one punch. The board is not hidden and does not leave the TOPMOST band.
class OverlayInputPassThrough final {
public:
    OverlayInputPassThrough();
    ~OverlayInputPassThrough();

    OverlayInputPassThrough(const OverlayInputPassThrough&) = delete;
    OverlayInputPassThrough& operator=(const OverlayInputPassThrough&) = delete;
    OverlayInputPassThrough(OverlayInputPassThrough&&) = delete;
    OverlayInputPassThrough& operator=(OverlayInputPassThrough&&) = delete;

    [[nodiscard]] static bool active();
};

/// Front of the TOPMOST band via HWND_TOP. Never HWND_NOTOPMOST (that hop
/// flashes the desktop). Same-process / owned windows are ignored. Returns
/// true if z-order changed.
bool raiseInTopmostBand(QWindow* w);

/// Tool overlays are Win32-owned by this host so they stay above it.
/// Call when the host HWND is created or recreated.
void setOverlayStackHost(QWindow* host);

/// Parent @p overlay to the stack host (no-op until a host is set).
void registerOverlayWindow(QWindow* overlay);
void unregisterOverlayWindow(QWindow* overlay);

/// Debounced restack when another process takes the foreground. Explorer then
/// restacks Shell_TrayWnd onto the top of the TOPMOST band.
class OverlayStackWatch final : public QObject {
public:
    explicit OverlayStackWatch(std::function<void()> restack, QObject* parent = nullptr);
    ~OverlayStackWatch() override;

    OverlayStackWatch(const OverlayStackWatch&) = delete;
    OverlayStackWatch& operator=(const OverlayStackWatch&) = delete;

private:
#ifdef Q_OS_WIN
    static void CALLBACK hookProc(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD);
    HWINEVENTHOOK m_hook = nullptr;
#endif
    std::function<void()> m_restack;
    QTimer m_debounce;
};

} // namespace gazer
