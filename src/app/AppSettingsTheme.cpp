#include "app/AppSettings.h"

#include "ui/MaterialPalette.h"
#include "ui/ThemeScheme.h"

#include <QtGlobal>

namespace gazer {
namespace {

void syncAppearanceWithTint(AppSettings& s)
{
    const bool dark = themeAppearanceIsDark(s.themeAppearance);
    const bool tinted = s.themeTintFamily != ThemeTintFamily::None;
    if (dark) {
        s.themeAppearance = tinted ? ThemeAppearance::DarkTinted : ThemeAppearance::Dark;
    } else {
        s.themeAppearance = tinted ? ThemeAppearance::LightTinted : ThemeAppearance::Light;
    }
}

void syncCustomNeutralsFromAppearance(AppSettings& s)
{
    const ThemePalette pal = ThemeScheme::fluent(
        s.themeAppearance, s.themeSaturation, s.themeSeeds().primary, s.themeSeeds().secondary,
        s.themeBrightness, s.surfaceTintColor());
    s.customBgColor = AppSettings::colorToHex(pal.colors.bgMain);
    s.customSurfaceColor = AppSettings::colorToHex(pal.colors.bgSurface);
    s.customTextColor = AppSettings::colorToHex(pal.colors.text);
}

bool paletteFamilyForTint(ThemeTintFamily tint, MaterialPalette::Family* family)
{
    if (!family) {
        return false;
    }
    switch (tint) {
    case ThemeTintFamily::None:
        return false;
    case ThemeTintFamily::Complementary:
        *family = MaterialPalette::Family::Complementary;
        return true;
    case ThemeTintFamily::Analogous1:
        *family = MaterialPalette::Family::Analogous1;
        return true;
    case ThemeTintFamily::Analogous2:
        *family = MaterialPalette::Family::Analogous2;
        return true;
    case ThemeTintFamily::Tertiary1:
        *family = MaterialPalette::Family::Triadic1;
        return true;
    case ThemeTintFamily::Tertiary2:
        *family = MaterialPalette::Family::Triadic2;
        return true;
    case ThemeTintFamily::Primary:
    default:
        *family = MaterialPalette::Family::Primary;
        return true;
    }
}

void commitTheme(AppSettings& s)
{
    if (s.themeCustom) {
        syncCustomNeutralsFromAppearance(s);
    }
    s.applyTheme();
}

void writeProgress(AppSettings& s, const ThemePalette& pal)
{
    s.progressColor = AppSettings::colorToHex(pal.progress);
    s.progressFillColor = AppSettings::colorToHex(pal.progressFill);
}

void seedCustomFromResolved(AppSettings& s)
{
    const ThemePalette pal = ThemeScheme::resolve(
        s.themeAppearance, kThemeSaturationDefault, s.themePrimaryIndex, s.themeSecondaryIndex,
        false, {}, s.themeBrightness, s.surfaceTintColor());
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
                                   themeSeeds().secondary, themeBrightness, surfaceTintColor());
    }
    return ThemeScheme::resolve(themeAppearance, themeSaturation, themePrimaryIndex,
                                themeSecondaryIndex, false, {}, themeBrightness, surfaceTintColor());
}

ThemeColors AppSettings::resolvedTheme() const
{
    return resolvedPalette().colors;
}

QColor AppSettings::resolvedHoverBorder() const
{
    return hoverCustom ? colorKey(QStringLiteral("hoverColor"))
                       : colorKey(QStringLiteral("progressColor"));
}

void AppSettings::applyTheme()
{
    themePrimaryIndex = qBound(0, themePrimaryIndex, kThemeBrandCount - 1);
    themeSecondaryIndex = qBound(0, themeSecondaryIndex, kThemeBrandCount - 1);
    themeSaturation = snapThemeSaturation(themeSaturation);
    themeBrightness = qBound(kThemeBrightnessMin, themeBrightness, kThemeBrightnessMax);
    syncAppearanceWithTint(*this);
    writeProgress(*this, resolvedPalette());
}

void AppSettings::setThemeAppearance(ThemeAppearance appearance)
{
    themeAppearance = appearance;
    if (themeAppearanceIsTinted(appearance)) {
        if (themeTintFamily == ThemeTintFamily::None) {
            themeTintFamily = ThemeTintFamily::Primary;
        }
    } else {
        themeTintFamily = ThemeTintFamily::None;
    }
    commitTheme(*this);
}

void AppSettings::setThemeDark(bool dark)
{
    themeAppearance = dark ? ThemeAppearance::Dark : ThemeAppearance::Light;
    commitTheme(*this);
}

void AppSettings::setThemeTintFamily(ThemeTintFamily family)
{
    themeTintFamily = family;
    commitTheme(*this);
}

void AppSettings::setThemeBrightness(int brightness)
{
    themeBrightness = qBound(kThemeBrightnessMin, brightness, kThemeBrightnessMax);
    commitTheme(*this);
}

QColor AppSettings::surfaceTintColor() const
{
    MaterialPalette::Family family = MaterialPalette::Family::Primary;
    if (!paletteFamilyForTint(themeTintFamily, &family)) {
        return {};
    }
    const QColor primary =
        themeCustom ? themeSeeds().primary
                    : ThemeScheme::brandAccent(themePrimaryIndex, themeAppearance);
    return MaterialPalette::shade(MaterialPalette::generate(primary), family, 5);
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
        || key == QLatin1String("progressFillColor")) {
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
