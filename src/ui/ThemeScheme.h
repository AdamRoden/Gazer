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

/// Named custom-theme roles shown in the color picker.
enum class ThemeColorRole {
    Background,
    Surface,
    Primary,
    Secondary,
    Tertiary,
    Foreground,
    Danger
};

/// Brand accent for the accent card row.
struct ThemeBrandInfo {
    const char* key = "blue";
    const char* name = "Blue";
    QColor color;
};

namespace ThemeScheme {

[[nodiscard]] const ThemeBrandInfo* brands();
[[nodiscard]] int brandCount();
[[nodiscard]] QColor brandCanonical(int index);
[[nodiscard]] QColor brandAccent(int index, ThemeAppearance appearance);
[[nodiscard]] const ThemeBrandInfo* progressSwatches();
[[nodiscard]] QColor progressCanonical(int index);
[[nodiscard]] QColor scaleSaturation(const QColor& c, int saturationPercent);
/// Map an old JSON `themeScheme` key to a brand index. "custom" returns -1.
[[nodiscard]] int brandIndexFromLegacySchemeKey(const QString& key);
[[nodiscard]] const char* progressVariantName(int index);

/// Light/Dark are neutral gray at that brightness. Light tint / Dark tint wash
/// brand hue onto the same brightness. Surfaces keep HSV value.
[[nodiscard]] ThemePalette fluent(ThemeAppearance appearance, int saturation, const QColor& primary,
                                  const QColor& secondary = {});

/// Brand accent × progress swatch × appearance. Custom uses @p customSeeds primary/progress.
[[nodiscard]] ThemePalette resolve(ThemeAppearance appearance, int saturation, int primaryIndex,
                                   int secondaryIndex, bool custom,
                                   const ThemeSeeds& customSeeds = {});

} // namespace ThemeScheme

} // namespace gazer
