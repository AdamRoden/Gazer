#include "app/AppSettings.h"

#include "ui/ThemeScheme.h"

#include <QtGlobal>

namespace gazer {
namespace {

void overlayCustomRoles(const AppSettings& s, ThemePalette& pal)
{
    pal.colors.bgMain = AppSettings::parseColor(s.customBgColor, pal.colors.bgMain);
    const QColor surface = AppSettings::parseColor(s.customSurfaceColor, pal.colors.bgSurface);
    pal.colors.bgSurface = surface;
    pal.colors.cellBg = surface;
    pal.colors.text = AppSettings::parseColor(s.customTextColor, pal.colors.text);

    pal.colors.accent = ThemeScheme::scaleSaturation(
        AppSettings::parseColor(s.customPrimaryColor, pal.colors.accent), s.themeSaturation);
    pal.colors.cellActive = ThemeScheme::scaleSaturation(
        AppSettings::parseColor(s.customTertiaryColor, pal.colors.cellActive), s.themeSaturation);
    pal.colors.bgSurfaceActive = pal.colors.cellActive;
    pal.colors.danger = ThemeScheme::scaleSaturation(
        AppSettings::parseColor(s.customDangerColor, pal.colors.danger), s.themeSaturation);

    const QColor sec = ThemeScheme::scaleSaturation(
        AppSettings::parseColor(s.customSecondaryColor, pal.progress), s.themeSaturation);
    pal.progress = sec;
    pal.progressBorder = sec;
    QColor fill = sec;
    fill.setAlpha(70);
    pal.progressFill = fill;
}

void syncCustomNeutralsFromAppearance(AppSettings& s)
{
    const ThemePalette pal = ThemeScheme::fluent(s.themeAppearance, s.themeSaturation,
                                                 s.themeSeeds().primary, s.themeSeeds().secondary);
    s.customBgColor = AppSettings::colorToHex(pal.colors.bgMain);
    s.customSurfaceColor = AppSettings::colorToHex(pal.colors.bgSurface);
    s.customTextColor = AppSettings::colorToHex(pal.colors.text);
}

void writeProgress(AppSettings& s, const ThemePalette& pal)
{
    s.progressColor = AppSettings::colorToHex(pal.progress);
    s.progressBorderColor = AppSettings::colorToHex(pal.progressBorder);
    s.progressFillColor = AppSettings::colorToHex(pal.progressFill);
}

void seedCustomFromResolved(AppSettings& s)
{
    // Store a saturation-neutral snapshot so overlay can apply the live sat once.
    const ThemePalette pal = ThemeScheme::resolve(s.themeAppearance, kThemeSaturationDefault,
                                                  s.themePrimaryIndex, s.themeSecondaryIndex, false);
    s.customBgColor = AppSettings::colorToHex(pal.colors.bgMain);
    s.customSurfaceColor = AppSettings::colorToHex(pal.colors.bgSurface);
    s.customPrimaryColor = AppSettings::colorToHex(pal.colors.accent);
    s.customSecondaryColor = AppSettings::colorToHex(pal.progress);
    s.customTertiaryColor = AppSettings::colorToHex(pal.colors.cellActive);
    s.customTextColor = AppSettings::colorToHex(pal.colors.text);
    s.customDangerColor = AppSettings::colorToHex(pal.colors.danger);
}

} // namespace

ThemePalette AppSettings::resolvedPalette() const
{
    if (themeCustom) {
        ThemePalette pal = ThemeScheme::fluent(themeAppearance, themeSaturation,
                                               themeSeeds().primary, themeSeeds().secondary);
        overlayCustomRoles(*this, pal);
        return pal;
    }
    return ThemeScheme::resolve(themeAppearance, themeSaturation, themePrimaryIndex,
                                themeSecondaryIndex, false);
}

ThemeColors AppSettings::resolvedTheme() const
{
    return resolvedPalette().colors;
}

void AppSettings::applyTheme()
{
    themePrimaryIndex = qBound(0, themePrimaryIndex, kThemeBrandCount - 1);
    themeSecondaryIndex = qBound(0, themeSecondaryIndex, kThemeHarmonyCount - 1);
    themeSaturation = snapThemeSaturation(themeSaturation);
    writeProgress(*this, resolvedPalette());
}

void AppSettings::setThemeAppearance(ThemeAppearance appearance)
{
    themeAppearance = appearance;
    if (themeCustom) {
        syncCustomNeutralsFromAppearance(*this);
    }
    applyTheme();
}

void AppSettings::setThemeCustom(bool on)
{
    if (on && !themeCustom) {
        seedCustomFromResolved(*this);
    }
    themeCustom = on;
    applyTheme();
}

void AppSettings::setThemePrimaryIndex(int index)
{
    themePrimaryIndex = qBound(0, index, kThemeBrandCount - 1);
    themeCustom = false;
    applyTheme();
}

void AppSettings::setThemeSecondaryIndex(int index)
{
    themeSecondaryIndex = qBound(0, index, kThemeHarmonyCount - 1);
    themeCustom = false;
    applyTheme();
}

void AppSettings::setThemeSaturation(int saturation)
{
    themeSaturation = snapThemeSaturation(saturation);
    applyTheme();
}

ThemeSeeds AppSettings::themeSeeds() const
{
    ThemeSeeds seeds;
    seeds.primary = parseColor(customPrimaryColor, QColor(96, 205, 255));
    seeds.secondary = parseColor(customSecondaryColor, ThemeColors::defaultProgressColor());
    return seeds;
}

QColor AppSettings::suggestedThemeColor(const QString& key) const
{
    const ThemeSeeds seeds = themeSeeds();
    const ThemePalette pal =
        ThemeScheme::fluent(themeAppearance, themeSaturation, seeds.primary, seeds.secondary);
    switch (themeColorRoleForKey(key)) {
    case ThemeColorRole::Surface:
        return pal.colors.bgSurface;
    case ThemeColorRole::Primary:
        return pal.colors.accent;
    case ThemeColorRole::Secondary:
        return pal.progress;
    case ThemeColorRole::Tertiary:
        return pal.colors.cellActive;
    case ThemeColorRole::Foreground:
        return pal.colors.text;
    case ThemeColorRole::Danger:
        return pal.colors.danger;
    case ThemeColorRole::Background:
        return pal.colors.bgMain;
    }
    return pal.colors.accent;
}

QString AppSettings::themeRoleForColorKey(const QString& key)
{
    if (key == QLatin1String("customBgColor")) {
        return QStringLiteral("window");
    }
    if (key == QLatin1String("customSurfaceColor")) {
        return QStringLiteral("surface");
    }
    if (key == QLatin1String("customPrimaryColor")) {
        return QStringLiteral("accent");
    }
    if (key == QLatin1String("customSecondaryColor") || key == QLatin1String("progressColor")
        || key == QLatin1String("progressFillColor") || key == QLatin1String("progressBorderColor")) {
        return QStringLiteral("progress");
    }
    if (key == QLatin1String("customTertiaryColor")) {
        return QStringLiteral("highlight");
    }
    if (key == QLatin1String("customTextColor")) {
        return QStringLiteral("foreground");
    }
    if (key == QLatin1String("customDangerColor")) {
        return QStringLiteral("danger");
    }
    return {};
}

ThemeColorRole AppSettings::themeColorRoleForKey(const QString& key)
{
    if (key == QLatin1String("customSurfaceColor")) {
        return ThemeColorRole::Surface;
    }
    if (key == QLatin1String("customPrimaryColor")) {
        return ThemeColorRole::Primary;
    }
    if (key == QLatin1String("customSecondaryColor") || key == QLatin1String("progressColor")
        || key == QLatin1String("progressFillColor") || key == QLatin1String("progressBorderColor")) {
        return ThemeColorRole::Secondary;
    }
    if (key == QLatin1String("customTertiaryColor")) {
        return ThemeColorRole::Tertiary;
    }
    if (key == QLatin1String("customTextColor")) {
        return ThemeColorRole::Foreground;
    }
    if (key == QLatin1String("customDangerColor")) {
        return ThemeColorRole::Danger;
    }
    return ThemeColorRole::Background;
}

bool AppSettings::isThemeSeedKey(const QString& key)
{
    return !themeRoleForColorKey(key).isEmpty();
}

void AppSettings::applyCustomPalette(bool overlayRoles)
{
    ThemePalette pal = ThemeScheme::fluent(themeAppearance, themeSaturation, themeSeeds().primary,
                                           themeSeeds().secondary);
    if (overlayRoles) {
        overlayCustomRoles(*this, pal);
    } else {
        customBgColor = colorToHex(pal.colors.bgMain);
        customSurfaceColor = colorToHex(pal.colors.bgSurface);
        customPrimaryColor = colorToHex(pal.colors.accent);
        customSecondaryColor = colorToHex(pal.progress);
        customTertiaryColor = colorToHex(pal.colors.cellActive);
        customTextColor = colorToHex(pal.colors.text);
        customDangerColor = colorToHex(pal.colors.danger);
    }
    writeProgress(*this, pal);
}

} // namespace gazer
