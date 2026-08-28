#include "app/AppSettings.h"

#include "assist/GazeFollowProfile.h"
#include "assist/LtsIndicator.h"
#include "assist/LtsSpeed.h"
#include "ui/PickStyle.h"



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
    double (*snap)(double) = nullptr;
    double (*nudge)(double, int) = nullptr;
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
    {"ltsCenterDwellMs", "LTS center dwell",
     "Dwell the hub to pause and open the plus menu (ms).",
     " ms", &AppSettings::ltsCenterDwellMs, 200, 2500, 50},
    {"comboInnerRadiusPx", "ComboMouse inner radius",
     "Inner edge of the drift ring (px). Hole / deadzone.", " px",
     &AppSettings::comboInnerRadiusPx, ComboMouseHit::kMinInnerRadiusPx,
     ComboMouseHit::kMaxInnerRadiusPx, 8},
    {"comboSharedRadiusPx", "ComboMouse shared radius",
     "Where the drift ring meets the command pie (px).", " px",
     &AppSettings::comboSharedRadiusPx, ComboMouseHit::kMinSharedRadiusPx,
     ComboMouseHit::kMaxSharedRadiusPx, 8},
    {"comboOuterRadiusPx", "ComboMouse outer radius",
     "Outer edge of the command pie (px).", " px",
     &AppSettings::comboOuterRadiusPx, ComboMouseHit::kMinOuterRadiusPx,
     ComboMouseHit::kMaxOuterRadiusPx, 8},
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
    {"comboInnerColor", "ComboMouse inner ring", &AppSettings::comboInnerColor,
     ComboMouseHit::kDefaultInnerFill},
    {"comboOuterColor", "ComboMouse outer ring", &AppSettings::comboOuterColor,
     ComboMouseHit::kDefaultOuterFill},
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
        double v = qBound(s.min, this->*s.member, s.max);
        this->*s.member = s.snap ? s.snap(v) : v;
    }
    magFollowProfile = gazeFollowProfileFromInt(int(magFollowProfile));
    ltsIndicatorStyle = ltsIndicatorFromInt(int(ltsIndicatorStyle));
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
    ltsMaxNotchesPerSec = snapLtsSpeed(qBound(1.0, ltsMaxNotchesPerSec, 40.0));

    double inner = comboInnerRadiusPx;
    double shared = comboSharedRadiusPx;
    double outer = comboOuterRadiusPx;
    ComboMouseHit::clampRadii(inner, shared, outer);
    comboInnerRadiusPx = qRound(inner);
    comboSharedRadiusPx = qRound(shared);
    comboOuterRadiusPx = qRound(outer);
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
        clamp();
        return true;
    }
    if (const DoubleSpec* s = findDouble(key)) {
        if (s->nudge) {
            this->*s->member = s->nudge(this->*s->member, dir);
            clamp();
            return true;
        }
        this->*s->member = qBound(s->min, this->*s->member + dir * s->step, s->max);
        clamp();
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
    magFollowProfile = gazeFollowProfileFromInt(profile);
}

void AppSettings::setLtsIndicatorStyle(int style)
{
    ltsIndicatorStyle = ltsIndicatorFromInt(style);
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
        return QLatin1String(gazeFollowProfileName(magFollowProfile));
    }
    if (key == QLatin1String("ltsIndicatorStyle")) {
        return QLatin1String(ltsIndicatorName(ltsIndicatorStyle));
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
    if (key == QLatin1String("ltsIndicatorStyle")) {
        return QStringLiteral("LTS indicator");
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
    if (key == QLatin1String("comboInnerColor")) {
        return QStringLiteral("Fill color and opacity of the ComboMouse drift ring.");
    }
    if (key == QLatin1String("comboOuterColor")) {
        return QStringLiteral("Fill color and opacity of the ComboMouse command slices.");
    }
    if (isColorKey(key)) {
        return QStringLiteral("Color used for dwell progress or completion flash.");
    }
    if (key == QLatin1String("ltsIndicatorStyle")) {
        return QStringLiteral(
            "Look-to-scroll overlay: Fan (deadzone + wedge), Orb (glow that stretches "
            "in the scroll direction), or Pause (center pause/resume only).");
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

} // namespace gazer
