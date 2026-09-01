#pragma once

#include <QColor>
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <optional>

namespace gazer {

enum class ThemeAppearance {
    Light,
    LightTinted,
    DarkTinted,
    Dark
};

[[nodiscard]] inline bool themeAppearanceIsDark(ThemeAppearance a)
{
    return a == ThemeAppearance::Dark || a == ThemeAppearance::DarkTinted;
}

[[nodiscard]] inline bool themeAppearanceIsTinted(ThemeAppearance a)
{
    return a == ThemeAppearance::LightTinted || a == ThemeAppearance::DarkTinted;
}

/// Brand fills in Page XML (`red`, `blue`, …) are translucent overlays.
constexpr int kNamedBrandFillAlpha = 0x99;

/// Chrome palette. JSON ownership lives here (toJson/fromJson).
/// AppSettings derives a live palette from appearance × accent × progress × saturation.
struct ThemeColors {
    QColor bgMain;
    QColor bgSurface;
    QColor bgSurfaceHover;
    QColor bgSurfaceActive;
    QColor border;
    QColor accent;
    QColor accentHover;
    QColor text;
    QColor textSecondary;
    QColor cellBg;
    QColor cellHover;
    QColor cellActive;
    QColor danger;
    /// Settings Progress swatch. Not persisted in toJson.
    QColor progress;
    ThemeAppearance appearance = ThemeAppearance::Dark;

    [[nodiscard]] static ThemeColors darkPreset();
    [[nodiscard]] static ThemeColors lightPreset();

    [[nodiscard]] QJsonObject toJson() const;
    void fromJson(const QJsonObject& o);

    /// Hex helpers used by settings persistence and UI.
    [[nodiscard]] static QColor parseColor(const QString& hex, const QColor& fallback = Qt::cyan);
    [[nodiscard]] static QString colorToHex(const QColor& c);

    /// Progress / overlay accent when a setting or theme color is missing.
    [[nodiscard]] static QColor defaultProgressColor() { return QColor(0, 220, 255); }

    /// Page XML tokens: Settings roles (`background`, `surface`, `accent`,
    /// `progress`, `tertiary`, `foreground`, `danger`) or an accent brand
    /// (`red`, `orange`, `yellow`, `green`, `teal`, `blue`, `indigo`, `purple`,
    /// `pink`).
    [[nodiscard]] static bool isNamedColor(const QString& name);
    [[nodiscard]] std::optional<QColor> namedColor(const QString& name) const;
    [[nodiscard]] std::optional<QColor> resolveToken(const QString& token) const;

    /// Readable text color for a fill (theme light/dark text).
    [[nodiscard]] static QColor contrastOn(const QColor& fill);
    /// WCAG contrast ratio of two opaque colors (1–21).
    [[nodiscard]] static double contrastRatio(const QColor& a, const QColor& b);
    /// Channel-wise mix. Invalid colors fall back to dark gray / the other side.
    [[nodiscard]] static QColor mix(const QColor& a, const QColor& b, double t);
    static constexpr double kReadableContrast = 4.5;
};

constexpr int kThemeBrandCount = 9;
/// Apple system Blue in the Red-to-Pink row.
constexpr int kThemeDefaultBrandIndex = 5;
constexpr int kThemeSaturationLevels = 5;
constexpr int kThemeSaturationMin = 20;
constexpr int kThemeSaturationMax = 100;
constexpr int kThemeSaturationDefault = 60;
constexpr int kThemeSaturationStep = 20;

/// Snap to 20, 40, 60, 80, 100.
[[nodiscard]] inline int snapThemeSaturation(int v)
{
    const int clamped = qBound(kThemeSaturationMin, v, kThemeSaturationMax);
    const int idx = (clamped - kThemeSaturationMin + kThemeSaturationStep / 2)
                    / kThemeSaturationStep;
    return kThemeSaturationMin
           + qBound(0, idx, kThemeSaturationLevels - 1) * kThemeSaturationStep;
}

[[nodiscard]] QString themeAppearanceToString(ThemeAppearance a);
[[nodiscard]] ThemeAppearance themeAppearanceFromString(const QString& s);

/// Voice sample palette (from Voice/js/app.js COLOR_PALETTE).
[[nodiscard]] QVector<QString> voiceColorPalette();

} // namespace gazer
