#pragma once

#include "ui/Theme.h"

#include <QColor>

namespace gazer {

/// Custom-theme accent + progress. Surfaces overlay from AppSettings seed strings.
struct ThemeSeeds {
    QColor primary;
    QColor secondary;
};

/// Resolved chrome + progress colors.
struct ThemePalette {
    ThemeColors colors;
    QColor progress;
    QColor progressFill;
    QColor progressBorder;
};

/// Apple system color for the accent / progress card rows.
struct ThemeBrandInfo {
    const char* key = "blue";
    const char* name = "Blue";
    QColor light;
    QColor dark;

    [[nodiscard]] QColor colorFor(ThemeAppearance appearance) const
    {
        return themeAppearanceIsDark(appearance) ? dark : light;
    }
};

namespace ThemeScheme {

[[nodiscard]] const ThemeBrandInfo* brands();
[[nodiscard]] int brandCount();
[[nodiscard]] QColor brandCanonical(int index);
[[nodiscard]] QColor brandAccent(int index, ThemeAppearance appearance);
[[nodiscard]] QColor scaleSaturation(const QColor& c, int saturationPercent);
/// Map an old JSON `themeScheme` key to a brand index. "custom" returns -1.
[[nodiscard]] int brandIndexFromLegacySchemeKey(const QString& key);

/// Light/Dark are neutral gray at that brightness. A valid @p surfaceTint washes
/// that hue onto the same brightness. Surfaces keep HSV value.
[[nodiscard]] ThemePalette fluent(ThemeAppearance appearance, int saturation, const QColor& primary,
                                  const QColor& secondary = {},
                                  int brightness = kThemeBrightnessDefault,
                                  const QColor& surfaceTint = {});

/// Apple system accent × progress × appearance. Custom uses @p customSeeds primary/progress.
[[nodiscard]] ThemePalette resolve(ThemeAppearance appearance, int saturation, int primaryIndex,
                                   int secondaryIndex, bool custom,
                                   const ThemeSeeds& customSeeds = {},
                                   int brightness = kThemeBrightnessDefault,
                                   const QColor& surfaceTint = {});

} // namespace ThemeScheme

} // namespace gazer
