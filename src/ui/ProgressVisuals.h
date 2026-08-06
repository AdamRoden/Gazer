#pragma once

#include "layout/LayoutTypes.h"

#include <QColor>
#include <QString>

namespace gazer {

/// How dwell progress is drawn on activators (and mouse-move reticle).
struct ProgressVisuals {
    bool radial = true;
    bool fillBackground = false;
    bool border = false;

    QColor progressColor = QColor(0, 220, 255);
    QColor fillColor = QColor(0, 180, 220, 70);
    QColor borderColor = QColor(0, 220, 255, 220);

    bool flashOnComplete = true;
    QColor flashBorderColor = QColor(255, 255, 255);
    QColor flashFillColor = QColor(0, 220, 255, 120);
    int flashMs = 140;

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
            const QColor c(hex);
            if (c.isValid()) {
                dest = c;
            }
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
        if (ovr.hasFlashBorderColor) {
            applyColor(v.flashBorderColor, ovr.flashBorderColor);
        }
        if (ovr.hasFlashFillColor) {
            applyColor(v.flashFillColor, ovr.flashFillColor);
        }
        if (ovr.hasFlashMs && ovr.flashMs > 0) {
            v.flashMs = ovr.flashMs;
        }
        return v;
    }
};

} // namespace gazer

