#pragma once

#include "layout/ProgressStyle.h"
#include "ui/Theme.h"

#include <QColor>
#include <QString>

namespace gazer {

/// How dwell progress, hover outline, and completion flash are drawn.
struct ProgressVisuals {
    ProgressStyle style;

    QColor progressColor = ThemeColors::defaultProgressColor();
    QColor fillColor = QColor(0, 180, 220, 70);

    bool flashCustom = false;
    int flashForegroundOpacity = 60;
    QColor flashColor = QColor(255, 255, 255);
    int flashMs = 60;

    QColor hoverBorder;
    double hoverBorderWidth = 0.0;

    [[nodiscard]] QColor resolvedFlashColor(const QColor& itemForeground) const
    {
        if (flashCustom) {
            return flashColor;
        }
        QColor c = itemForeground.isValid() ? itemForeground : QColor(255, 255, 255);
        c.setAlpha(qBound(0, qRound(255.0 * double(flashForegroundOpacity) / 100.0), 255));
        return c;
    }

    /// Pointer pick has no item foreground. Custom flash color as-is, or that
    /// color at flashForegroundOpacity when Custom is off.
    [[nodiscard]] QColor pointerFlashColor() const
    {
        QColor c = flashColor.isValid() ? flashColor : QColor(255, 255, 255);
        if (!flashCustom) {
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

    /// Shift progress and fill toward @p foreground when each is close to @p fill
    /// in HSL. @p canvas is the page behind a translucent fill.
    [[nodiscard]] ProgressVisuals onFill(const QColor& fill, const QColor& foreground,
                                        const QColor& canvas) const
    {
        ProgressVisuals v = *this;
        v.progressColor = ThemeColors::progressOnFill(fill, v.progressColor, foreground, canvas);
        v.fillColor = ThemeColors::progressOnFill(fill, v.fillColor, foreground, canvas);
        return v;
    }

    void applyStyle(const ProgressStyle& s) { style = s; }

    void setStylesFromCsv(const QString& csv) { style = ProgressStyle::fromCsv(csv); }

    [[nodiscard]] QString stylesCsv() const { return style.toCsv(); }
};

} // namespace gazer
