#pragma once

#include "ui/Theme.h"

#include <QColor>

namespace gazer {

/// Accent + progress seeds from AppSettings color strings.
struct ThemeSeeds {
    QColor primary;
    QColor secondary;
};

/// Resolved chrome + progress colors.
struct ThemePalette {
    ThemeColors colors;
    QColor progress;
    QColor progressFill;
};

namespace ThemeScheme {

[[nodiscard]] QColor scaleSaturation(const QColor& c, int saturationPercent);

/// Light/Dark are neutral gray at that brightness. A valid @p surfaceTint washes
/// that hue onto the same brightness. Surfaces keep HSV value.
[[nodiscard]] ThemePalette fluent(ThemeAppearance appearance, int saturation, const QColor& primary,
                                  const QColor& secondary = {},
                                  int brightness = kThemeBrightnessDefault,
                                  const QColor& surfaceTint = {});

} // namespace ThemeScheme

} // namespace gazer
