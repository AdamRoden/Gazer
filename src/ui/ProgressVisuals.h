#pragma once

#include "layout/LayoutTypes.h"
#include "ui/Theme.h"

#include <QColor>
#include <QPainter>
#include <QPen>
#include <QString>

namespace gazer {

/// How dwell progress is drawn on activators (and mouse-move reticle).
struct ProgressVisuals {
    bool radial = true;
    bool fillBackground = false;
    bool border = false;

    QColor progressColor = ThemeColors::defaultProgressColor();
    QColor fillColor = QColor(0, 180, 220, 70);
    QColor borderColor = ThemeColors::defaultProgressColor();

    bool flashUseForeground = true;
    int flashForegroundOpacity = 60;
    QColor flashColor = QColor(255, 255, 255);
    int flashMs = 140;

    [[nodiscard]] QColor resolvedFlashColor(const QColor& itemForeground) const
    {
        if (!flashUseForeground) {
            return flashColor;
        }
        QColor c = itemForeground.isValid() ? itemForeground : QColor(255, 255, 255);
        c.setAlpha(qBound(0, qRound(255.0 * double(flashForegroundOpacity) / 100.0), 255));
        return c;
    }

    [[nodiscard]] ProgressVisuals withItemFlash(const QColor& itemForeground) const
    {
        ProgressVisuals v = *this;
        v.flashColor = v.resolvedFlashColor(itemForeground);
        return v;
    }

    void setStylesFromCsv(const QString& csv)
    {
        radial = false;
        fillBackground = false;
        border = false;
        const QStringList parts = csv.toLower().split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (QString p : parts) {
            p = p.trimmed();
            if (p == QLatin1String("radial") || p == QLatin1String("ring")) {
                radial = true;
            } else if (p == QLatin1String("fill") || p == QLatin1String("background")) {
                fillBackground = true;
            } else if (p == QLatin1String("border") || p == QLatin1String("outline")) {
                border = true;
            }
        }
        if (!radial && !fillBackground && !border) {
            radial = true;
        }
    }

    [[nodiscard]] QString stylesCsv() const
    {
        QStringList p;
        if (radial) {
            p << QStringLiteral("radial");
        }
        if (fillBackground) {
            p << QStringLiteral("fill");
        }
        if (border) {
            p << QStringLiteral("border");
        }
        return p.join(QLatin1Char(','));
    }

    /// Merge layout/item dwell overrides onto a base (usually AppSettings-derived).
    [[nodiscard]] ProgressVisuals mergedWith(const LayoutDwellConfig& ovr) const
    {
        ProgressVisuals v = *this;
        if (ovr.hasProgressStyle) {
            v.setStylesFromCsv(ovr.progressStyle);
        }
        auto applyColor = [](QColor& dest, const QString& hex) {
            if (hex.isEmpty()) {
                return;
            }
            dest = ThemeColors::parseColor(hex, dest);
        };
        if (ovr.hasProgressColor) {
            applyColor(v.progressColor, ovr.progressColor);
        }
        if (ovr.hasFillColor) {
            applyColor(v.fillColor, ovr.fillColor);
        }
        if (ovr.hasBorderColor) {
            applyColor(v.borderColor, ovr.borderColor);
        }
        if (ovr.hasFlashColor) {
            applyColor(v.flashColor, ovr.flashColor);
            v.flashUseForeground = false;
        }
        if (ovr.hasFlashMs && ovr.flashMs > 0) {
            v.flashMs = ovr.flashMs;
        }
        return v;
    }
};

enum class ProgressShape { RoundedRect, Ellipse };

inline void paintProgress(QPainter& p, const QRectF& r, double progress, const ProgressVisuals& v,
                          ProgressShape shape, double radius = 0.0)
{
    if (progress <= 0.0) {
        return;
    }
    const QPointF c = r.center();
    if (v.fillBackground) {
        QColor fill = v.fillColor;
        fill.setAlpha(qBound(0, int(fill.alpha() * progress + 20 * progress), 255));
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        if (shape == ProgressShape::Ellipse) {
            p.drawEllipse(c, r.width() * 0.5 * progress, r.height() * 0.5 * progress);
        } else {
            const double hw = r.width() * 0.5 * progress;
            const double hh = r.height() * 0.5 * progress;
            p.drawRoundedRect(QRectF(c.x() - hw, c.y() - hh, hw * 2.0, hh * 2.0), radius, radius);
        }
    }
    if (v.border) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(v.borderColor, 2.0 + 2.0 * progress));
        if (shape == ProgressShape::Ellipse) {
            p.drawEllipse(r.adjusted(2, 2, -2, -2));
        } else {
            p.drawRoundedRect(r.adjusted(2, 2, -2, -2), radius, radius);
        }
    }
    if (v.radial) {
        QRectF arc = r;
        const qreal penW = (shape == ProgressShape::Ellipse) ? 3.0 : 4.0;
        if (shape == ProgressShape::RoundedRect) {
            arc = r.adjusted(8, 8, -8, -8);
            const double side = qMin(arc.width(), arc.height()) * 0.45;
            arc = QRectF(c.x() - side / 2.0, c.y() - side / 2.0, side, side);
        }
        p.setBrush(Qt::NoBrush);
        QColor track = v.progressColor;
        track.setAlpha(80);
        p.setPen(QPen(track, penW));
        p.drawEllipse(arc);
        p.setPen(QPen(v.progressColor, penW));
        p.drawArc(arc, 90 * 16, int(-360 * 16 * progress));
    }
}

} // namespace gazer

