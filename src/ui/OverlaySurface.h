#pragma once

#include "utils/WinOverlay.h"

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

    void setOverlayLayer(OverlayLayer layer)
    {
        m_layer = layer;
        if (QWindow* wh = windowHandle()) {
            registerOverlayWindow(wh, m_layer);
        }
    }

    ~OverlaySurface() override
    {
        if (QWindow* wh = windowHandle()) {
            unregisterOverlayWindow(wh);
        }
    }

    /// Show once. Already-visible overlays stay put (no z-order hop / flash).
    void showOverlay()
    {
        if (isVisible()) {
            return;
        }
        show();
        restackGazerBand();
    }

protected:
    void showEvent(QShowEvent* event) override
    {
        QWidget::showEvent(event);
        applyToolChrome();
    }

private:
    void applyToolChrome()
    {
        (void)winId();
        if (QWindow* wh = windowHandle()) {
            registerOverlayWindow(wh, m_layer);
        }
        applyOverlayWindowChrome(this, /*overlay=*/true);
        applyOverlayClickThrough(this);
    }

    OverlayLayer m_layer = OverlayLayer::Assist;
};

} // namespace gazer
