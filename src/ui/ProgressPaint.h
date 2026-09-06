#pragma once

#include "ui/ProgressVisuals.h"
#include "layout/RoundBox.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QtMath>

namespace gazer {

inline constexpr qreal kRadialStrokePx = 10.0;
inline constexpr qreal kRadialMarginPx = 6.0;
inline constexpr qreal kRadialShortSide = 0.9;
inline constexpr qreal kRadialStrokeFrac = 0.10;

enum class ProgressShape { RoundedRect, Ellipse };

struct RadialRing {
    QRectF arc;
    QRectF disk;
    qreal stroke = kRadialStrokePx;
};

/// Stroke width: 10% of the short side, clamped 4–10 px.
[[nodiscard]] inline qreal radialStrokePx(qreal shortSide)
{
    return qBound(4.0, shortSide * kRadialStrokeFrac, kRadialStrokePx);
}

/// `disk` is the outer visual circle (pie). `arc` is the stroke centerline.
/// `kRadialShortSide` is that outer diameter, then clamped to a `kRadialMarginPx` inset.
[[nodiscard]] inline RadialRing radialRingGeom(const QRectF& r, ProgressShape shape)
{
    RadialRing g;
    const double shortSide = qMin(r.width(), r.height());
    g.stroke = radialStrokePx(shortSide);
    if (shape == ProgressShape::RoundedRect) {
        const QPointF c = r.center();
        const double maxOuter = shortSide - 2.0 * kRadialMarginPx;
        const double outer = qMax(g.stroke + 8.0, qMin(shortSide * kRadialShortSide, maxOuter));
        const double side = qMax(8.0, outer - g.stroke);
        g.disk = QRectF(c.x() - outer / 2.0, c.y() - outer / 2.0, outer, outer);
        g.arc = QRectF(c.x() - side / 2.0, c.y() - side / 2.0, side, side);
    } else {
        const double inset = g.stroke * 0.5;
        g.disk = r;
        g.arc = r.adjusted(inset, inset, -inset, -inset);
    }
    return g;
}

/// Unfilled ring: a fraction of the pie fill alpha so the track stays lighter.
[[nodiscard]] inline QColor radialTrackColor(const QColor& pieFill)
{
    QColor track = pieFill.isValid() ? pieFill : ThemeColors::defaultProgressColor();
    const int pieA = track.alpha();
    track.setAlpha(qBound(0, qRound(pieA * 80.0 / 255.0), 255));
    return track;
}

/// Cap a stroke at the pie fill alpha so an opaque border/ring does not
/// overpower a translucent pie.
[[nodiscard]] inline QColor progressStrokeColor(const QColor& stroke, const QColor& pieFill)
{
    QColor c = stroke.isValid() ? stroke : pieFill;
    if (!c.isValid()) {
        return c;
    }
    const int pieA = pieFill.isValid() ? pieFill.alpha() : 255;
    if (c.alpha() > pieA) {
        c.setAlpha(pieA);
    }
    return c;
}

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
    const RadialRing ring = (v.style.pie || v.style.radial) ? radialRingGeom(r, shape) : RadialRing{};
    if (v.style.pie) {
        p.save();
        p.setClipPath(shapePath);
        p.setPen(Qt::NoPen);
        p.setBrush(v.progressColor);
        p.drawPie(ring.disk, 90 * 16, int(-360 * 16 * progress));
        p.restore();
    }
    if (v.style.border) {
        p.setBrush(Qt::NoBrush);
        const qreal w = 2.0 + 2.0 * progress;
        p.setPen(QPen(progressStrokeColor(v.borderColor, v.progressColor), w, Qt::SolidLine,
                      Qt::FlatCap, Qt::RoundJoin));
        if (shape == ProgressShape::Ellipse) {
            p.drawEllipse(r.adjusted(2, 2, -2, -2));
        } else {
            const double h = w / 2.0;
            p.drawPath(roundedBoxPath(r.adjusted(h, h, -h, -h), radii));
        }
    }
    if (v.style.radial) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(radialTrackColor(v.progressColor), ring.stroke, Qt::SolidLine, Qt::FlatCap,
                      Qt::RoundJoin));
        p.drawEllipse(ring.arc);
        p.setPen(QPen(v.progressColor, ring.stroke, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
        p.drawArc(ring.arc, 90 * 16, int(-360 * 16 * progress));
    }
}

inline void paintProgress(QPainter& p, const QRectF& r, double progress, const ProgressVisuals& v,
                          ProgressShape shape, double radius = 0.0)
{
    paintProgress(p, r, progress, v, shape, PageBox::all(radius));
}

} // namespace gazer
