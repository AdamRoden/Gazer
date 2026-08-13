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

    /// Restack this overlay on top of the TOPMOST band. Does not emit stackChanged.
    void raiseStack()
    {
        applyOverlayWindowChrome(this, /*excludeFromCapture=*/true);
        raise();
        if (QWindow* wh = windowHandle()) {
            raiseAboveTaskbar(wh);
        }
    }

    void showOverlay()
    {
        if (!isVisible()) {
            show();
        }
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
        applyOverlayWindowChrome(this, /*excludeFromCapture=*/true);
    }

    void hideEvent(QHideEvent* event) override
    {
        QWidget::hideEvent(event);
        emit stackChanged();
    }
};

} // namespace gazer
