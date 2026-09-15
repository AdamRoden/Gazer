#pragma once

#include <QColor>
#include <QString>
#include <QtGlobal>
#include <optional>

namespace gazer {

enum class ThemeAppearance {
    Light,
    LightTinted,
    DarkTinted,
    Dark
};

/// Hue washed onto Light/Dark neutrals. None is gray.
enum class ThemeTintFamily {
    None,
    Primary,
    Complementary,
    Analogous1,
    Analogous2,
    Tertiary1,
    Tertiary2
};

[[nodiscard]] inline bool themeAppearanceIsDark(ThemeAppearance a)
{
    return a == ThemeAppearance::Dark || a == ThemeAppearance::DarkTinted;
}

[[nodiscard]] inline bool themeAppearanceIsTinted(ThemeAppearance a)
{
    return a == ThemeAppearance::LightTinted || a == ThemeAppearance::DarkTinted;
}

/// Pie / theme progress fills (60%).
constexpr int kProgressFillAlpha = 0x99;

/// Opaque progress fills get 60% alpha. Authored translucent colors stay as-is.
[[nodiscard]] inline QColor withProgressFillAlpha(const QColor& c)
{
    if (!c.isValid() || c.alpha() < 255) {
        return c;
    }
    QColor o = c;
    o.setAlpha(kProgressFillAlpha);
    return o;
}

/// Page XML tone stops (`bg100` … `bg05`, `accent100` … `accent05`) and
/// picker families (`red05` … `red95`, same weights on orange…neutral).
/// For `bg*` / `accent*`, 100 is the seed; lower numbers mix toward white or
/// black, whichever contrasts the seed. Picker tokens are fixed hexes.
constexpr int kToneCount = 12;
constexpr int kToneWeights[kToneCount] = {100, 95, 90, 80, 70, 60, 50, 40, 30, 20, 10, 5};
/// Unset chrome and `bg100` are the canvas. Hover/active step from this.
constexpr int kRestTone = 100;
constexpr int kHoverToneSteps = 1;
/// First 10-weight rung (after 100/95/90). Ink hold steps from here.
constexpr int kFgHoldFromTone = 80;
constexpr int kFgHoldSteps = 2;
/// Active `bg*` / `neutral*` mixes this much accent into a one-stop shade toward black.
constexpr double kActiveAccentMix = 0.5;

/// Parsed `bg*` / `accent*` / picker-family token. Empty `family` means hex or a
/// non-tone role.
struct ToneRef {
    QString family;
    int weight = kRestTone;
};

/// Chrome palette. AppSettings derives a live palette from appearance × accent ×
/// progress × saturation (not this struct).
struct ThemeColors {
    QColor bgMain;
    QColor border;
    QColor accent;
    QColor accentHover;
    QColor text;
    QColor textSecondary;
    QColor danger;
    /// Settings Progress swatch.
    QColor progress;
    ThemeAppearance appearance = ThemeAppearance::Dark;

    [[nodiscard]] static ThemeColors darkPreset();
    [[nodiscard]] static ThemeColors lightPreset();

    /// Hex helpers used by settings persistence and UI.
    [[nodiscard]] static QColor parseColor(const QString& hex, const QColor& fallback = Qt::cyan);
    [[nodiscard]] static QString colorToHex(const QColor& c);

    /// Progress / overlay accent when a setting or theme color is missing.
    [[nodiscard]] static QColor defaultProgressColor() { return QColor(0, 220, 255); }

    /// Page XML tokens: roles (`background`, `accent`, `progress`, `foreground`,
    /// `danger`, `border`), tone stops (`bg100`…`bg05`, `accent100`…`accent05`),
    /// or picker stops (`red05`…`red95` and the other 17 families).
    [[nodiscard]] static bool isNamedColor(const QString& name);
    [[nodiscard]] static std::optional<ToneRef> parseToneToken(const QString& name);
    /// Empty / `background` → bg100; `accent` → accent100; else `parseToneToken`.
    [[nodiscard]] static ToneRef toneRef(const QString& token);
    [[nodiscard]] static int toneIndex(int weight);
    [[nodiscard]] static int steppedWeight(int weight, int steps);
    [[nodiscard]] static QColor mixTone(const QColor& seed, int weight);
    /// Mix @p fill toward black by @p steps from @p fromWeight. Same on light and dark.
    [[nodiscard]] static QColor shadeTowardBlack(const QColor& fill, int steps,
                                                 int fromWeight = kRestTone);

    [[nodiscard]] QColor bgAt(int weight) const { return mixTone(bgMain, weight); }
    [[nodiscard]] QColor accentAt(int weight) const { return mixTone(accent, weight); }
    [[nodiscard]] QColor defaultCell() const { return bgAt(kRestTone); }
    [[nodiscard]] QColor defaultHover() const { return bgAt(steppedWeight(kRestTone, kHoverToneSteps)); }
    [[nodiscard]] QColor defaultActive() const
    {
        return mix(shadeTowardBlack(bgMain, kHoverToneSteps), accent, kActiveAccentMix);
    }

    /// Page `background` token as canvas; theme `bgMain` when empty.
    [[nodiscard]] QColor pageCanvas(const QString& pageBgToken) const;
    [[nodiscard]] std::optional<QColor> namedColor(const QString& name,
                                                   const QColor& seed = {}) const;
    [[nodiscard]] std::optional<QColor> resolveToken(const QString& token,
                                                     const QColor& seed = {}) const;
    /// Rest / hover / active fill. Empty token is the canvas (`bg100`).
    /// Active: `bg*` / `neutral*` (and empty) shade toward black then mix accent.
    /// Every other fill is a one-stop shade of the canvas (`bg100`) toward black.
    [[nodiscard]] QColor resolveFill(const QString& token, const QColor& seed, bool hovered,
                                     bool active) const;
    /// Light or dark ink: prefer @p authored when it contrasts, else switch.
    /// Transparent fills are judged against @p canvas. Authored `bg*` / `accent*`
    /// tokens mix against that canvas.
    [[nodiscard]] QColor readableForeground(const QColor& fill, const QString& authoredToken = {},
                                            const QColor& canvas = {}) const;

    /// Readable text color for a fill (light ink held two 10-weight stops past mid).
    [[nodiscard]] static QColor contrastOn(const QColor& fill);
    /// WCAG contrast ratio of two opaque colors (1–21).
    [[nodiscard]] static double contrastRatio(const QColor& a, const QColor& b);
    /// Channel-wise mix. Invalid colors fall back to dark gray / the other side.
    [[nodiscard]] static QColor mix(const QColor& a, const QColor& b, double t);
    static constexpr double kReadableContrast = 4.5;
};

constexpr int kThemeSaturationLevels = 5;
constexpr int kThemeSaturationMin = 20;
constexpr int kThemeSaturationMax = 100;
constexpr int kThemeSaturationDefault = 60;
constexpr int kThemeSaturationStep = 20;
constexpr int kThemeBrightnessLevels = 5;
constexpr int kThemeBrightnessMin = 0;
constexpr int kThemeBrightnessMax = 4;
constexpr int kThemeBrightnessDefault = 2;

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
[[nodiscard]] QString themeTintFamilyToString(ThemeTintFamily f);
[[nodiscard]] ThemeTintFamily themeTintFamilyFromString(const QString& s);

} // namespace gazer
