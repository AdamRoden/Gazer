#pragma once

#include "layout/PageBox.h"

#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QtMath>

namespace gazer {

/// Circular corners that fit the box. A single end can be a semicircle
/// (radius = the full opposite side). Adjacent pairs scale if they overflow.
[[nodiscard]] inline PageBox fitCornerRadii(const QRectF& r, const PageBox& radii)
{
    const double w = r.width();
    const double h = r.height();
    double tl = qMax(0.0, radii.at(0));
    double tr = qMax(0.0, radii.at(1));
    double br = qMax(0.0, radii.at(2));
    double bl = qMax(0.0, radii.at(3));
    if (tl + tr > w) {
        const double s = w / qMax(1.0, tl + tr);
        tl *= s;
        tr *= s;
    }
    if (bl + br > w) {
        const double s = w / qMax(1.0, bl + br);
        bl *= s;
        br *= s;
    }
    if (tl + bl > h) {
        const double s = h / qMax(1.0, tl + bl);
        tl *= s;
        bl *= s;
    }
    if (tr + br > h) {
        const double s = h / qMax(1.0, tr + br);
        tr *= s;
        br *= s;
    }
    return PageBox::of(tl, tr, br, bl);
}

[[nodiscard]] inline QPainterPath roundedBoxPath(const QRectF& r, const PageBox& radii)
{
    const PageBox fit = fitCornerRadii(r, radii);
    const double tl = fit.at(0);
    const double tr = fit.at(1);
    const double br = fit.at(2);
    const double bl = fit.at(3);
    QPainterPath path;
    path.moveTo(r.left() + tl, r.top());
    path.lineTo(r.right() - tr, r.top());
    if (tr > 0.0) {
        path.arcTo(QRectF(r.right() - 2.0 * tr, r.top(), 2.0 * tr, 2.0 * tr), 90, -90);
    } else {
        path.lineTo(r.right(), r.top());
    }
    path.lineTo(r.right(), r.bottom() - br);
    if (br > 0.0) {
        path.arcTo(QRectF(r.right() - 2.0 * br, r.bottom() - 2.0 * br, 2.0 * br, 2.0 * br), 0, -90);
    } else {
        path.lineTo(r.right(), r.bottom());
    }
    path.lineTo(r.left() + bl, r.bottom());
    if (bl > 0.0) {
        path.arcTo(QRectF(r.left(), r.bottom() - 2.0 * bl, 2.0 * bl, 2.0 * bl), 270, -90);
    } else {
        path.lineTo(r.left(), r.bottom());
    }
    path.lineTo(r.left(), r.top() + tl);
    if (tl > 0.0) {
        path.arcTo(QRectF(r.left(), r.top(), 2.0 * tl, 2.0 * tl), 180, -90);
    } else {
        path.lineTo(r.left(), r.top());
    }
    path.closeSubpath();
    return path;
}

[[nodiscard]] inline bool roundedBoxContains(const QRectF& r, const PageBox& radii,
                                             const QPointF& pos)
{
    if (!r.contains(pos)) {
        return false;
    }
    if (!radii.isSet()
        || (radii.at(0) <= 0.0 && radii.at(1) <= 0.0 && radii.at(2) <= 0.0 && radii.at(3) <= 0.0)) {
        return true;
    }
    return roundedBoxPath(r, radii).contains(pos);
}

} // namespace gazer
