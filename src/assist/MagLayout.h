#pragma once

#include <QPoint>
#include <QRect>
#include <QScreen>
#include <QtMath>

namespace gazer {

/// Where to capture and where to put the static zoom window.
struct MagPresentation {
    QPoint srcCenter;
    QPoint destCenter;
    int destSide = 0; ///< Square window edge. Must be set by the caller.
    double zoom = 2.0;
};

inline QPoint clampToRect(const QPoint& p, const QRect& r)
{
    return {qBound(r.left(), p.x(), r.right()), qBound(r.top(), p.y(), r.bottom())};
}

inline QRect slideOnto(QRect r, const QRect& bounds)
{
    if (r.width() > bounds.width()) {
        r.setWidth(bounds.width());
    }
    if (r.height() > bounds.height()) {
        r.setHeight(bounds.height());
    }
    r.moveLeft(qBound(bounds.left(), r.left(), bounds.right() - r.width() + 1));
    r.moveTop(qBound(bounds.top(), r.top(), bounds.bottom() - r.height() + 1));
    return r;
}

/// Fixed dest size; capture is dest/zoom, slid fully on-screen (never shrinks the box).
inline bool layoutMagWindow(QScreen* screen, const MagPresentation& spec, QRect* srcOut,
                            QRect* destOut)
{
    if (!screen || !srcOut || !destOut) {
        return false;
    }
    const QRect screenGeom = screen->geometry();
    if (screenGeom.width() < 40 || screenGeom.height() < 40 || spec.destSide < 40) {
        return false;
    }

    const int destSide =
        qBound(40, spec.destSide, qMin(screenGeom.width(), screenGeom.height()));
    const double zoom = qMax(1.25, spec.zoom);
    const QPoint destC = clampToRect(spec.destCenter, screenGeom);
    const QPoint srcC = clampToRect(spec.srcCenter, screenGeom);

    QRect dest(destC.x() - destSide / 2, destC.y() - destSide / 2, destSide, destSide);
    dest = slideOnto(dest, screenGeom);

    const int srcSide = qMax(40, qRound(double(destSide) / zoom));
    QRect src(srcC.x() - srcSide / 2, srcC.y() - srcSide / 2, srcSide, srcSide);
    src = slideOnto(src, screenGeom);

    *srcOut = src;
    *destOut = dest;
    return src.width() >= 40 && src.height() >= 40;
}

} // namespace gazer
