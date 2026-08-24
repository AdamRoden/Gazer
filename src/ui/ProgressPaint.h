#pragma once

#include "ui/ProgressVisuals.h"
#include "layout/RoundBox.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QtMath>

namespace gazer {

inline constexpr qreal kRadialStrokePx = 6.0;
inline constexpr qreal kRadialMarginPx = 6.0;
inline constexpr qreal kRadialShortSide = 0.9;

enum class ProgressShape { RoundedRect, Ellipse };

[[nodiscard]] inline QRectF progressFillSlice(const QRectF& r, double t, ProgressFillDir dir)
{
    t = qBound(0.0, t, 1.0);
    switch (dir) {
    case ProgressFillDir::Up:
        return QRectF(r.left(), r.bottom() - r.height() * t, r.width(), r.height() * t);
    case ProgressFillDir::Down:
        return QRectF(r.left(), r.top(), r.width(), r.height() * t);
    case ProgressFillDir::Right:
        return QRectF(r.left(), r.top(), r.width() * t, r.height());
    case ProgressFillDir::Left:
        return QRectF(r.right() - r.width() * t, r.top(), r.width() * t, r.height());
    case ProgressFillDir::Center:
    case ProgressFillDir::None:
        break;
    }
    const QPointF c = r.center();
    const double hw = r.width() * 0.5 * t;
    const double hh = r.height() * 0.5 * t;
    return QRectF(c.x() - hw, c.y() - hh, hw * 2.0, hh * 2.0);
}

inline void paintProgress(QPainter& p, const QRectF& r, double progress, const ProgressVisuals& v,
                          ProgressShape shape, const PageBox& radii)
{
    if (progress <= 0.0 || r.isEmpty()) {
        return;
    }
    const QPointF c = r.center();
    QPainterPath shapePath;
    if (shape == ProgressShape::Ellipse) {
        shapePath.addEllipse(r);
    } else {
        shapePath = roundedBoxPath(r, radii);
    }
    if (v.style.fillBackground) {
        QColor fill = v.fillColor;
        fill.setAlpha(qBound(0, int(fill.alpha() * progress + 20 * progress), 255));
        p.save();
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        if (shape == ProgressShape::Ellipse && v.style.fillDir == ProgressFillDir::Center) {
            p.drawEllipse(c, r.width() * 0.5 * progress, r.height() * 0.5 * progress);
        } else {
            p.setClipPath(shapePath);
            p.drawRect(progressFillSlice(r, progress, v.style.fillDir));
        }
        p.restore();
    }
    if (v.style.border) {
        p.setBrush(Qt::NoBrush);
        const qreal w = 2.0 + 2.0 * progress;
        p.setPen(QPen(v.borderColor, w, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
        if (shape == ProgressShape::Ellipse) {
            p.drawEllipse(r.adjusted(2, 2, -2, -2));
        } else {
            const double h = w / 2.0;
            p.drawPath(roundedBoxPath(r.adjusted(h, h, -h, -h), radii));
        }
    }
    if (v.style.radial) {
        const qreal penW = kRadialStrokePx;
        QRectF arc = r;
        if (shape == ProgressShape::RoundedRect) {
            const double shortSide = qMin(r.width(), r.height());
            const double maxOuter = shortSide - 2.0 * kRadialMarginPx;
            const double side = qMax(8.0, qMin(shortSide * kRadialShortSide, maxOuter - penW));
            arc = QRectF(c.x() - side / 2.0, c.y() - side / 2.0, side, side);
        } else {
            const double inset = penW * 0.5;
            arc = r.adjusted(inset, inset, -inset, -inset);
        }
        p.setBrush(Qt::NoBrush);
        QColor track = v.progressColor;
        track.setAlpha(80);
        p.setPen(QPen(track, penW, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
        p.drawEllipse(arc);
        p.setPen(QPen(v.progressColor, penW, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
        p.drawArc(arc, 90 * 16, int(-360 * 16 * progress));
    }
}

inline void paintProgress(QPainter& p, const QRectF& r, double progress, const ProgressVisuals& v,
                          ProgressShape shape, double radius = 0.0)
{
    paintProgress(p, r, progress, v, shape, PageBox::all(radius));
}

} // namespace gazer
