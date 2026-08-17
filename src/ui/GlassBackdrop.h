#pragma once

#include <QColor>
#include <QObject>
#include <QPixmap>
#include <QRectF>
#include <QTimer>

class QPainter;
class QWindow;

namespace gazer {

/// Screen-behind frost for style.blur fills. Owns capture, blur, and refresh.
/// Paint only draws the last rebuilt pixmap plus the authored tint.
class GlassBackdrop final : public QObject {
    Q_OBJECT

public:
    explicit GlassBackdrop(QWindow* host);

    /// 0 disables capture. Non-zero is the blur radius used for the shared frost.
    void setActive(double maxBlurRadius);
    void paint(QPainter& p, const QRectF& localRect, double cornerRadius, const QColor& tint) const;

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
    QPixmap m_frosted;
};

} // namespace gazer
