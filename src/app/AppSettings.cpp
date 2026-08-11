#include "app/AppSettings.h"

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

void AppSettings::clamp()
{
    if (dwellSequence.isEmpty()) {
        dwellSequence = {700};
    }
    for (int& ms : dwellSequence) {
        ms = qBound(50, ms, 10000);
    }
    scanGraceMs = qBound(0, scanGraceMs, 2000);
    dwellGraceMs = qBound(0, dwellGraceMs, 800);
    mouseMoveDwellMs = qBound(200, mouseMoveDwellMs, 2500);
    mouseMoveSelectTimeoutMs = qBound(0, mouseMoveSelectTimeoutMs, 120000);
    magZoom = qBound(1.25, magZoom, 6.0);
    magLensSize = qBound(160, magLensSize, 900);
    magFollowProfile = qBound(0, magFollowProfile, 2);
    ltsDeadzonePx = qBound(30, ltsDeadzonePx, 400);
    ltsFalloffPx = qBound(80, ltsFalloffPx, 800);
    ltsMaxNotchesPerSec = qBound(0.5, ltsMaxNotchesPerSec, 24.0);
    ltsAccelPerSec = qBound(0.0, ltsAccelPerSec, 2.0);
    ltsCenterDwellMs = qBound(200, ltsCenterDwellMs, 2500);
    trackerPref = qBound(0, trackerPref, 1);
    flashMs = qBound(40, flashMs, 1000);
    layoutAutoCloseIdleMs = qBound(500, layoutAutoCloseIdleMs, 120000);
    layoutAutoCloseFadeMs = qBound(50, layoutAutoCloseFadeMs, 60000);
    if (!progressRadial && !progressFill && !progressBorder) {
        progressRadial = true;
    }
    if (!mouseProgressRadial && !mouseProgressFill && !mouseProgressBorder) {
        mouseProgressRadial = true;
    }
}

void AppSettings::setDwellPreset(int preset)
{
    switch (qBound(0, preset, 2)) {
    case 0:
        dwellSequence = {1000};
        mouseMoveDwellMs = 900;
        dwellGraceMs = 220;
        scanGraceMs = 150;
        break;
    case 2:
        dwellSequence = {450};
        mouseMoveDwellMs = 500;
        dwellGraceMs = 140;
        scanGraceMs = 80;
        break;
    default:
        dwellSequence = {700};
        mouseMoveDwellMs = 700;
        dwellGraceMs = 180;
        scanGraceMs = 100;
        break;
    }
}

int AppSettings::dwellPreset() const
{
    if (dwellSequence.size() == 1 && dwellSequence[0] <= 520) {
        return 2;
    }
    if (dwellSequence.size() == 1 && dwellSequence[0] >= 900) {
        return 0;
    }
    return 1;
}

void AppSettings::setMagFollowProfile(int profile)
{
    magFollowProfile = qBound(0, profile, 2);
}

bool AppSettings::isColorKey(const QString& key)
{
    return key.endsWith(QLatin1String("Color")) || key.endsWith(QLatin1String("color"));
}

bool AppSettings::isNumericKey(const QString& key)
{
    return key == QLatin1String("dwellMs") || key == QLatin1String("dwellSequence")
           || key == QLatin1String("scanGraceMs") || key == QLatin1String("dwellGraceMs")
           || key == QLatin1String("mouseMoveDwellMs")
           || key == QLatin1String("mouseMoveSelectTimeoutMs")
           || key == QLatin1String("magZoom") || key == QLatin1String("magLensSize")
           || key == QLatin1String("ltsDeadzonePx") || key == QLatin1String("ltsFalloffPx")
           || key == QLatin1String("ltsMaxNotchesPerSec") || key == QLatin1String("ltsAccelPerSec")
           || key == QLatin1String("ltsCenterDwellMs") || key == QLatin1String("flashMs");
}

QColor AppSettings::colorKey(const QString& key) const
{
    if (key == QLatin1String("progressColor")) {
        return parseColor(progressColor);
    }
    if (key == QLatin1String("progressFillColor")) {
        return parseColor(progressFillColor, QColor(0, 180, 220, 70));
    }
    if (key == QLatin1String("progressBorderColor")) {
        return parseColor(progressBorderColor);
    }
    if (key == QLatin1String("mouseProgressColor")) {
        return parseColor(mouseProgressColor, QColor(255, 200, 40));
    }
    if (key == QLatin1String("mouseProgressFillColor")) {
        return parseColor(mouseProgressFillColor, QColor(255, 200, 40, 64));
    }
    if (key == QLatin1String("mouseProgressBorderColor")) {
        return parseColor(mouseProgressBorderColor, QColor(255, 200, 40));
    }
    if (key == QLatin1String("flashBorderColor")) {
        return parseColor(flashBorderColor, Qt::white);
    }
    if (key == QLatin1String("flashFillColor")) {
        return parseColor(flashFillColor, QColor(0, 220, 255, 120));
    }
    return QColor(0, 220, 255);
}

bool AppSettings::setColorKey(const QString& key, const QColor& c)
{
    if (!c.isValid()) {
        return false;
    }
    const QString hex = colorToHex(c);
    if (key == QLatin1String("progressColor")) {
        progressColor = hex;
    } else if (key == QLatin1String("progressFillColor")) {
        progressFillColor = hex;
    } else if (key == QLatin1String("progressBorderColor")) {
        progressBorderColor = hex;
    } else if (key == QLatin1String("mouseProgressColor")) {
        mouseProgressColor = hex;
    } else if (key == QLatin1String("mouseProgressFillColor")) {
        mouseProgressFillColor = hex;
    } else if (key == QLatin1String("mouseProgressBorderColor")) {
        mouseProgressBorderColor = hex;
    } else if (key == QLatin1String("flashBorderColor")) {
        flashBorderColor = hex;
    } else if (key == QLatin1String("flashFillColor")) {
        flashFillColor = hex;
    } else {
        return false;
    }
    return true;
}

QString AppSettings::displayValue(const QString& key) const
{
    if (key == QLatin1String("dwellMs") || key == QLatin1String("dwellSequence")) {
        return dwellSequenceString() + QStringLiteral(" ms");
    }
    if (key == QLatin1String("scanGraceMs")) {
        return QStringLiteral("%1 ms").arg(scanGraceMs);
    }
    if (key == QLatin1String("dwellGraceMs")) {
        return QStringLiteral("%1 ms").arg(dwellGraceMs);
    }
    if (key == QLatin1String("mouseMoveDwellMs")) {
        return QStringLiteral("%1 ms").arg(mouseMoveDwellMs);
    }
    if (key == QLatin1String("mouseMoveSelectTimeoutMs")) {
        if (mouseMoveSelectTimeoutMs <= 0) {
            return QStringLiteral("Off");
        }
        return QStringLiteral("%1 ms").arg(mouseMoveSelectTimeoutMs);
    }
    if (key == QLatin1String("magZoom")) {
        return QStringLiteral("%1×").arg(magZoom, 0, 'f', 2);
    }
    if (key == QLatin1String("magLensSize")) {
        return QStringLiteral("%1 px").arg(magLensSize);
    }
    if (key == QLatin1String("magFollowProfile")) {
        static const char* names[] = {"Sticky", "Balanced", "Snappy"};
        return QLatin1String(names[qBound(0, magFollowProfile, 2)]);
    }
    if (key == QLatin1String("ltsDeadzonePx")) {
        return QStringLiteral("%1 px").arg(ltsDeadzonePx);
    }
    if (key == QLatin1String("ltsFalloffPx")) {
        return QStringLiteral("%1 px").arg(ltsFalloffPx);
    }
    if (key == QLatin1String("ltsMaxNotchesPerSec")) {
        return QStringLiteral("%1 n/s").arg(ltsMaxNotchesPerSec, 0, 'f', 1);
    }
    if (key == QLatin1String("ltsAccelPerSec")) {
        return QStringLiteral("%1 /s").arg(ltsAccelPerSec, 0, 'f', 2);
    }
    if (key == QLatin1String("ltsCenterDwellMs")) {
        return QStringLiteral("%1 ms").arg(ltsCenterDwellMs);
    }
    if (key == QLatin1String("ltsPlaceCursorFirst") || key == QLatin1String("autoCollapseMain")
        || key == QLatin1String("startDocked") || key == QLatin1String("layoutAutoClose")
        || key == QLatin1String("speakAlsoType") || key == QLatin1String("progressRadial")
        || key == QLatin1String("progressFill") || key == QLatin1String("progressBorder")
        || key == QLatin1String("mouseProgressRadial") || key == QLatin1String("mouseProgressFill")
        || key == QLatin1String("mouseProgressBorder") || key == QLatin1String("flashOnComplete")) {
        const bool on = (key == QLatin1String("ltsPlaceCursorFirst") && ltsPlaceCursorFirst)
                        || (key == QLatin1String("autoCollapseMain") && autoCollapseMain)
                        || (key == QLatin1String("startDocked") && startDocked)
                        || (key == QLatin1String("layoutAutoClose") && layoutAutoClose)
                        || (key == QLatin1String("speakAlsoType") && speakAlsoType)
                        || (key == QLatin1String("progressRadial") && progressRadial)
                        || (key == QLatin1String("progressFill") && progressFill)
                        || (key == QLatin1String("progressBorder") && progressBorder)
                        || (key == QLatin1String("mouseProgressRadial") && mouseProgressRadial)
                        || (key == QLatin1String("mouseProgressFill") && mouseProgressFill)
                        || (key == QLatin1String("mouseProgressBorder") && mouseProgressBorder)
                        || (key == QLatin1String("flashOnComplete") && flashOnComplete);
        return on ? QStringLiteral("ON") : QStringLiteral("OFF");
    }
    if (key == QLatin1String("trackerPref")) {
        return trackerPref == 1 ? QStringLiteral("Mouse only") : QStringLiteral("Auto Tobii");
    }
    if (key == QLatin1String("flashMs")) {
        return QStringLiteral("%1 ms").arg(flashMs);
    }
    if (isColorKey(key)) {
        return colorKey(key).name(QColor::HexArgb).toUpper();
    }
    return {};
}

QString AppSettings::settingTitle(const QString& key)
{
    if (key == QLatin1String("dwellMs") || key == QLatin1String("dwellSequence")) {
        return QStringLiteral("Dwell sequence");
    }
    if (key == QLatin1String("scanGraceMs")) {
        return QStringLiteral("Scan grace");
    }
    if (key == QLatin1String("dwellGraceMs")) {
        return QStringLiteral("Blink grace");
    }
    if (key == QLatin1String("mouseMoveDwellMs")) {
        return QStringLiteral("Mouse-move dwell");
    }
    if (key == QLatin1String("mouseMoveSelectTimeoutMs")) {
        return QStringLiteral("Mouse-move timeout");
    }
    if (key == QLatin1String("magZoom")) {
        return QStringLiteral("Magnifier zoom");
    }
    if (key == QLatin1String("magLensSize")) {
        return QStringLiteral("Lens size");
    }
    if (key == QLatin1String("ltsDeadzonePx")) {
        return QStringLiteral("LTS deadzone");
    }
    if (key == QLatin1String("ltsFalloffPx")) {
        return QStringLiteral("LTS falloff");
    }
    if (key == QLatin1String("ltsMaxNotchesPerSec")) {
        return QStringLiteral("LTS max speed");
    }
    if (key == QLatin1String("flashMs")) {
        return QStringLiteral("Flash duration");
    }
    if (key == QLatin1String("progressColor")) {
        return QStringLiteral("Progress color");
    }
    if (key == QLatin1String("progressFillColor")) {
        return QStringLiteral("Fill highlight");
    }
    if (key == QLatin1String("progressBorderColor")) {
        return QStringLiteral("Border highlight");
    }
    if (key == QLatin1String("mouseProgressColor")) {
        return QStringLiteral("Mouse progress color");
    }
    if (key == QLatin1String("mouseProgressFillColor")) {
        return QStringLiteral("Mouse fill color");
    }
    if (key == QLatin1String("mouseProgressBorderColor")) {
        return QStringLiteral("Mouse border color");
    }
    if (key == QLatin1String("flashBorderColor")) {
        return QStringLiteral("Flash border");
    }
    if (key == QLatin1String("flashFillColor")) {
        return QStringLiteral("Flash fill");
    }
    return key;
}

QString AppSettings::settingDescription(const QString& key)
{
    if (key == QLatin1String("dwellMs") || key == QLatin1String("dwellSequence")) {
        return QStringLiteral(
            "Comma-separated dwell times in ms while you keep gazing "
            "(e.g. 600,300,100,600). Steps advance until the last value, which "
            "then repeats forever. Replaces single dwell + repeat.");
    }
    if (key == QLatin1String("scanGraceMs")) {
        return QStringLiteral(
            "Time on-target before dwell progress begins (ms). Precursor to the "
            "dwell sequence; layout/item dwell.scanGraceMs can override.");
    }
    if (key == QLatin1String("dwellGraceMs")) {
        return QStringLiteral("Blink grace window without canceling dwell (ms).");
    }
    if (key == QLatin1String("mouseMoveDwellMs")) {
        return QStringLiteral("Dwell time for mouse cursor placement (ms).");
    }
    if (key == QLatin1String("mouseMoveSelectTimeoutMs")) {
        return QStringLiteral(
            "Cancel mouse-move / gaze-click loop if no target is selected within "
            "this many ms (0 = off). Resets after each successful selection in a loop.");
    }
    if (key == QLatin1String("magZoom")) {
        return QStringLiteral("Magnifier zoom factor (1.25–6).");
    }
    if (key == QLatin1String("magLensSize")) {
        return QStringLiteral("Lens diameter in pixels (160–900).");
    }
    if (key == QLatin1String("ltsDeadzonePx")) {
        return QStringLiteral("No-scroll radius around cursor (px).");
    }
    if (key == QLatin1String("ltsFalloffPx")) {
        return QStringLiteral("Distance to full scroll speed past deadzone (px).");
    }
    if (key == QLatin1String("ltsMaxNotchesPerSec")) {
        return QStringLiteral("Peak scroll rate (notches/second).");
    }
    if (key == QLatin1String("flashMs")) {
        return QStringLiteral("Completion flash duration after activation (ms).");
    }
    if (isColorKey(key)) {
        return QStringLiteral("Color used for dwell progress or completion flash.");
    }
    return {};
}

QString AppSettings::numericBufferSeed(const QString& key) const
{
    if (key == QLatin1String("dwellMs") || key == QLatin1String("dwellSequence")) {
        return dwellSequenceString();
    }
    if (key == QLatin1String("scanGraceMs")) {
        return QString::number(scanGraceMs);
    }
    if (key == QLatin1String("dwellGraceMs")) {
        return QString::number(dwellGraceMs);
    }
    if (key == QLatin1String("mouseMoveDwellMs")) {
        return QString::number(mouseMoveDwellMs);
    }
    if (key == QLatin1String("mouseMoveSelectTimeoutMs")) {
        return QString::number(mouseMoveSelectTimeoutMs);
    }
    if (key == QLatin1String("magZoom")) {
        return QString::number(magZoom, 'f', 2);
    }
    if (key == QLatin1String("magLensSize")) {
        return QString::number(magLensSize);
    }
    if (key == QLatin1String("ltsDeadzonePx")) {
        return QString::number(ltsDeadzonePx);
    }
    if (key == QLatin1String("ltsFalloffPx")) {
        return QString::number(ltsFalloffPx);
    }
    if (key == QLatin1String("ltsMaxNotchesPerSec")) {
        return QString::number(ltsMaxNotchesPerSec, 'f', 1);
    }
    if (key == QLatin1String("flashMs")) {
        return QString::number(flashMs);
    }
    return {};
}

bool AppSettings::applyNumericBuffer(const QString& key, const QString& buffer, QString* error)
{
    const QString b = buffer.trimmed();
    if (key == QLatin1String("dwellMs") || key == QLatin1String("dwellSequence")) {
        QString err;
        const QVector<int> seq = parseDwellSequence(b, &err);
        if (seq.isEmpty()) {
            if (error) {
                *error = err;
            }
            return false;
        }
        dwellSequence = seq;
        clamp();
        return true;
    }

    bool ok = false;
    if (key == QLatin1String("scanGraceMs") || key == QLatin1String("dwellGraceMs")
        || key == QLatin1String("mouseMoveDwellMs")
        || key == QLatin1String("mouseMoveSelectTimeoutMs")
        || key == QLatin1String("magLensSize") || key == QLatin1String("ltsDeadzonePx")
        || key == QLatin1String("ltsFalloffPx") || key == QLatin1String("flashMs")) {
        if (b.contains(QLatin1Char(',')) || b.contains(QLatin1Char('.'))) {
            if (error) {
                *error = QStringLiteral("Enter a whole number only");
            }
            return false;
        }
        const int v = b.toInt(&ok);
        if (!ok) {
            if (error) {
                *error = QStringLiteral("Enter a whole number");
            }
            return false;
        }
        if (key == QLatin1String("scanGraceMs")) {
            scanGraceMs = v;
        } else if (key == QLatin1String("dwellGraceMs")) {
            dwellGraceMs = v;
        } else if (key == QLatin1String("mouseMoveDwellMs")) {
            mouseMoveDwellMs = v;
        } else if (key == QLatin1String("mouseMoveSelectTimeoutMs")) {
            mouseMoveSelectTimeoutMs = v;
        } else if (key == QLatin1String("magLensSize")) {
            magLensSize = v;
        } else if (key == QLatin1String("ltsDeadzonePx")) {
            ltsDeadzonePx = v;
        } else if (key == QLatin1String("ltsFalloffPx")) {
            ltsFalloffPx = v;
        } else {
            flashMs = v;
        }
        clamp();
        return true;
    }
    if (key == QLatin1String("magZoom") || key == QLatin1String("ltsMaxNotchesPerSec")) {
        if (b.count(QLatin1Char('.')) > 1 || b.contains(QLatin1Char(','))) {
            if (error) {
                *error = QStringLiteral("Use a single decimal number (period, not comma)");
            }
            return false;
        }
        const double v = b.toDouble(&ok);
        if (!ok || v <= 0) {
            if (error) {
                *error = QStringLiteral("Enter a positive number");
            }
            return false;
        }
        if (key == QLatin1String("magZoom")) {
            magZoom = v;
        } else {
            ltsMaxNotchesPerSec = v;
        }
        clamp();
        return true;
    }
    if (error) {
        *error = QStringLiteral("Not a numeric setting");
    }
    return false;
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
    mouseMoveSelectTimeoutMs =
        o.value(QStringLiteral("mouseMoveSelectTimeoutMs")).toInt(mouseMoveSelectTimeoutMs);
    mouseMoveMagPick = o.value(QStringLiteral("mouseMoveMagPick")).toBool(mouseMoveMagPick);
    mouseMoveMagPickCenterOnDwell =
        o.value(QStringLiteral("mouseMoveMagPickCenterOnDwell")).toBool(mouseMoveMagPickCenterOnDwell);
    magZoom = o.value(QStringLiteral("magZoom")).toDouble(magZoom);
    magLensSize = o.value(QStringLiteral("magLensSize")).toInt(magLensSize);
    magFollowProfile = o.value(QStringLiteral("magFollowProfile")).toInt(magFollowProfile);
    ltsDeadzonePx = o.value(QStringLiteral("ltsDeadzonePx")).toInt(ltsDeadzonePx);
    ltsFalloffPx = o.value(QStringLiteral("ltsFalloffPx")).toInt(ltsFalloffPx);
    ltsMaxNotchesPerSec =
        o.value(QStringLiteral("ltsMaxNotchesPerSec")).toDouble(ltsMaxNotchesPerSec);
    ltsAccelPerSec = o.value(QStringLiteral("ltsAccelPerSec")).toDouble(ltsAccelPerSec);
    ltsCenterDwellMs = o.value(QStringLiteral("ltsCenterDwellMs")).toInt(ltsCenterDwellMs);
    ltsPlaceCursorFirst =
        o.value(QStringLiteral("ltsPlaceCursorFirst")).toBool(ltsPlaceCursorFirst);
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
    mouseProgressColor = o.value(QStringLiteral("mouseProgressColor")).toString(mouseProgressColor);
    mouseProgressFillColor =
        o.value(QStringLiteral("mouseProgressFillColor")).toString(mouseProgressFillColor);
    mouseProgressBorderColor =
        o.value(QStringLiteral("mouseProgressBorderColor")).toString(mouseProgressBorderColor);
    flashOnComplete = o.value(QStringLiteral("flashOnComplete")).toBool(flashOnComplete);
    flashBorderColor = o.value(QStringLiteral("flashBorderColor")).toString(flashBorderColor);
    flashFillColor = o.value(QStringLiteral("flashFillColor")).toString(flashFillColor);
    flashMs = o.value(QStringLiteral("flashMs")).toInt(flashMs);

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
    o.insert(QStringLiteral("mouseMoveSelectTimeoutMs"), copy.mouseMoveSelectTimeoutMs);
    o.insert(QStringLiteral("mouseMoveMagPick"), copy.mouseMoveMagPick);
    o.insert(QStringLiteral("mouseMoveMagPickCenterOnDwell"), copy.mouseMoveMagPickCenterOnDwell);
    o.insert(QStringLiteral("magZoom"), copy.magZoom);
    o.insert(QStringLiteral("magLensSize"), copy.magLensSize);
    o.insert(QStringLiteral("magFollowProfile"), copy.magFollowProfile);
    o.insert(QStringLiteral("ltsDeadzonePx"), copy.ltsDeadzonePx);
    o.insert(QStringLiteral("ltsFalloffPx"), copy.ltsFalloffPx);
    o.insert(QStringLiteral("ltsMaxNotchesPerSec"), copy.ltsMaxNotchesPerSec);
    o.insert(QStringLiteral("ltsAccelPerSec"), copy.ltsAccelPerSec);
    o.insert(QStringLiteral("ltsCenterDwellMs"), copy.ltsCenterDwellMs);
    o.insert(QStringLiteral("ltsPlaceCursorFirst"), copy.ltsPlaceCursorFirst);
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
    o.insert(QStringLiteral("progressRadial"), copy.progressRadial);
    o.insert(QStringLiteral("progressFill"), copy.progressFill);
    o.insert(QStringLiteral("progressBorder"), copy.progressBorder);
    o.insert(QStringLiteral("progressColor"), copy.progressColor);
    o.insert(QStringLiteral("progressFillColor"), copy.progressFillColor);
    o.insert(QStringLiteral("progressBorderColor"), copy.progressBorderColor);
    o.insert(QStringLiteral("mouseProgressRadial"), copy.mouseProgressRadial);
    o.insert(QStringLiteral("mouseProgressFill"), copy.mouseProgressFill);
    o.insert(QStringLiteral("mouseProgressBorder"), copy.mouseProgressBorder);
    o.insert(QStringLiteral("mouseProgressColor"), copy.mouseProgressColor);
    o.insert(QStringLiteral("mouseProgressFillColor"), copy.mouseProgressFillColor);
    o.insert(QStringLiteral("mouseProgressBorderColor"), copy.mouseProgressBorderColor);
    o.insert(QStringLiteral("flashOnComplete"), copy.flashOnComplete);
    o.insert(QStringLiteral("flashBorderColor"), copy.flashBorderColor);
    o.insert(QStringLiteral("flashFillColor"), copy.flashFillColor);
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
