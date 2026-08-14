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

namespace {

struct IntSpec {
    const char* key;
    const char* title;
    const char* hint;
    const char* suffix;
    int AppSettings::* member;
    int min;
    int max;
    int step;
};

struct DoubleSpec {
    const char* key;
    const char* title;
    const char* hint;
    const char* suffix;
    double AppSettings::* member;
    double min;
    double max;
    double step;
    int decimals;
};

struct ColorSpec {
    const char* key;
    const char* title;
    QString AppSettings::* member;
    QColor fallback;
};

struct BoolSpec {
    const char* key;
    bool AppSettings::* member;
};

constexpr IntSpec kIntSpecs[] = {
    {"scanGraceMs", "Scan grace",
     "Time on-target before dwell progress begins (ms).", " ms",
     &AppSettings::scanGraceMs, 0, 2000, 20},
    {"dwellGraceMs", "Blink grace",
     "Blink grace window without canceling dwell (ms).", " ms",
     &AppSettings::dwellGraceMs, 0, 800, 20},
    {"mouseMoveDwellMs", "Mouse-move dwell",
     "Dwell time for mouse cursor placement (ms).", " ms",
     &AppSettings::mouseMoveDwellMs, 200, 2500, 50},
    {"mouseMoveSelectTimeoutMs", "Mouse-move timeout",
     "Cancel mouse-move / gaze-click loop if no target is selected within this many ms (0 = off).",
     " ms", &AppSettings::mouseMoveSelectTimeoutMs, 0, 120000, 500},
    {"magLensSize", "Lens size", "Lens diameter in pixels (160–900).", " px",
     &AppSettings::magLensSize, 160, 900, 20},
    {"ltsDeadzonePx", "LTS deadzone", "No-scroll radius around cursor (px).", " px",
     &AppSettings::ltsDeadzonePx, 30, 400, 10},
    {"ltsFalloffPx", "LTS falloff", "Distance to full scroll speed past deadzone (px).", " px",
     &AppSettings::ltsFalloffPx, 80, 800, 20},
    {"ltsCenterDwellMs", "LTS center dwell", "Dwell in the deadzone center to pause/resume (ms).",
     " ms", &AppSettings::ltsCenterDwellMs, 200, 2500, 50},
    {"flashMs", "Flash duration", "Completion flash duration after activation (ms).", " ms",
     &AppSettings::flashMs, 40, 1000, 20},
};

constexpr DoubleSpec kDoubleSpecs[] = {
    {"magZoom", "Magnifier zoom", "Magnifier zoom factor (1.25–6).", "×",
     &AppSettings::magZoom, 1.25, 6.0, 0.25, 2},
    {"ltsMaxNotchesPerSec", "LTS max speed", "Peak scroll rate (notches/second).", " n/s",
     &AppSettings::ltsMaxNotchesPerSec, 0.5, 24.0, 0.5, 1},
    {"ltsAccelPerSec", "LTS accel/s", "Speed growth while outside deadzone.", " /s",
     &AppSettings::ltsAccelPerSec, 0.0, 2.0, 0.05, 2},
};

const ColorSpec kColorSpecs[] = {
    {"progressColor", "Progress color", &AppSettings::progressColor,
     ThemeColors::defaultProgressColor()},
    {"progressFillColor", "Fill highlight", &AppSettings::progressFillColor,
     QColor(0, 180, 220, 70)},
    {"progressBorderColor", "Border highlight", &AppSettings::progressBorderColor,
     ThemeColors::defaultProgressColor()},
    {"flashColor", "Flash color", &AppSettings::flashColor, Qt::white},
};

constexpr BoolSpec kBoolSpecs[] = {
    {"ltsPlaceCursorFirst", &AppSettings::ltsPlaceCursorFirst},
    {"autoCollapseMain", &AppSettings::autoCollapseMain},
    {"startDocked", &AppSettings::startDocked},
    {"layoutAutoClose", &AppSettings::layoutAutoClose},
    {"speakAlsoType", &AppSettings::speakAlsoType},
    {"progressRadial", &AppSettings::progressRadial},
    {"progressFill", &AppSettings::progressFill},
    {"progressBorder", &AppSettings::progressBorder},
    {"mouseProgressRadial", &AppSettings::mouseProgressRadial},
    {"mouseProgressFill", &AppSettings::mouseProgressFill},
    {"mouseProgressBorder", &AppSettings::mouseProgressBorder},
    {"flashOnComplete", &AppSettings::flashOnComplete},
};

bool keyEq(const char* a, const QString& b)
{
    return b == QLatin1String(a);
}

const IntSpec* findInt(const QString& key)
{
    for (const IntSpec& s : kIntSpecs) {
        if (keyEq(s.key, key)) {
            return &s;
        }
    }
    return nullptr;
}

const DoubleSpec* findDouble(const QString& key)
{
    for (const DoubleSpec& s : kDoubleSpecs) {
        if (keyEq(s.key, key)) {
            return &s;
        }
    }
    return nullptr;
}

const ColorSpec* findColor(const QString& key)
{
    if (key == QLatin1String("flashBorderColor") || key == QLatin1String("flashFillColor")) {
        return findColor(QStringLiteral("flashColor"));
    }
    for (const ColorSpec& s : kColorSpecs) {
        if (keyEq(s.key, key)) {
            return &s;
        }
    }
    return nullptr;
}

bool isSequenceKey(const QString& key)
{
    return key == QLatin1String("dwellMs") || key == QLatin1String("dwellSequence");
}

} // namespace


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
        dwellSequence = defaultDwellSequence();
    }
    for (int& ms : dwellSequence) {
        ms = qBound(50, ms, 10000);
    }
    for (const IntSpec& s : kIntSpecs) {
        this->*s.member = qBound(s.min, this->*s.member, s.max);
    }
    for (const DoubleSpec& s : kDoubleSpecs) {
        this->*s.member = qBound(s.min, this->*s.member, s.max);
    }
    magFollowProfile = qBound(0, magFollowProfile, 2);
    trackerPref = qBound(0, trackerPref, 1);
    layoutAutoCloseIdleMs = qBound(500, layoutAutoCloseIdleMs, 120000);
    layoutAutoCloseFadeMs = qBound(50, layoutAutoCloseFadeMs, 60000);
    if (!progressRadial && !progressFill && !progressBorder) {
        progressRadial = true;
    }
    if (!mouseProgressRadial && !mouseProgressFill && !mouseProgressBorder) {
        mouseProgressRadial = true;
    }
}

bool AppSettings::nudge(const QString& key, int dir)
{
    if (isSequenceKey(key)) {
        if (dwellSequence.isEmpty()) {
            dwellSequence = defaultDwellSequence();
        }
        dwellSequence[0] = qBound(50, dwellSequence[0] + dir * 50, 10000);
        return true;
    }
    if (const IntSpec* s = findInt(key)) {
        this->*s->member = qBound(s->min, this->*s->member + dir * s->step, s->max);
        return true;
    }
    if (const DoubleSpec* s = findDouble(key)) {
        this->*s->member = qBound(s->min, this->*s->member + dir * s->step, s->max);
        return true;
    }
    return false;
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
        dwellSequence = defaultDwellSequence();
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
    return isSequenceKey(key) || findInt(key) || findDouble(key);
}

QStringList AppSettings::numericKeys()
{
    QStringList keys;
    keys << QStringLiteral("dwellMs");
    for (const IntSpec& s : kIntSpecs) {
        keys << QLatin1String(s.key);
    }
    for (const DoubleSpec& s : kDoubleSpecs) {
        keys << QLatin1String(s.key);
    }
    return keys;
}

QColor AppSettings::colorKey(const QString& key) const
{
    if (const ColorSpec* s = findColor(key)) {
        return parseColor(this->*s->member, s->fallback);
    }
    return ThemeColors::defaultProgressColor();
}

bool AppSettings::setColorKey(const QString& key, const QColor& c)
{
    if (!c.isValid()) {
        return false;
    }
    const ColorSpec* s = findColor(key);
    if (!s) {
        return false;
    }
    this->*s->member = colorToHex(c);
    return true;
}

QString AppSettings::displayValue(const QString& key) const
{
    if (isSequenceKey(key)) {
        return dwellSequenceString() + QStringLiteral(" ms");
    }
    if (const IntSpec* s = findInt(key)) {
        const int v = this->*s->member;
        if (keyEq(s->key, QStringLiteral("mouseMoveSelectTimeoutMs")) && v <= 0) {
            return QStringLiteral("Off");
        }
        return QString::number(v) + QLatin1String(s->suffix);
    }
    if (const DoubleSpec* s = findDouble(key)) {
        return QString::number(this->*s->member, 'f', s->decimals) + QLatin1String(s->suffix);
    }
    if (key == QLatin1String("magFollowProfile")) {
        static const char* names[] = {"Sticky", "Balanced", "Snappy"};
        return QLatin1String(names[qBound(0, magFollowProfile, 2)]);
    }
    for (const BoolSpec& s : kBoolSpecs) {
        if (keyEq(s.key, key)) {
            return (this->*s.member) ? QStringLiteral("ON") : QStringLiteral("OFF");
        }
    }
    if (key == QLatin1String("trackerPref")) {
        return trackerPref == 1 ? QStringLiteral("Mouse only") : QStringLiteral("Auto Tobii");
    }
    if (isColorKey(key)) {
        return colorKey(key).name(QColor::HexArgb).toUpper();
    }
    return {};
}

QString AppSettings::settingTitle(const QString& key)
{
    if (isSequenceKey(key)) {
        return QStringLiteral("Dwell sequence");
    }
    if (const IntSpec* s = findInt(key)) {
        return QLatin1String(s->title);
    }
    if (const DoubleSpec* s = findDouble(key)) {
        return QLatin1String(s->title);
    }
    if (const ColorSpec* s = findColor(key)) {
        return QLatin1String(s->title);
    }
    return key;
}

QString AppSettings::settingDescription(const QString& key)
{
    if (isSequenceKey(key)) {
        return QStringLiteral(
            "Comma-separated dwell times in ms while you keep gazing "
            "(e.g. 600,300,100,600). Steps advance until the last value, which "
            "then repeats forever.");
    }
    if (const IntSpec* s = findInt(key)) {
        return QLatin1String(s->hint);
    }
    if (const DoubleSpec* s = findDouble(key)) {
        return QLatin1String(s->hint);
    }
    if (key == QLatin1String("flashColor")) {
        return QStringLiteral("Color applied to both the flash border and fill.");
    }
    if (isColorKey(key)) {
        return QStringLiteral("Color used for dwell progress or completion flash.");
    }
    return {};
}

QString AppSettings::numericBufferSeed(const QString& key) const
{
    if (isSequenceKey(key)) {
        return dwellSequenceString();
    }
    if (const IntSpec* s = findInt(key)) {
        return QString::number(this->*s->member);
    }
    if (const DoubleSpec* s = findDouble(key)) {
        return QString::number(this->*s->member, 'f', s->decimals);
    }
    return {};
}

bool AppSettings::applyNumericBuffer(const QString& key, const QString& buffer, QString* error)
{
    const QString b = buffer.trimmed();
    if (isSequenceKey(key)) {
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
    if (const IntSpec* s = findInt(key)) {
        if (b.contains(QLatin1Char(',')) || b.contains(QLatin1Char('.'))) {
            if (error) {
                *error = QStringLiteral("Enter a whole number only");
            }
            return false;
        }
        bool ok = false;
        const int v = b.toInt(&ok);
        if (!ok) {
            if (error) {
                *error = QStringLiteral("Enter a whole number");
            }
            return false;
        }
        this->*s->member = v;
        clamp();
        return true;
    }
    if (const DoubleSpec* s = findDouble(key)) {
        if (b.count(QLatin1Char('.')) > 1 || b.contains(QLatin1Char(','))) {
            if (error) {
                *error = QStringLiteral("Use a single decimal number (period, not comma)");
            }
            return false;
        }
        bool ok = false;
        const double v = b.toDouble(&ok);
        if (!ok) {
            if (error) {
                *error = QStringLiteral("Enter a number");
            }
            return false;
        }
        this->*s->member = v;
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
    flashOnComplete = o.value(QStringLiteral("flashOnComplete")).toBool(flashOnComplete);
    if (o.contains(QStringLiteral("flashColor"))) {
        flashColor = o.value(QStringLiteral("flashColor")).toString(flashColor);
    } else if (o.contains(QStringLiteral("flashBorderColor"))) {
        flashColor = o.value(QStringLiteral("flashBorderColor")).toString(flashColor);
    } else if (o.contains(QStringLiteral("flashFillColor"))) {
        flashColor = o.value(QStringLiteral("flashFillColor")).toString(flashColor);
    }
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
    o.insert(QStringLiteral("flashOnComplete"), copy.flashOnComplete);
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
