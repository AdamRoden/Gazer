#pragma once

#include "utils/WinOverlay.h"

#include <QHideEvent>
#include <QWidget>

namespace gazer {

/// Shared chrome for always-on-top, non-activating tool overlays.
class OverlaySurface : public QWidget {
    Q_OBJECT

public:
    explicit OverlaySurface(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool
                       | Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_QuitOnClose, false);
    }

    ~OverlaySurface() override
    {
        if (QWindow* wh = windowHandle()) {
            unregisterOverlayWindow(wh);
        }
    }

    /// Restack this overlay on top of the TOPMOST band. Does not emit stackChanged.
    /// Always hops if another topmost window is above us (small HWND, unlike the
    /// board which only hops when the taskbar occludes it). Owned by the board
    /// host so a board hop keeps us above it.
    void raiseStack()
    {
        bindStackHost();
        applyOverlayWindowChrome(this, /*excludeFromCapture=*/true);
        raise();
        if (QWindow* wh = windowHandle()) {
            raiseAboveTaskbar(wh);
        }
    }

    /// Show + raise once. Already-visible overlays stay put; call raiseStack to restack.
    void showOverlay()
    {
        if (isVisible()) {
            return;
        }
        show();
        raiseStack();
        emit stackChanged();
    }

signals:
    /// Overlay HWND shown, raised, or hidden — chrome boards may need a restack.
    void stackChanged();

protected:
    void showEvent(QShowEvent* event) override
    {
        QWidget::showEvent(event);
        bindStackHost();
        applyOverlayWindowChrome(this, /*excludeFromCapture=*/true);
    }

    void hideEvent(QHideEvent* event) override
    {
        QWidget::hideEvent(event);
        emit stackChanged();
    }

private:
    void bindStackHost()
    {
        (void)winId();
        if (QWindow* wh = windowHandle()) {
            registerOverlayWindow(wh);
        }
    }
};

} // namespace gazer
