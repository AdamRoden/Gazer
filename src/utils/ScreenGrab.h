#pragma once

#include <QGuiApplication>
#include <QPainter>
#include <QPixmap>
#include <QRect>
#include <QScreen>

namespace gazer {

[[nodiscard]] inline QRect virtualDesktop()
{
    QRect u;
    for (QScreen* s : QGuiApplication::screens()) {
        if (s) {
            u = u.united(s->geometry());
        }
    }
    if (u.isEmpty()) {
        if (QScreen* s = QGuiApplication::primaryScreen()) {
            u = s->geometry();
        }
    }
    return u;
}

/// Primary display, same rect Tobii and PageSession use for board placement.
[[nodiscard]] inline QRect overlayScreenGeometry()
{
    if (QScreen* s = QGuiApplication::primaryScreen()) {
        return s->geometry();
    }
    return virtualDesktop();
}

[[nodiscard]] inline QRect overlayDesktopGeometry()
{
    if (QScreen* s = QGuiApplication::primaryScreen()) {
        return s->availableGeometry();
    }
    return overlayScreenGeometry();
}

/// Logical-pixel screenshot of `globalRect` on `screen`. Off-screen pixels stay `fill`.
inline QPixmap grabScreenRect(QScreen* screen, const QRect& globalRect,
                              const QColor& fill = Qt::transparent)
{
    if (!screen || globalRect.isEmpty()) {
        return {};
    }

    const QRect screenGeo = screen->geometry();
    const QRect visible = globalRect.intersected(screenGeo);
    QPixmap canvas(globalRect.size());
    canvas.fill(fill);
    canvas.setDevicePixelRatio(1.0);
    if (visible.isEmpty()) {
        return canvas;
    }

    const QRect srcLocal = visible.translated(-screenGeo.topLeft());
    QPixmap piece =
        screen->grabWindow(0, srcLocal.x(), srcLocal.y(), srcLocal.width(), srcLocal.height());
    if (piece.isNull()) {
        return canvas;
    }
    if (piece.size() != visible.size()) {
        piece = piece.scaled(visible.size(), Qt::IgnoreAspectRatio, Qt::FastTransformation);
    }
    if (visible == globalRect) {
        piece.setDevicePixelRatio(1.0);
        return piece;
    }
    QPainter p(&canvas);
    p.drawPixmap(QRect(visible.topLeft() - globalRect.topLeft(), visible.size()), piece);
    p.end();
    return canvas;
}

} // namespace gazer
