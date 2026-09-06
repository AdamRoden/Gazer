#include "app/AppSettings.h"

#include "ui/ThemeScheme.h"

#include <QtGlobal>

namespace gazer {
namespace {

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
    const ThemePalette pal = ThemeScheme::resolve(s.themeAppearance, kThemeSaturationDefault,
                                                  s.themePrimaryIndex, s.themeSecondaryIndex, false);
    s.customBgColor = AppSettings::colorToHex(pal.colors.bgMain);
    s.customSurfaceColor = AppSettings::colorToHex(pal.colors.bgSurface);
    s.customPrimaryColor = AppSettings::colorToHex(pal.colors.accent);
    s.customSourceColor = s.customPrimaryColor;
    s.customSecondaryColor = AppSettings::colorToHex(pal.progress);
    s.customTertiaryColor = AppSettings::colorToHex(pal.colors.cellActive);
    s.customTextColor = AppSettings::colorToHex(pal.colors.text);
    s.customDangerColor = AppSettings::colorToHex(pal.colors.danger);
}

} // namespace

ThemePalette AppSettings::resolvedPalette() const
{
    if (themeCustom) {
        return ThemeScheme::fluent(themeAppearance, themeSaturation, themeSeeds().primary,
                                   themeSeeds().secondary);
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
    themeSecondaryIndex = qBound(0, themeSecondaryIndex, kThemeBrandCount - 1);
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
    themeSecondaryIndex = qBound(0, index, kThemeBrandCount - 1);
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
    seeds.primary = parseColor(customPrimaryColor, QColor(0x1E, 0x97, 0xF3));
    seeds.secondary = parseColor(customSecondaryColor, QColor(0xFF, 0x47, 0x3D, kProgressFillAlpha));
    return seeds;
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

} // namespace gazer
