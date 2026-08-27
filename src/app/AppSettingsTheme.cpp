#include "app/AppSettings.h"

#include "ui/ThemeScheme.h"

namespace gazer {

void AppSettings::setCustomContrast(int contrastPercent)
{
    customContrast = snapContrastPercent(contrastPercent);
    applyCustomPalette(true);
}

ThemeSeeds AppSettings::themeSeeds() const
{
    ThemeSeeds seeds;
    seeds.background = parseColor(customBgColor, QColor(10, 10, 11));
    seeds.primary = parseColor(customPrimaryColor, QColor(138, 180, 248));
    seeds.secondary = parseColor(customSecondaryColor, ThemeColors::defaultProgressColor());
    seeds.tertiary = parseColor(customTertiaryColor, QColor(126, 82, 96));
    seeds.contrastPercent = snapContrastPercent(customContrast);
    return seeds;
}

void AppSettings::syncThemeSlidersFromSeeds()
{
    const ThemeSliders sl = ThemeScheme::inferSliders(
        parseColor(customBgColor, QColor(10, 10, 11)),
        parseColor(customPrimaryColor, QColor(138, 180, 248)));
    customContrast = snapContrastPercent(sl.contrast);
}

QColor AppSettings::suggestedThemeColor(const QString& key) const
{
    return ThemeScheme::suggestColor(themeSeeds(), themeColorRoleForKey(key));
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

void AppSettings::applyCustomPalette(bool fitContrast)
{
    ThemeSeeds seeds = themeSeeds();
    if (fitContrast) {
        seeds = ThemeScheme::fitContrast(seeds);
        customBgColor = colorToHex(seeds.background);
        customPrimaryColor = colorToHex(seeds.primary);
        customSecondaryColor = colorToHex(seeds.secondary);
        customTertiaryColor = colorToHex(seeds.tertiary);
    }

    const ThemePalette pal = ThemeScheme::build(seeds, /*fitTones=*/false);
    customColors = pal.colors;

    if (fitContrast) {
        customSurfaceColor = colorToHex(customColors.bgSurface);
        customTextColor = colorToHex(customColors.text);
        customDangerColor = colorToHex(customColors.danger);
        progressColor = colorToHex(pal.progress);
        progressBorderColor = colorToHex(pal.progressBorder);
        progressFillColor = colorToHex(pal.progressFill);
    } else {
        customColors.bgMain = parseColor(customBgColor, customColors.bgMain);
        const QColor surface = parseColor(customSurfaceColor, customColors.bgSurface);
        customColors.bgSurface = surface;
        customColors.cellBg = surface;
        customColors.accent = parseColor(customPrimaryColor, customColors.accent);
        customColors.cellActive = parseColor(customTertiaryColor, customColors.cellActive);
        customColors.bgSurfaceActive = customColors.cellActive;
        customColors.text = parseColor(customTextColor, customColors.text);
        customColors.danger = parseColor(customDangerColor, customColors.danger);
        const QColor sec = parseColor(customSecondaryColor, pal.progress);
        progressColor = colorToHex(sec);
        progressBorderColor = colorToHex(sec);
        QColor fill = sec;
        fill.setAlpha(70);
        progressFillColor = colorToHex(fill);
    }
    themeMode = ThemeMode::Custom;
}

} // namespace gazer
