#pragma once

#include <QObject>
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

/// Temporarily exclude `w` from screen capture; restores the previous affinity.
class CaptureExclusion {
public:
    explicit CaptureExclusion(QWindow* w)
    {
#ifdef Q_OS_WIN
        if (!w) {
            return;
        }
        m_hwnd = reinterpret_cast<HWND>(w->winId());
        if (!m_hwnd) {
            return;
        }
        if (!GetWindowDisplayAffinity(m_hwnd, &m_prev)) {
            m_prev = WDA_NONE;
        }
        SetWindowDisplayAffinity(m_hwnd, WDA_EXCLUDEFROMCAPTURE);
#else
        Q_UNUSED(w);
#endif
    }

    ~CaptureExclusion()
    {
#ifdef Q_OS_WIN
        if (m_hwnd) {
            SetWindowDisplayAffinity(m_hwnd, m_prev);
        }
#endif
    }

    CaptureExclusion(const CaptureExclusion&) = delete;
    CaptureExclusion& operator=(const CaptureExclusion&) = delete;

private:
#ifdef Q_OS_WIN
    HWND m_hwnd = nullptr;
    DWORD m_prev = WDA_NONE;
#endif
};

/// Restack a topmost overlay above the Windows taskbar (same TOPMOST band).
/// Already-topmost windows ignore a second HWND_TOPMOST, so this drops out of
/// the band and re-enters it. Explorer restacks Shell_TrayWnd after a show or
/// app activate; call again on a short delay.
inline void raiseAboveTaskbar(QWindow* w)
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
    ex |= WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW;
    ex &= ~WS_EX_APPWINDOW;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex);

    constexpr UINT flags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED;
    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, flags);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, flags);
#else
    Q_UNUSED(w);
#endif
}

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
