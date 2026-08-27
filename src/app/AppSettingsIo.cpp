#include "app/AppSettings.h"

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

namespace gazer {

AppSettings AppSettings::defaults()
{
    return {};
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
    mouseMoveForesightDoubleZoom =
        o.value(QStringLiteral("mouseMoveForesightDoubleZoom")).toBool(mouseMoveForesightDoubleZoom);
    magPickStyle = o.value(QStringLiteral("magPickStyle")).toInt(magPickStyle);
    mousePickStyle = o.value(QStringLiteral("mousePickStyle")).toInt(mousePickStyle);
    magZoom = o.value(QStringLiteral("magZoom")).toDouble(magZoom);
    magLensSize = o.value(QStringLiteral("magLensSize")).toInt(magLensSize);
    pickZoom = o.value(QStringLiteral("pickZoom")).toDouble(pickZoom);
    pickWindowPx = o.value(QStringLiteral("pickWindowPx")).toInt(pickWindowPx);
    pickWindowRound = o.value(QStringLiteral("pickWindowRound")).toBool(pickWindowRound);
    magFollowProfile = o.value(QStringLiteral("magFollowProfile")).toInt(magFollowProfile);
    ltsDeadzonePx = o.value(QStringLiteral("ltsDeadzonePx")).toInt(ltsDeadzonePx);
    ltsFalloffPx = o.value(QStringLiteral("ltsFalloffPx")).toInt(ltsFalloffPx);
    ltsMaxNotchesPerSec =
        o.value(QStringLiteral("ltsMaxNotchesPerSec")).toDouble(ltsMaxNotchesPerSec);
    ltsAccelPerSec = o.value(QStringLiteral("ltsAccelPerSec")).toDouble(ltsAccelPerSec);
    ltsCenterDwellMs = o.value(QStringLiteral("ltsCenterDwellMs")).toInt(ltsCenterDwellMs);
    ltsPlaceCursorFirst =
        o.value(QStringLiteral("ltsPlaceCursorFirst")).toBool(ltsPlaceCursorFirst);
    ltsIndicatorStyle =
        ltsIndicatorFromInt(o.value(QStringLiteral("ltsIndicatorStyle")).toInt(int(ltsIndicatorStyle)));
    autoCollapseMain = o.value(QStringLiteral("autoCollapseMain")).toBool(autoCollapseMain);
    startDocked = o.value(QStringLiteral("startDocked")).toBool(startDocked);
    layoutAutoClose = o.value(QStringLiteral("layoutAutoClose")).toBool(layoutAutoClose);
    layoutAutoCloseIdleMs =
        o.value(QStringLiteral("layoutAutoCloseIdleMs")).toInt(layoutAutoCloseIdleMs);
    layoutAutoCloseFadeMs =
        o.value(QStringLiteral("layoutAutoCloseFadeMs")).toInt(layoutAutoCloseFadeMs);
    trackerPref = o.value(QStringLiteral("trackerPref")).toInt(trackerPref);
    speakAlsoType = o.value(QStringLiteral("speakAlsoType")).toBool(speakAlsoType);
    themeMode = themeModeFromString(o.value(QStringLiteral("themeMode")).toString(QStringLiteral("dark")));
    if (o.contains(QStringLiteral("lightColors"))) {
        lightColors.fromJson(o.value(QStringLiteral("lightColors")).toObject());
    }
    if (o.contains(QStringLiteral("darkColors"))) {
        darkColors.fromJson(o.value(QStringLiteral("darkColors")).toObject());
    }
    if (o.contains(QStringLiteral("customColors"))) {
        customColors.fromJson(o.value(QStringLiteral("customColors")).toObject());
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
    if (o.contains(QStringLiteral("customContrast"))) {
        const QJsonValue cv = o.value(QStringLiteral("customContrast"));
        if (cv.isString()) {
            const QString cs = cv.toString().toLower();
            if (cs == QLatin1String("lowest") || cs == QLatin1String("low")) {
                customContrast = kThemeContrastLowPct;
            } else if (cs == QLatin1String("high") || cs == QLatin1String("highest")) {
                customContrast = kThemeContrastHighPct;
            } else {
                customContrast = kThemeContrastMediumPct;
            }
        } else {
            customContrast = snapContrastPercent(cv.toInt(customContrast));
        }
    }
    progressRadial = o.value(QStringLiteral("progressRadial")).toBool(progressRadial);
    progressFill = o.value(QStringLiteral("progressFill")).toBool(progressFill);
    progressBorder = o.value(QStringLiteral("progressBorder")).toBool(progressBorder);
    progressColor = o.value(QStringLiteral("progressColor")).toString(progressColor);
    progressFillColor = o.value(QStringLiteral("progressFillColor")).toString(progressFillColor);
    progressBorderColor =
        o.value(QStringLiteral("progressBorderColor")).toString(progressBorderColor);
    mouseProgressRadial =
        o.value(QStringLiteral("mouseProgressRadial")).toBool(mouseProgressRadial);
    mouseProgressFill = o.value(QStringLiteral("mouseProgressFill")).toBool(mouseProgressFill);
    mouseProgressBorder =
        o.value(QStringLiteral("mouseProgressBorder")).toBool(mouseProgressBorder);
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

    if (!o.contains(QStringLiteral("customBgColor")) && o.contains(QStringLiteral("customColors"))) {
        customBgColor = colorToHex(customColors.bgMain);
        customPrimaryColor = colorToHex(customColors.accent);
        customSecondaryColor = progressColor;
        customTertiaryColor = colorToHex(customColors.cellActive);
        customSurfaceColor = colorToHex(customColors.bgSurface);
        customTextColor = colorToHex(customColors.text);
        customDangerColor = colorToHex(customColors.danger);
    }
    if (themeMode == ThemeMode::Custom) {
        applyCustomPalette(false);
    }

    clamp();
    GAZER_INFO << "Loaded settings from" << path;
    return true;
}

bool AppSettings::saveToFile(const QString& path, QString* error) const
{
    AppSettings copy = *this;
    copy.clamp();

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
    o.insert(QStringLiteral("mouseMoveSelectTimeoutMs"), copy.mouseMoveSelectTimeoutMs);
    o.insert(QStringLiteral("mouseMoveMagPick"), copy.mouseMoveMagPick);
    o.insert(QStringLiteral("mouseMoveMagPickCenterOnDwell"), copy.mouseMoveMagPickCenterOnDwell);
    o.insert(QStringLiteral("mouseMoveMagPickFullScreen"), copy.mouseMoveMagPickFullScreen);
    o.insert(QStringLiteral("mouseMoveForesight"), copy.mouseMoveForesight);
    o.insert(QStringLiteral("mouseMoveForesightDwellMs"), copy.mouseMoveForesightDwellMs);
    o.insert(QStringLiteral("mouseMoveForesightDoubleZoom"), copy.mouseMoveForesightDoubleZoom);
    o.insert(QStringLiteral("magPickStyle"), copy.magPickStyle);
    o.insert(QStringLiteral("mousePickStyle"), copy.mousePickStyle);
    o.insert(QStringLiteral("magZoom"), copy.magZoom);
    o.insert(QStringLiteral("magLensSize"), copy.magLensSize);
    o.insert(QStringLiteral("pickZoom"), copy.pickZoom);
    o.insert(QStringLiteral("pickWindowPx"), copy.pickWindowPx);
    o.insert(QStringLiteral("pickWindowRound"), copy.pickWindowRound);
    o.insert(QStringLiteral("magFollowProfile"), copy.magFollowProfile);
    o.insert(QStringLiteral("ltsDeadzonePx"), copy.ltsDeadzonePx);
    o.insert(QStringLiteral("ltsFalloffPx"), copy.ltsFalloffPx);
    o.insert(QStringLiteral("ltsMaxNotchesPerSec"), copy.ltsMaxNotchesPerSec);
    o.insert(QStringLiteral("ltsAccelPerSec"), copy.ltsAccelPerSec);
    o.insert(QStringLiteral("ltsCenterDwellMs"), copy.ltsCenterDwellMs);
    o.insert(QStringLiteral("ltsPlaceCursorFirst"), copy.ltsPlaceCursorFirst);
    o.insert(QStringLiteral("ltsIndicatorStyle"), int(copy.ltsIndicatorStyle));
    o.insert(QStringLiteral("autoCollapseMain"), copy.autoCollapseMain);
    o.insert(QStringLiteral("startDocked"), copy.startDocked);
    o.insert(QStringLiteral("layoutAutoClose"), copy.layoutAutoClose);
    o.insert(QStringLiteral("layoutAutoCloseIdleMs"), copy.layoutAutoCloseIdleMs);
    o.insert(QStringLiteral("layoutAutoCloseFadeMs"), copy.layoutAutoCloseFadeMs);
    o.insert(QStringLiteral("trackerPref"), copy.trackerPref);
    o.insert(QStringLiteral("speakAlsoType"), copy.speakAlsoType);
    o.insert(QStringLiteral("themeMode"), themeModeToString(copy.themeMode));
    o.insert(QStringLiteral("lightColors"), copy.lightColors.toJson());
    o.insert(QStringLiteral("darkColors"), copy.darkColors.toJson());
    o.insert(QStringLiteral("customColors"), copy.customColors.toJson());
    o.insert(QStringLiteral("customBgColor"), copy.customBgColor);
    o.insert(QStringLiteral("customPrimaryColor"), copy.customPrimaryColor);
    o.insert(QStringLiteral("customSecondaryColor"), copy.customSecondaryColor);
    o.insert(QStringLiteral("customTertiaryColor"), copy.customTertiaryColor);
    o.insert(QStringLiteral("customSurfaceColor"), copy.customSurfaceColor);
    o.insert(QStringLiteral("customTextColor"), copy.customTextColor);
    o.insert(QStringLiteral("customDangerColor"), copy.customDangerColor);
    o.insert(QStringLiteral("themeBrightness"), copy.themeBrightness);
    o.insert(QStringLiteral("customContrast"), copy.customContrast);
    o.insert(QStringLiteral("progressRadial"), copy.progressRadial);
    o.insert(QStringLiteral("progressFill"), copy.progressFill);
    o.insert(QStringLiteral("progressBorder"), copy.progressBorder);
    o.insert(QStringLiteral("progressColor"), copy.progressColor);
    o.insert(QStringLiteral("progressFillColor"), copy.progressFillColor);
    o.insert(QStringLiteral("progressBorderColor"), copy.progressBorderColor);
    o.insert(QStringLiteral("mouseProgressRadial"), copy.mouseProgressRadial);
    o.insert(QStringLiteral("mouseProgressFill"), copy.mouseProgressFill);
    o.insert(QStringLiteral("mouseProgressBorder"), copy.mouseProgressBorder);
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
