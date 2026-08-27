#pragma once

#include "layout/PageBox.h"

#include <QColor>
#include <QObject>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QRectF>
#include <QTimer>

class QPainter;
class QWindow;

namespace gazer {

/// Screen-behind frost for style.blur fills. Owns capture, blur, and refresh.
class GlassBackdrop final : public QObject {
    Q_OBJECT

public:
    explicit GlassBackdrop(QWindow* host);

    void setActive(double maxBlurRadius);
    /// Global capture rect (frosted chrome union). Empty disables capture.
    void setCaptureRect(const QRect& globalRect);
    /// Opaque overlay chrome in window-local pixels, plus the window's global origin.
    void setUnderlay(QPixmap windowLocal, QPoint windowOrigin);
    /// Grab the capture rect immediately so frost exists before motion.
    void captureNow();
    /// Cancel debounce / periodic recapture. Host uses this while chrome is moving.
    void stopRefresh();
    void paint(QPainter& p, const QRectF& localRect, const PageBox& radii, const QColor& tint) const;

signals:
    void updated();

private slots:
    void invalidate();
    void rebuild();

private:
    QWindow* m_host = nullptr;
    QTimer m_timer;
    double m_radius = 0.0;
    int m_pad = 0;
    QRect m_capture;
    QPixmap m_frosted;
    qint64 m_frostKey = 0;
    QPixmap m_underlay;
    QPoint m_underlayOrigin;
};

} // namespace gazer
