#include "app/AppSettings.h"

#include "assist/GazeFollowProfile.h"
#include "assist/LtsIndicator.h"
#include "ui/Theme.h"
#include "ui/ThemeScheme.h"
#include "utils/Log.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QtGlobal>

namespace gazer {

AppSettings AppSettings::defaults()
{
    AppSettings s;
    s.applyTheme();
    return s;
}

QString AppSettings::defaultFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return QDir(dir).filePath(QStringLiteral("settings.json"));
}

QColor AppSettings::parseColor(const QString& hex, const QColor& fallback)
{
    return ThemeColors::parseColor(hex, fallback);
}

QString AppSettings::colorToHex(const QColor& c)
{
    return ThemeColors::colorToHex(c);
}

QString AppSettings::dwellSequenceString() const
{
    QStringList parts;
    for (int ms : dwellSequence) {
        parts << QString::number(ms);
    }
    return parts.join(QLatin1Char(','));
}

QVector<int> AppSettings::parseDwellSequence(const QString& text, QString* error)
{
    QVector<int> out;
    const QString cleaned = text.trimmed();
    if (cleaned.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Enter at least one dwell time in ms");
        }
        return {};
    }
    const QRegularExpression sep(QStringLiteral("[,;\\s]+"));
    for (const QString& part : cleaned.split(sep, Qt::SkipEmptyParts)) {
        bool ok = false;
        const int v = part.toInt(&ok);
        if (!ok || v < 50 || v > 10000) {
            if (error) {
                *error = QStringLiteral("Each step must be an integer 50–10000 ms");
            }
            return {};
        }
        out.push_back(v);
    }
    if (out.isEmpty() && error) {
        *error = QStringLiteral("Enter at least one dwell time in ms");
    }
    return out;
}

namespace {

bool preApple9SchemeKey(const QString& key)
{
    const QString t = key.toLower();
    return t == QLatin1String("blue") || t == QLatin1String("green") || t == QLatin1String("amber")
           || t == QLatin1String("red") || t == QLatin1String("meadow")
           || t == QLatin1String("forest") || t == QLatin1String("orchid")
           || t == QLatin1String("dusk") || t == QLatin1String("bloom")
           || t == QLatin1String("sunset") || t == QLatin1String("copper")
           || t == QLatin1String("sand");
}

} // namespace

bool AppSettings::loadFromFile(const QString& path, QString* error)
{
    QFile f(path);
    if (!f.exists()) {
        if (error) {
            *error = QStringLiteral("Settings file not found");
        }
        return false;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Cannot open settings: %1").arg(f.errorString());
        }
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) {
        if (error) {
            *error = QStringLiteral("Settings JSON root must be an object");
        }
        return false;
    }
    const QJsonObject o = doc.object();

    if (o.contains(QStringLiteral("dwellSequence"))) {
        dwellSequence.clear();
        for (const QJsonValue& v : o.value(QStringLiteral("dwellSequence")).toArray()) {
            const int n = v.toInt(0);
            if (n > 0) {
                dwellSequence.push_back(n);
            }
        }
    } else if (o.contains(QStringLiteral("dwellMs"))) {
        const QJsonValue ms = o.value(QStringLiteral("dwellMs"));
        if (ms.isArray()) {
            dwellSequence.clear();
            for (const QJsonValue& v : ms.toArray()) {
                const int n = v.toInt(0);
                if (n > 0) {
                    dwellSequence.push_back(n);
                }
            }
        } else if (ms.isString()) {
            dwellSequence = parseDwellSequence(ms.toString());
        } else {
            dwellSequence = {ms.toInt(700)};
        }
    }

    scanGraceMs = o.value(QStringLiteral("scanGraceMs")).toInt(scanGraceMs);
    dwellGraceMs = o.value(QStringLiteral("dwellGraceMs")).toInt(dwellGraceMs);
    mouseMoveDwellMs = o.value(QStringLiteral("mouseMoveDwellMs")).toInt(mouseMoveDwellMs);
    magPickDwellMs = o.value(QStringLiteral("magPickDwellMs")).toInt(magPickDwellMs);
    if (o.contains(QStringLiteral("customDwellSequence"))) {
        customTiming.sequence.clear();
        for (const QJsonValue& v : o.value(QStringLiteral("customDwellSequence")).toArray()) {
            const int n = v.toInt(0);
            if (n > 0) {
                customTiming.sequence.push_back(n);
            }
        }
    }
    customTiming.scanGraceMs =
        o.value(QStringLiteral("customScanGraceMs")).toInt(customTiming.scanGraceMs);
    customTiming.blinkGraceMs =
        o.value(QStringLiteral("customDwellGraceMs")).toInt(customTiming.blinkGraceMs);
    customTiming.pointerDwellMs =
        o.value(QStringLiteral("customMouseMoveDwellMs")).toInt(customTiming.pointerDwellMs);
    customTiming.zoomDwellMs =
        o.value(QStringLiteral("customMagPickDwellMs")).toInt(customTiming.zoomDwellMs);
    mouseMoveSelectTimeoutMs =
        o.value(QStringLiteral("mouseMoveSelectTimeoutMs")).toInt(mouseMoveSelectTimeoutMs);
    mouseMoveMagPick = o.value(QStringLiteral("mouseMoveMagPick")).toBool(mouseMoveMagPick);
    mouseMoveMagPickCenterOnDwell =
        o.value(QStringLiteral("mouseMoveMagPickCenterOnDwell")).toBool(mouseMoveMagPickCenterOnDwell);
    mouseMoveMagPickFullScreen =
        o.value(QStringLiteral("mouseMoveMagPickFullScreen")).toBool(mouseMoveMagPickFullScreen);
    mouseMoveForesight = o.value(QStringLiteral("mouseMoveForesight")).toBool(mouseMoveForesight);
    mouseMoveForesightDwellMs =
        o.value(QStringLiteral("mouseMoveForesightDwellMs")).toInt(mouseMoveForesightDwellMs);
    mouseMoveForesightHoldMs =
        o.value(QStringLiteral("mouseMoveForesightHoldMs")).toInt(mouseMoveForesightHoldMs);
    mouseMoveForesightSecondZoom =
        o.value(QStringLiteral("mouseMoveForesightSecondZoom")).toBool(mouseMoveForesightSecondZoom);
    magPickStyle = o.value(QStringLiteral("magPickStyle")).toInt(magPickStyle);
    mousePickStyle = o.value(QStringLiteral("mousePickStyle")).toInt(mousePickStyle);
    magZoom = o.value(QStringLiteral("magZoom")).toDouble(magZoom);
    magLensSize = o.value(QStringLiteral("magLensSize")).toInt(magLensSize);
    pickZoom = o.value(QStringLiteral("pickZoom")).toDouble(pickZoom);
    pickWindowPx = o.value(QStringLiteral("pickWindowPx")).toInt(pickWindowPx);
    pickWindowRound = o.value(QStringLiteral("pickWindowRound")).toBool(pickWindowRound);
    magFollowProfile = gazeFollowProfileFromInt(
        o.value(QStringLiteral("magFollowProfile")).toInt(int(magFollowProfile)));
    ltsDeadzonePx = o.value(QStringLiteral("ltsDeadzonePx")).toInt(ltsDeadzonePx);
    ltsFalloffPx = o.value(QStringLiteral("ltsFalloffPx")).toInt(ltsFalloffPx);
    ltsMaxNotchesPerSec =
        o.value(QStringLiteral("ltsMaxNotchesPerSec")).toDouble(ltsMaxNotchesPerSec);
    ltsAccelPerSec = o.value(QStringLiteral("ltsAccelPerSec")).toDouble(ltsAccelPerSec);
    ltsCenterDwellMs = o.value(QStringLiteral("ltsCenterDwellMs")).toInt(ltsCenterDwellMs);
    ltsIndicatorStyle =
        ltsIndicatorFromInt(o.value(QStringLiteral("ltsIndicatorStyle")).toInt(int(ltsIndicatorStyle)));
    comboInnerRadiusPx = o.value(QStringLiteral("comboInnerRadiusPx")).toInt(comboInnerRadiusPx);
    comboSharedRadiusPx = o.value(QStringLiteral("comboSharedRadiusPx")).toInt(comboSharedRadiusPx);
    comboOuterRadiusPx = o.value(QStringLiteral("comboOuterRadiusPx")).toInt(comboOuterRadiusPx);
    comboInnerColor = o.value(QStringLiteral("comboInnerColor")).toString(comboInnerColor);
    comboOuterColor = o.value(QStringLiteral("comboOuterColor")).toString(comboOuterColor);
    autoCollapseMain = o.value(QStringLiteral("autoCollapseMain")).toBool(autoCollapseMain);
    startDocked = o.value(QStringLiteral("startDocked")).toBool(startDocked);
    layoutAutoClose = o.value(QStringLiteral("layoutAutoClose")).toBool(layoutAutoClose);
    layoutAutoCloseIdleMs =
        o.value(QStringLiteral("layoutAutoCloseIdleMs")).toInt(layoutAutoCloseIdleMs);
    layoutAutoCloseFadeMs =
        o.value(QStringLiteral("layoutAutoCloseFadeMs")).toInt(layoutAutoCloseFadeMs);
    trackerPref = o.value(QStringLiteral("trackerPref")).toInt(trackerPref);
    speakAlsoType = o.value(QStringLiteral("speakAlsoType")).toBool(speakAlsoType);
    const QString legacyMode =
        o.value(QStringLiteral("themeMode")).toString(QStringLiteral("dark")).toLower();
    if (o.contains(QStringLiteral("themeAppearance"))) {
        themeAppearance = themeAppearanceFromString(
            o.value(QStringLiteral("themeAppearance")).toString());
    } else {
        themeAppearance = legacyMode == QLatin1String("light") ? ThemeAppearance::Light
                                                               : ThemeAppearance::Dark;
    }
    const QString schemeKey = o.value(QStringLiteral("themeScheme")).toString();
    bool schemeMapsToBrand = false;
    if (o.contains(QStringLiteral("themeScheme"))) {
        const int brand = ThemeScheme::brandIndexFromLegacySchemeKey(schemeKey);
        themeCustom = brand < 0;
        if (brand >= 0) {
            themePrimaryIndex = brand;
            schemeMapsToBrand = true;
        }
    } else {
        themeCustom = legacyMode == QLatin1String("custom");
    }
    if (o.contains(QStringLiteral("themePrimaryIndex")) && !schemeMapsToBrand) {
        themePrimaryIndex = o.value(QStringLiteral("themePrimaryIndex")).toInt(kThemeDefaultBrandIndex);
    }
    const QString secondaryKey = o.value(QStringLiteral("themeSecondaryScheme")).toString();
    const int secondaryBrand = ThemeScheme::brandIndexFromLegacySchemeKey(secondaryKey);
    if (!secondaryKey.isEmpty() && secondaryBrand >= 0) {
        themeSecondaryIndex = secondaryBrand;
    } else if (o.contains(QStringLiteral("themeSecondaryIndex"))) {
        const int old = o.value(QStringLiteral("themeSecondaryIndex")).toInt(kThemeDefaultBrandIndex);
        static const int kLegacyFourSwatch[] = {5, 3, 1, 0};
        if (!o.contains(QStringLiteral("themeSecondaryScheme")) && preApple9SchemeKey(schemeKey)
            && old >= 0 && old < 4) {
            themeSecondaryIndex = kLegacyFourSwatch[old];
        } else {
            themeSecondaryIndex = old;
        }
    }
    if (o.contains(QStringLiteral("themeSaturation"))) {
        themeSaturation = o.value(QStringLiteral("themeSaturation")).toInt(kThemeSaturationDefault);
    } else if (o.contains(QStringLiteral("themeVibrance"))) {
        themeSaturation = o.value(QStringLiteral("themeVibrance")).toInt(kThemeSaturationDefault);
    }
    ThemeColors legacyCustomColors;
    const bool hasLegacyCustomColors = o.contains(QStringLiteral("customColors"));
    if (hasLegacyCustomColors) {
        legacyCustomColors.fromJson(o.value(QStringLiteral("customColors")).toObject());
    }
    customBgColor = o.value(QStringLiteral("customBgColor")).toString(customBgColor);
    customPrimaryColor = o.value(QStringLiteral("customPrimaryColor")).toString(customPrimaryColor);
    customSecondaryColor =
        o.value(QStringLiteral("customSecondaryColor")).toString(customSecondaryColor);
    customTertiaryColor =
        o.value(QStringLiteral("customTertiaryColor")).toString(customTertiaryColor);
    customSurfaceColor = o.value(QStringLiteral("customSurfaceColor")).toString(customSurfaceColor);
    customTextColor = o.value(QStringLiteral("customTextColor")).toString(customTextColor);
    customDangerColor = o.value(QStringLiteral("customDangerColor")).toString(customDangerColor);
    themeBrightness = o.value(QStringLiteral("themeBrightness")).toInt(themeBrightness);
    int legacyContrastPct = 85;
    if (o.contains(QStringLiteral("customContrast"))) {
        const QJsonValue cv = o.value(QStringLiteral("customContrast"));
        if (cv.isString()) {
            const QString cs = cv.toString().toLower();
            if (cs == QLatin1String("lowest") || cs == QLatin1String("low")) {
                legacyContrastPct = 70;
            } else if (cs == QLatin1String("high") || cs == QLatin1String("highest")) {
                legacyContrastPct = 100;
            } else {
                legacyContrastPct = 85;
            }
        } else {
            const int v = cv.toInt(85);
            if (v <= 4) {
                legacyContrastPct = v <= 1 ? 70 : (v >= 3 ? 100 : 85);
            } else if (v < 78) {
                legacyContrastPct = 70;
            } else if (v < 93) {
                legacyContrastPct = 85;
            } else {
                legacyContrastPct = 100;
            }
        }
    }
    for (const StyleToggle& t : kStyleToggles) {
        styleFlag(t) = o.value(QLatin1String(t.jsonKey)).toBool(styleFlag(t));
    }
    progressColor = o.value(QStringLiteral("progressColor")).toString(progressColor);
    progressFillColor = o.value(QStringLiteral("progressFillColor")).toString(progressFillColor);
    progressBorderColor =
        o.value(QStringLiteral("progressBorderColor")).toString(progressBorderColor);
    flashUseForeground = o.value(QStringLiteral("flashUseForeground")).toBool(flashUseForeground);
    flashForegroundOpacity =
        o.value(QStringLiteral("flashForegroundOpacity")).toInt(flashForegroundOpacity);
    if (o.contains(QStringLiteral("flashColor"))) {
        flashColor = o.value(QStringLiteral("flashColor")).toString(flashColor);
    } else if (o.contains(QStringLiteral("flashBorderColor"))) {
        flashColor = o.value(QStringLiteral("flashBorderColor")).toString(flashColor);
    } else if (o.contains(QStringLiteral("flashFillColor"))) {
        flashColor = o.value(QStringLiteral("flashFillColor")).toString(flashColor);
    }
    flashMs = o.value(QStringLiteral("flashMs")).toInt(flashMs);

    if (!o.contains(QStringLiteral("customBgColor")) && hasLegacyCustomColors) {
        customBgColor = colorToHex(legacyCustomColors.bgMain);
        customPrimaryColor = colorToHex(legacyCustomColors.accent);
        customSecondaryColor = progressColor;
        customTertiaryColor = colorToHex(legacyCustomColors.cellActive);
        customSurfaceColor = colorToHex(legacyCustomColors.bgSurface);
        customTextColor = colorToHex(legacyCustomColors.text);
        customDangerColor = colorToHex(legacyCustomColors.danger);
    }
    if (!o.contains(QStringLiteral("themeSaturation"))
        && !o.contains(QStringLiteral("themeVibrance"))) {
        const bool moreContrast = o.value(QStringLiteral("themeMoreContrast")).toBool(false)
                                  || legacyContrastPct >= 100;
        if (moreContrast) {
            themeSaturation = kThemeSaturationMax;
        } else if (legacyContrastPct <= 70) {
            themeSaturation = kThemeSaturationMin;
        } else {
            themeSaturation = kThemeSaturationDefault;
        }
    }
    if (themeCustom && legacyMode == QLatin1String("custom")
        && !o.contains(QStringLiteral("themeAppearance"))) {
        const QColor bg = parseColor(customBgColor, QColor(10, 10, 11));
        themeAppearance = bg.lightness() > 140 ? ThemeAppearance::Light : ThemeAppearance::Dark;
    }
    applyTheme();

    clamp();
    GAZER_INFO << "Loaded settings from" << path;
    return true;
}

bool AppSettings::saveToFile(const QString& path, QString* error) const
{
    AppSettings copy = *this;
    copy.clamp();
    copy.applyTheme();

    QJsonObject o;
    o.insert(QStringLiteral("schemaVersion"), 2);
    QJsonArray seq;
    for (int ms : copy.dwellSequence) {
        seq.append(ms);
    }
    o.insert(QStringLiteral("dwellSequence"), seq);
    o.insert(QStringLiteral("scanGraceMs"), copy.scanGraceMs);
    o.insert(QStringLiteral("dwellGraceMs"), copy.dwellGraceMs);
    o.insert(QStringLiteral("mouseMoveDwellMs"), copy.mouseMoveDwellMs);
    o.insert(QStringLiteral("magPickDwellMs"), copy.magPickDwellMs);
    QJsonArray customSeq;
    for (int ms : copy.customTiming.sequence) {
        customSeq.append(ms);
    }
    o.insert(QStringLiteral("customDwellSequence"), customSeq);
    o.insert(QStringLiteral("customScanGraceMs"), copy.customTiming.scanGraceMs);
    o.insert(QStringLiteral("customDwellGraceMs"), copy.customTiming.blinkGraceMs);
    o.insert(QStringLiteral("customMouseMoveDwellMs"), copy.customTiming.pointerDwellMs);
    o.insert(QStringLiteral("customMagPickDwellMs"), copy.customTiming.zoomDwellMs);
    o.insert(QStringLiteral("mouseMoveSelectTimeoutMs"), copy.mouseMoveSelectTimeoutMs);
    o.insert(QStringLiteral("mouseMoveMagPick"), copy.mouseMoveMagPick);
    o.insert(QStringLiteral("mouseMoveMagPickCenterOnDwell"), copy.mouseMoveMagPickCenterOnDwell);
    o.insert(QStringLiteral("mouseMoveMagPickFullScreen"), copy.mouseMoveMagPickFullScreen);
    o.insert(QStringLiteral("mouseMoveForesight"), copy.mouseMoveForesight);
    o.insert(QStringLiteral("mouseMoveForesightDwellMs"), copy.mouseMoveForesightDwellMs);
    o.insert(QStringLiteral("mouseMoveForesightHoldMs"), copy.mouseMoveForesightHoldMs);
    o.insert(QStringLiteral("mouseMoveForesightSecondZoom"), copy.mouseMoveForesightSecondZoom);
    o.insert(QStringLiteral("magPickStyle"), copy.magPickStyle);
    o.insert(QStringLiteral("mousePickStyle"), copy.mousePickStyle);
    o.insert(QStringLiteral("magZoom"), copy.magZoom);
    o.insert(QStringLiteral("magLensSize"), copy.magLensSize);
    o.insert(QStringLiteral("pickZoom"), copy.pickZoom);
    o.insert(QStringLiteral("pickWindowPx"), copy.pickWindowPx);
    o.insert(QStringLiteral("pickWindowRound"), copy.pickWindowRound);
    o.insert(QStringLiteral("magFollowProfile"), int(copy.magFollowProfile));
    o.insert(QStringLiteral("ltsDeadzonePx"), copy.ltsDeadzonePx);
    o.insert(QStringLiteral("ltsFalloffPx"), copy.ltsFalloffPx);
    o.insert(QStringLiteral("ltsMaxNotchesPerSec"), copy.ltsMaxNotchesPerSec);
    o.insert(QStringLiteral("ltsAccelPerSec"), copy.ltsAccelPerSec);
    o.insert(QStringLiteral("ltsCenterDwellMs"), copy.ltsCenterDwellMs);
    o.insert(QStringLiteral("ltsIndicatorStyle"), int(copy.ltsIndicatorStyle));
    o.insert(QStringLiteral("comboInnerRadiusPx"), copy.comboInnerRadiusPx);
    o.insert(QStringLiteral("comboSharedRadiusPx"), copy.comboSharedRadiusPx);
    o.insert(QStringLiteral("comboOuterRadiusPx"), copy.comboOuterRadiusPx);
    o.insert(QStringLiteral("comboInnerColor"), copy.comboInnerColor);
    o.insert(QStringLiteral("comboOuterColor"), copy.comboOuterColor);
    o.insert(QStringLiteral("autoCollapseMain"), copy.autoCollapseMain);
    o.insert(QStringLiteral("startDocked"), copy.startDocked);
    o.insert(QStringLiteral("layoutAutoClose"), copy.layoutAutoClose);
    o.insert(QStringLiteral("layoutAutoCloseIdleMs"), copy.layoutAutoCloseIdleMs);
    o.insert(QStringLiteral("layoutAutoCloseFadeMs"), copy.layoutAutoCloseFadeMs);
    o.insert(QStringLiteral("trackerPref"), copy.trackerPref);
    o.insert(QStringLiteral("speakAlsoType"), copy.speakAlsoType);
    const bool darkAppearance = themeAppearanceIsDark(copy.themeAppearance);
    o.insert(QStringLiteral("themeMode"),
             copy.themeCustom ? QStringLiteral("custom")
                              : (darkAppearance ? QStringLiteral("dark") : QStringLiteral("light")));
    o.insert(QStringLiteral("themeAppearance"), themeAppearanceToString(copy.themeAppearance));
    o.insert(QStringLiteral("themeScheme"),
             copy.themeCustom
                 ? QStringLiteral("custom")
                 : QLatin1String(ThemeScheme::brands()[qBound(0, copy.themePrimaryIndex,
                                                              kThemeBrandCount - 1)]
                                     .key));
    o.insert(QStringLiteral("themePrimaryIndex"), copy.themePrimaryIndex);
    o.insert(QStringLiteral("themeSecondaryIndex"), copy.themeSecondaryIndex);
    o.insert(QStringLiteral("themeSecondaryScheme"),
             QLatin1String(ThemeScheme::brands()[qBound(0, copy.themeSecondaryIndex,
                                                        kThemeBrandCount - 1)]
                               .key));
    o.insert(QStringLiteral("themeSaturation"), copy.themeSaturation);
    const ThemePalette lightPal = ThemeScheme::resolve(
        ThemeAppearance::Light, copy.themeSaturation, copy.themePrimaryIndex,
        copy.themeSecondaryIndex, false);
    const ThemePalette darkPal = ThemeScheme::resolve(
        ThemeAppearance::Dark, copy.themeSaturation, copy.themePrimaryIndex,
        copy.themeSecondaryIndex, false);
    o.insert(QStringLiteral("lightColors"), lightPal.colors.toJson());
    o.insert(QStringLiteral("darkColors"), darkPal.colors.toJson());
    o.insert(QStringLiteral("customColors"), copy.resolvedPalette().colors.toJson());
    o.insert(QStringLiteral("customBgColor"), copy.customBgColor);
    o.insert(QStringLiteral("customPrimaryColor"), copy.customPrimaryColor);
    o.insert(QStringLiteral("customSecondaryColor"), copy.customSecondaryColor);
    o.insert(QStringLiteral("customTertiaryColor"), copy.customTertiaryColor);
    o.insert(QStringLiteral("customSurfaceColor"), copy.customSurfaceColor);
    o.insert(QStringLiteral("customTextColor"), copy.customTextColor);
    o.insert(QStringLiteral("customDangerColor"), copy.customDangerColor);
    o.insert(QStringLiteral("themeBrightness"), copy.themeBrightness);
    int derivedContrast = 85;
    if (copy.themeSaturation >= 75) {
        derivedContrast = 100;
    } else if (copy.themeSaturation <= 40) {
        derivedContrast = 70;
    }
    o.insert(QStringLiteral("customContrast"), derivedContrast);
    for (const StyleToggle& t : kStyleToggles) {
        o.insert(QLatin1String(t.jsonKey), copy.styleFlag(t));
    }
    o.insert(QStringLiteral("progressColor"), copy.progressColor);
    o.insert(QStringLiteral("progressFillColor"), copy.progressFillColor);
    o.insert(QStringLiteral("progressBorderColor"), copy.progressBorderColor);
    o.insert(QStringLiteral("flashUseForeground"), copy.flashUseForeground);
    o.insert(QStringLiteral("flashForegroundOpacity"), copy.flashForegroundOpacity);
    o.insert(QStringLiteral("flashColor"), copy.flashColor);
    o.insert(QStringLiteral("flashMs"), copy.flashMs);

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = QStringLiteral("Cannot write settings: %1").arg(f.errorString());
        }
        return false;
    }
    f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
    GAZER_INFO << "Saved settings to" << path;
    return true;
}

} // namespace gazer
