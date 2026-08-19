#include "app/AppSettings.h"

#include "ui/PickStyle.h"
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
    {"mouseMoveDwellMs", "Pointer dwell",
     "Dwell time for the final cursor / click placement (ms).", " ms",
     &AppSettings::mouseMoveDwellMs, 200, 2500, 50},
    {"magPickDwellMs", "Zoom dwell",
     "Dwell time to choose the region to magnify (ms).", " ms",
     &AppSettings::magPickDwellMs, 200, 2500, 50},
    {"mouseMoveForesightDwellMs", "Foresight dwell",
     "Dwell time to store a foresight point before Move-to (ms).", " ms",
     &AppSettings::mouseMoveForesightDwellMs, 100, 2500, 50},
    {"mouseMoveSelectTimeoutMs", "Pointer grace",
     "Cancel pointer aim if no target is selected within this many ms (0 = off).",
     " ms", &AppSettings::mouseMoveSelectTimeoutMs, 0, 120000, 500},
    {"magLensSize", "Lens size", "Live lens diameter in pixels (160–900).", " px",
     &AppSettings::magLensSize, 160, 900, 20},
    {"pickWindowPx", "Zoom size",
     "Static zoom window size for magnify and foresight (px).", " px",
     &AppSettings::pickWindowPx, 200, 1600, 40},
    {"ltsDeadzonePx", "LTS deadzone", "No-scroll radius around cursor (px).", " px",
     &AppSettings::ltsDeadzonePx, 30, 400, 10},
    {"ltsFalloffPx", "LTS falloff", "Distance to full scroll speed past deadzone (px).", " px",
     &AppSettings::ltsFalloffPx, 80, 800, 20},
    {"ltsCenterDwellMs", "LTS center dwell", "Dwell in the deadzone center to pause/resume (ms).",
     " ms", &AppSettings::ltsCenterDwellMs, 200, 2500, 50},
    {"flashMs", "Completion flash duration", "How long the completion flash is shown (ms).", " ms",
     &AppSettings::flashMs, 40, 1000, 20},
    {"flashForegroundOpacity", "Flash opacity",
     "Opacity of the completion flash when using the item foreground color.", "%",
     &AppSettings::flashForegroundOpacity, 0, 100, 5},
};

constexpr DoubleSpec kDoubleSpecs[] = {
    {"magZoom", "Lens zoom", "Live lens magnification (1.25–6). Not used by pick zoom.", "",
     &AppSettings::magZoom, 1.25, 6.0, 0.25, 2},
    {"pickZoom", "Zoom level",
     "Static magnification for magnify and foresight (1.25–8).", "",
     &AppSettings::pickZoom, 1.25, 8.0, 0.25, 2},
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
    {"customBgColor", "Background", &AppSettings::customBgColor, QColor(10, 10, 11)},
    {"customPrimaryColor", "Primary", &AppSettings::customPrimaryColor, QColor(138, 180, 248)},
    {"customSecondaryColor", "Secondary", &AppSettings::customSecondaryColor,
     ThemeColors::defaultProgressColor()},
    {"customTertiaryColor", "Tertiary", &AppSettings::customTertiaryColor, QColor(126, 82, 96)},
    {"customSurfaceColor", "Surface", &AppSettings::customSurfaceColor, QColor(18, 19, 20)},
    {"customTextColor", "Foreground", &AppSettings::customTextColor, QColor(230, 225, 229)},
    {"customDangerColor", "Danger", &AppSettings::customDangerColor, QColor(255, 180, 171)},
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
    {"flashUseForeground", &AppSettings::flashUseForeground},
    {"pickWindowRound", &AppSettings::pickWindowRound},
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
    customContrast = snapContrastPercent(customContrast);
    magPickStyle = PickStyle::sanitizeMag(magPickStyle);
    mousePickStyle = PickStyle::sanitizeMouse(mousePickStyle);
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
        magPickDwellMs = 900;
        dwellGraceMs = 220;
        scanGraceMs = 150;
        break;
    case 2:
        dwellSequence = {450};
        mouseMoveDwellMs = 500;
        magPickDwellMs = 500;
        dwellGraceMs = 140;
        scanGraceMs = 80;
        break;
    default:
        dwellSequence = defaultDwellSequence();
        mouseMoveDwellMs = 700;
        magPickDwellMs = 700;
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

bool AppSettings::setColorKey(const QString& key, const QColor& c, bool rebuildPalette)
{
    if (!c.isValid()) {
        return false;
    }
    const ColorSpec* s = findColor(key);
    if (!s) {
        return false;
    }
    this->*s->member = colorToHex(c);
    if (key == QLatin1String("progressColor")) {
        customSecondaryColor = colorToHex(c);
    } else if (key == QLatin1String("customSecondaryColor")) {
        progressColor = colorToHex(c);
    }
    if (!rebuildPalette || themeRoleForColorKey(key).isEmpty()) {
        return true;
    }
    applyCustomPalette(false);
    return true;
}

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
        const int places = (keyEq(s->key, QStringLiteral("magZoom"))
                            || keyEq(s->key, QStringLiteral("pickZoom")))
                               ? 2
                               : s->decimals;
        return QString::number(this->*s->member, 'f', places) + QLatin1String(s->suffix);
    }
    if (key == QLatin1String("magFollowProfile")) {
        static const char* names[] = {"Sticky", "Balanced", "Snappy"};
        return QLatin1String(names[qBound(0, magFollowProfile, 2)]);
    }
    if (key == QLatin1String("customContrast")) {
        return themeContrastToString(themeContrastFromInt(customContrast));
    }

    if (key == QLatin1String("magPickStyle")) {
        return PickStyle::label(magPickStyle);
    }
    if (key == QLatin1String("mousePickStyle")) {
        return PickStyle::label(mousePickStyle);
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
        return QStringLiteral("Custom color for the completion flash border and fill.");
    }
    if (key == QLatin1String("flashUseForeground")) {
        return QStringLiteral("Flash the activating item's foreground color.");
    }
    if (key == QLatin1String("customBgColor")) {
        return QStringLiteral("Page background. Variant, foreground, and accent suggestions come from this.");
    }
    if (key == QLatin1String("customPrimaryColor")) {
        return QStringLiteral("Highlighted foreground (active labels, accent).");
    }
    if (key == QLatin1String("customSecondaryColor")) {
        return QStringLiteral("Progress ring / fill / border color.");
    }
    if (key == QLatin1String("customTertiaryColor")) {
        return QStringLiteral("Highlighted background (active cells).");
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
        const int places = (keyEq(s->key, QStringLiteral("magZoom"))
                            || keyEq(s->key, QStringLiteral("pickZoom")))
                               ? 2
                               : s->decimals;
        return QString::number(this->*s->member, 'f', places);
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
