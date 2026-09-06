#pragma once

#include "layout/ProgressStyle.h"
#include "ui/Theme.h"

#include <QColor>
#include <QString>
#include <QtMath>

namespace gazer {

/// How dwell progress is drawn on activators (and mouse-move reticle).
struct ProgressVisuals {
    ProgressStyle style;

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

    /// Pointer pick has no item foreground. Custom flash color as-is, or that
    /// color at flashForegroundOpacity when "use foreground" is on.
    [[nodiscard]] QColor pointerFlashColor() const
    {
        QColor c = flashColor.isValid() ? flashColor : QColor(255, 255, 255);
        if (flashUseForeground) {
            c.setAlpha(qBound(0, qRound(255.0 * double(flashForegroundOpacity) / 100.0), 255));
        }
        return c;
    }

    [[nodiscard]] ProgressVisuals withItemFlash(const QColor& itemForeground) const
    {
        ProgressVisuals v = *this;
        v.flashColor = v.resolvedFlashColor(itemForeground);
        return v;
    }

    void applyStyle(const ProgressStyle& s) { style = s; }

    void setStylesFromCsv(const QString& csv) { style = ProgressStyle::fromCsv(csv); }

    [[nodiscard]] QString stylesCsv() const { return style.toCsv(); }
};

} // namespace gazer
