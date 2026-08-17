#pragma once

#include <QGuiApplication>
#include <QPainter>
#include <QPixmap>
#include <QRect>
#include <QScreen>

namespace gazer {

/// Map a screen-local logical rect onto a grabWindow pixmap (handles DPI scale).
inline QRect mapLogicalRectToPixmap(const QRect& logical, const QSize& pixmapSize,
                                    const QSize& logicalSize)
{
    if (logical.isEmpty() || pixmapSize.isEmpty() || logicalSize.isEmpty()) {
        return {};
    }
    if (pixmapSize == logicalSize) {
        return logical;
    }
    const qreal sx = qreal(pixmapSize.width()) / qreal(logicalSize.width());
    const qreal sy = qreal(pixmapSize.height()) / qreal(logicalSize.height());
    return QRect(qRound(logical.x() * sx), qRound(logical.y() * sy),
                 qMax(1, qRound(logical.width() * sx)), qMax(1, qRound(logical.height() * sy)));
}

/// Logical-pixel screenshot of `globalRect` on `screen`. Off-screen pixels stay `fill`.
inline QPixmap grabScreenRect(QScreen* screen, const QRect& globalRect,
                              const QColor& fill = Qt::transparent)
{
    if (!screen || globalRect.isEmpty()) {
        return {};
    }

    const QPixmap desk = screen->grabWindow(0);
    QPixmap canvas(globalRect.size());
    canvas.fill(fill);
    if (desk.isNull()) {
        canvas.setDevicePixelRatio(1.0);
        return canvas;
    }

    const QRect screenGeo = screen->geometry();
    const QRect visible = globalRect.intersected(screenGeo);
    if (visible.isEmpty()) {
        canvas.setDevicePixelRatio(1.0);
        return canvas;
    }

    const QRect srcLocal = visible.translated(-screenGeo.topLeft());
    const QRect srcInPixmap = mapLogicalRectToPixmap(srcLocal, desk.size(), screenGeo.size());
    const QPixmap piece = desk.copy(srcInPixmap);
    QPainter p(&canvas);
    p.drawPixmap(QRect(visible.topLeft() - globalRect.topLeft(), visible.size()), piece);
    p.end();
    canvas.setDevicePixelRatio(1.0);
    return canvas;
}

} // namespace gazer
