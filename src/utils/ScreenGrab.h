#pragma once

#include <QGuiApplication>
#include <QPainter>
#include <QPixmap>
#include <QRect>
#include <QScreen>
#include <QVector>
#include <QtMath>

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

/// Work area in screen-local pixels (origin at the overlay screen's top-left).
[[nodiscard]] inline QRect overlayDesktopLocal()
{
    const QRect screen = overlayScreenGeometry();
    QRect desk = overlayDesktopGeometry().translated(-screen.topLeft());
    const QRect local(QPoint(0, 0), screen.size());
    desk = desk.intersected(local);
    return desk.isEmpty() ? local : desk;
}

/// Reserved strips (taskbar and other shell chrome) in @p screen minus @p work.
[[nodiscard]] inline QVector<QRect> reservedStrips(const QRect& screen, const QRect& work)
{
    QVector<QRect> out;
    const QRect w = work.intersected(screen);
    if (w.isEmpty() || w == screen) {
        return out;
    }
    if (w.y() > screen.y()) {
        out.push_back(QRect(screen.x(), screen.y(), screen.width(), w.y() - screen.y()));
    }
    const int workBottom = w.y() + w.height();
    const int screenBottom = screen.y() + screen.height();
    if (workBottom < screenBottom) {
        out.push_back(QRect(screen.x(), workBottom, screen.width(), screenBottom - workBottom));
    }
    if (w.x() > screen.x()) {
        out.push_back(QRect(screen.x(), w.y(), w.x() - screen.x(), w.height()));
    }
    const int workRight = w.x() + w.width();
    const int screenRight = screen.x() + screen.width();
    if (workRight < screenRight) {
        out.push_back(QRect(workRight, w.y(), screenRight - workRight, w.height()));
    }
    return out;
}

/// Cheap frost: downscale then upscale. Radius < 1 returns @p src.
[[nodiscard]] inline QPixmap downscaleBlur(const QPixmap& src, double radius)
{
    if (src.isNull() || radius < 1.0) {
        return src;
    }
    const int factor = qBound(2, qRound(radius / 2.0), 12);
    const QSize small(qMax(1, src.width() / factor), qMax(1, src.height() / factor));
    return src.scaled(small, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
        .scaled(src.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
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
