#pragma once

#include "ui/Theme.h"

#include <QColor>

namespace gazer {

/// User-chosen custom-theme seeds. HCT-like tones are applied at build time.
struct ThemeSeeds {
    QColor background;
    QColor primary;
    QColor secondary;
    QColor tertiary;
    int contrastPercent = kThemeContrastMediumPct;
};

/// Resolved chrome + progress colors from seeds.
struct ThemePalette {
    ThemeColors colors;
    QColor progress;
    QColor progressFill;
    QColor progressBorder;
};

/// Contrast percent (70 / 85 / 100) inferred from background + primary.
struct ThemeSliders {
    int contrast = 2;
    int brightness = 4;
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

namespace ThemeScheme {

/// Suggest primary / secondary / tertiary from a background (Material 3 hue split).
[[nodiscard]] ThemeSeeds suggestFromBackground(const QColor& background, int contrastPercent);

/// Suggest secondary / tertiary / background from an accent, keeping the current light/dark.
[[nodiscard]] ThemeSeeds suggestFromAccent(const QColor& accent, int contrastPercent,
                                           const QColor& currentBackground);

/// One-role suggestion. Primary uses background+secondary, secondary uses background+primary,
/// background uses primary, tertiary uses background+primary+secondary,
/// surface/foreground/danger use background.
[[nodiscard]] QColor suggestColor(const ThemeSeeds& seeds, ThemeColorRole role);

/// Contrast from the background/primary tone gap, snapped to 70 / 85 / 100.
[[nodiscard]] ThemeSliders inferSliders(const QColor& background, const QColor& primary);

/// Keep hues and chroma; remap tones (and chroma scale) to the contrast level.
[[nodiscard]] ThemeSeeds fitContrast(const ThemeSeeds& seeds);

/// Build the full palette. When @p fitTones is false, seed role colors are used as-is.
[[nodiscard]] ThemePalette build(const ThemeSeeds& seeds, bool fitTones = true);

} // namespace ThemeScheme

} // namespace gazer
