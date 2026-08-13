#pragma once

#include <QWidget>
#include <QWindow>

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

/// Restack a topmost overlay above the Windows taskbar (same TOPMOST band).
/// Explorer often restacks Shell_TrayWnd after a show; call again on a short delay.
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

    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);
#else
    Q_UNUSED(w);
#endif
}

} // namespace gazer
