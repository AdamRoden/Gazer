#include "app/AppSettings.h"

#include "assist/GazeFollowProfile.h"
#include "mapping/HeadPoseCurve.h"
#include "ui/PickStyle.h"

#include <QSet>
#include <QStringList>
#include <QUuid>
#include <utility>

namespace gazer {

AppSettings AppSettings::defaults()
{
    AppSettings s;
    s.applyTheme();
    return s;
}

HeadPoseMap AppSettings::makeDefaultHeadPoseMap()
{
    HeadPoseMap m = defaultHeadPoseMap();
    m.id = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    return m;
}

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

const AppSettings::TimingPack kDwellSlow{
    {1200, 1000, 800, 600, 400},
    {800, 700, 600, 500, 400, 200},
    1200,
    800,
    250,
};
const AppSettings::TimingPack kDwellNormal = AppSettings::defaultTimingPack();
const AppSettings::TimingPack kDwellFast{
    {400, 600, 400, 250, 150, 50},
    {100, 600, 400, 250, 150, 50},
    400,
    300,
    150,
};

void clampSequence(QVector<int>& seq, const QVector<int>& fallback)
{
    if (seq.isEmpty()) {
        seq = fallback;
    }
    for (int& ms : seq) {
        ms = qBound(0, ms, 10000);
    }
}

AppSettings::TimingPack liveTiming(const AppSettings& s)
{
    return {s.dwellSequence, s.rapidDwellSequence, s.mouseMoveDwellMs, s.magPickDwellMs,
            s.dwellGraceMs};
}

void applyTimingPack(AppSettings& s, const AppSettings::TimingPack& p)
{
    s.dwellSequence = p.sequence;
    s.rapidDwellSequence = p.rapidSequence;
    s.mouseMoveDwellMs = p.mouseMoveDwellMs;
    s.magPickDwellMs = p.magPickDwellMs;
    s.dwellGraceMs = p.blinkGraceMs;
}

bool matchesTimingPack(const AppSettings& s, const AppSettings::TimingPack& p)
{
    const AppSettings::TimingPack live = liveTiming(s);
    return live.sequence == p.sequence && live.rapidSequence == p.rapidSequence
           && live.mouseMoveDwellMs == p.mouseMoveDwellMs && live.magPickDwellMs == p.magPickDwellMs
           && live.blinkGraceMs == p.blinkGraceMs;
}

void clampTimingPack(AppSettings::TimingPack& p)
{
    clampSequence(p.sequence, AppSettings::defaultDwellSequence());
    clampSequence(p.rapidSequence, AppSettings::defaultRapidDwellSequence());
    p.blinkGraceMs = qBound(0, p.blinkGraceMs, 800);
    p.mouseMoveDwellMs = qBound(200, p.mouseMoveDwellMs, 2500);
    p.magPickDwellMs = qBound(200, p.magPickDwellMs, 2500);
}

QVector<int>& sequenceField(AppSettings& s, const QString& key)
{
    return AppSettings::isRapidSequenceKey(key) ? s.rapidDwellSequence : s.dwellSequence;
}

const QVector<int>& sequenceField(const AppSettings& s, const QString& key)
{
    return AppSettings::isRapidSequenceKey(key) ? s.rapidDwellSequence : s.dwellSequence;
}

QString formatSequence(const QVector<int>& seq)
{
    QStringList parts;
    for (int ms : seq) {
        parts << QString::number(ms);
    }
    return parts.join(QLatin1Char(','));
}

constexpr IntSpec kIntSpecs[] = {
    {"dwellGraceMs", "Blink grace",
     "Blink grace window without canceling dwell (ms).", " ms",
     &AppSettings::dwellGraceMs, 0, 800, 20},
    {"scanGraceMs", "Scan grace",
     "Time on-target before dwell progress begins (ms).", " ms",
     &AppSettings::scanGraceMs, 0, 2000, 20},
    {"mouseMoveDwellMs", "Pointer dwell",
     "Dwell time for the final cursor / click placement (ms).", " ms",
     &AppSettings::mouseMoveDwellMs, 200, 2500, 50},
    {"magPickDwellMs", "Zoom dwell",
     "Dwell time to choose the region to magnify (ms).", " ms",
     &AppSettings::magPickDwellMs, 200, 2500, 50},
    {"mouseMoveForesightDwellMs", "Foresight dwell",
     "Dwell time to store a foresight point before Move-to (ms).", " ms",
     &AppSettings::mouseMoveForesightDwellMs, 100, 2500, 50},
    {"mouseMoveForesightHoldMs", "Foresight hold",
     "How long a stored foresight point stays valid (ms).", " ms",
     &AppSettings::mouseMoveForesightHoldMs, 200, 30000, 100},
    {"mouseMoveSelectTimeoutMs", "Pointer grace",
     "Extra time after zoom dwell (pre-pick) or pointer dwell (point pick) before aim cancels (0 = off).",
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
     "Dwell the hub to pause and open the Look-to-scroll pie (ms).",
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
    {"hoverBorderWeight", "Hover border", "Outline thickness while gazing at a cell (px).", " px",
     &AppSettings::hoverBorderWeight, 0, 16, 1},
    {"layoutAutoCloseIdleMs", "Auto-close idle",
     "Close idle boards after this many ms.", " ms",
     &AppSettings::layoutAutoCloseIdleMs, 500, 120000, 500},
    {"flashForegroundOpacity", "Flash opacity",
     "Opacity of the completion flash when using the item foreground color.", "%",
     &AppSettings::flashForegroundOpacity, 0, 100, 5},
    {"themeSaturation", "Saturation",
     "How colorful accent and progress are (five steps). Surfaces keep their brightness.", "",
     &AppSettings::themeSaturation, kThemeSaturationMin, kThemeSaturationMax,
     kThemeSaturationStep},
};

constexpr DoubleSpec kDoubleSpecs[] = {
    {"magZoom", "Lens zoom", "Live lens magnification (1.25–6). Not used by pick zoom.", "",
     &AppSettings::magZoom, 1.25, 6.0, 0.25, 2},
    {"pickZoom", "Zoom level",
     "Static magnification for magnify and foresight (1.25–8).", "",
     &AppSettings::pickZoom, 1.25, 8.0, 0.25, 2},
    {"ltsAccelPerSec", "LTS accel/s",
     "Speed growth per second while that axis is contributing. Resets when the axis is ~0.", " /s",
     &AppSettings::ltsAccelPerSec, kLtsAccelMin, kLtsAccelMax, 0.5, 1},
    {"speechSpeed", "Speech speed", "ElevenLabs and SAPI speed (0.5–2).", "",
     &AppSettings::speechSpeed, 0.5, 2.0, 0.1, 2},
    {"speechVolume", "Speech boost", "Make composer voices louder (1–5×). Applies to ElevenLabs clips.",
     "×", &AppSettings::speechVolume, 1.0, 5.0, 0.5, 1},
};

const ColorSpec kColorSpecs[] = {
    {"progressColor", "Progress color", &AppSettings::progressColor,
     ThemeColors::defaultProgressColor()},
    {"progressFillColor", "Fill highlight", &AppSettings::progressFillColor,
     QColor(0, 180, 220, 70)},
    {"hoverColor", "Hover color", &AppSettings::hoverColor, ThemeColors::defaultProgressColor()},
    {"flashColor", "Flash color", &AppSettings::flashColor, Qt::white},
    {"comboInnerColor", "ComboMouse inner ring", &AppSettings::comboInnerColor,
     ComboMouseHit::kDefaultInnerFill},
    {"comboOuterColor", "ComboMouse outer ring", &AppSettings::comboOuterColor,
     ComboMouseHit::kDefaultOuterFill},
    {"customSourceColor", "Source", &AppSettings::customSourceColor, QColor(0x1E, 0x97, 0xF3)},
    {"customPrimaryColor", "Accent", &AppSettings::customPrimaryColor, QColor(96, 205, 255)},
    {"customSecondaryColor", "Progress", &AppSettings::customSecondaryColor,
     ThemeColors::defaultProgressColor()},
    {"customTextColor", "Foreground", &AppSettings::customTextColor, QColor(230, 225, 229)},
    {"customDangerColor", "Danger", &AppSettings::customDangerColor, QColor(255, 180, 171)},
};

constexpr BoolSpec kBoolSpecs[] = {
    {"autoCollapseMain", &AppSettings::autoCollapseMain},
    {"startDocked", &AppSettings::startDocked},
    {"layoutAutoClose", &AppSettings::layoutAutoClose},
    {"flashCustom", &AppSettings::flashCustom},
    {"hoverCustom", &AppSettings::hoverCustom},
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
    for (const ColorSpec& s : kColorSpecs) {
        if (keyEq(s.key, key)) {
            return &s;
        }
    }
    return nullptr;
}

} // namespace

bool AppSettings::isRapidSequenceKey(const QString& key)
{
    return key == QLatin1String("rapidDwellMs") || key == QLatin1String("rapidDwellSequence");
}

bool AppSettings::isSequenceKey(const QString& key)
{
    return key == QLatin1String("dwellMs") || key == QLatin1String("dwellSequence")
           || isRapidSequenceKey(key);
}

const AppSettings::StyleToggle* AppSettings::findStyleToggle(const QString& jsonKey)
{
    for (const StyleToggle& t : kStyleToggles) {
        if (jsonKey == QLatin1String(t.jsonKey)) {
            return &t;
        }
    }
    return nullptr;
}

QString AppSettings::normalizeSpeechTag(QString raw)
{
    raw = raw.trimmed();
    if (raw.startsWith(QLatin1Char('[')) && raw.endsWith(QLatin1Char(']')) && raw.size() >= 2) {
        raw = raw.mid(1, raw.size() - 2).trimmed();
    }
    raw.remove(QLatin1Char('['));
    raw.remove(QLatin1Char(']'));
    return raw.simplified();
}

void AppSettings::clamp()
{
    clampSequence(dwellSequence, defaultDwellSequence());
    clampSequence(rapidDwellSequence, defaultRapidDwellSequence());
    clampTimingPack(customTiming);
    for (const IntSpec& s : kIntSpecs) {
        this->*s.member = qBound(s.min, this->*s.member, s.max);
    }
    for (const DoubleSpec& s : kDoubleSpecs) {
        double v = qBound(s.min, this->*s.member, s.max);
        this->*s.member = s.snap ? s.snap(v) : v;
    }
    magFollowProfile = gazeFollowProfileFromInt(int(magFollowProfile));
    ltsIndicatorStyle = ltsIndicatorFromInt(int(ltsIndicatorStyle));
    ltsScrollMode = ltsScrollModeFromInt(int(ltsScrollMode));
    themeSaturation = snapThemeSaturation(themeSaturation);
    themeBrightness = qBound(kThemeBrightnessMin, themeBrightness, kThemeBrightnessMax);
    magPickStyle = PickStyle::sanitizeMag(magPickStyle);
    mousePickStyle = PickStyle::sanitizeMouse(mousePickStyle);
    trackerPref = qBound(0, trackerPref, 1);
    speechPitch = qBound(0.5, speechPitch, 2.0);
    auto clampList = [](QStringList list, int max) {
        QStringList out;
        for (QString s : list) {
            s = s.trimmed();
            if (s.isEmpty()) {
                continue;
            }
            out.push_back(s);
            if (out.size() >= max) {
                break;
            }
        }
        return out;
    };
    elevenFavoriteVoiceIds = clampList(elevenFavoriteVoiceIds, 24);
    {
        QVector<SavedSpeechTag> tags;
        for (SavedSpeechTag t : savedSpeechTags) {
            t.name = normalizeSpeechTag(t.name);
            if (t.name.isEmpty()) {
                continue;
            }
            t.color = t.color.trimmed();
            t.icon = t.icon.trimmed();
            tags.push_back(t);
            if (tags.size() >= 24) {
                break;
            }
        }
        savedSpeechTags = std::move(tags);
    }
    {
        QVector<SavedSpeechVoice> voices;
        for (SavedSpeechVoice v : savedSpeechVoices) {
            v.id = v.id.trimmed();
            v.name = v.name.trimmed();
            if (v.name.size() > 24) {
                v.name = v.name.left(24);
            }
            v.voiceId = v.voiceId.trimmed();
            v.speed = qBound(0.5, v.speed, 2.0);
            v.volume = qBound(1.0, v.volume, 5.0);
            v.color = v.color.trimmed();
            v.icon = v.icon.trimmed();
            if (v.model != QLatin1String("sapi") && v.model != QLatin1String("eleven_v3")
                && v.model != QLatin1String("eleven_flash_v2_5")) {
                v.model = QStringLiteral("sapi");
            }
            if (v.id.isEmpty() || v.name.isEmpty()) {
                continue;
            }
            voices.push_back(v);
            if (voices.size() >= kMaxSavedSpeechVoices) {
                break;
            }
        }
        savedSpeechVoices = std::move(voices);
    }
    speechLangFilter = speechLangFilter.trimmed().toLower();
    if (speechLangFilter == QLatin1String("all")) {
        speechLangFilter.clear();
    }
    if (speechModel != QLatin1String("sapi") && speechModel != QLatin1String("eleven_v3")
        && speechModel != QLatin1String("eleven_flash_v2_5")) {
        speechModel = QStringLiteral("sapi");
    }
    if (headPoseMaps.size() > kMaxHeadPoseMaps) {
        headPoseMaps.resize(kMaxHeadPoseMaps);
    }
    QVector<HeadPoseMap> maps;
    QSet<QString> seen;
    for (HeadPoseMap m : headPoseMaps) {
        clampHeadPoseMap(m);
        if (m.id.isEmpty()) {
            continue;
        }
        if (seen.contains(m.id)) {
            continue;
        }
        seen.insert(m.id);
        maps.push_back(m);
    }
    headPoseMaps = std::move(maps);

    layoutAutoCloseFadeMs = qBound(50, layoutAutoCloseFadeMs, 60000);
    progress.ensureDefault();
    mouseProgress.ensureDefault();
    ltsMaxNotchesPerSec = snapLtsSpeed(
        qBound(kLtsSpeedStops[0], ltsMaxNotchesPerSec, kLtsSpeedStops[kLtsSpeedStopCount - 1]));

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
        QVector<int>& seq = sequenceField(*this, key);
        if (seq.isEmpty()) {
            seq = isRapidSequenceKey(key) ? defaultRapidDwellSequence() : defaultDwellSequence();
        }
        seq[0] = qBound(0, seq[0] + dir * 50, 10000);
        return true;
    }
    if (const IntSpec* s = findInt(key)) {
        if (key == QLatin1String("themeSaturation")) {
            setThemeSaturation(themeSaturation + dir * s->step);
            return true;
        }
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
    switch (qBound(0, preset, 3)) {
    case 0:
        applyTimingPack(*this, kDwellSlow);
        break;
    case 2:
        applyTimingPack(*this, kDwellFast);
        break;
    case 3:
        if (dwellPreset() != 3) {
            applyDwellCustom();
        }
        break;
    default:
        applyTimingPack(*this, kDwellNormal);
        break;
    }
}

int AppSettings::dwellPreset() const
{
    if (matchesTimingPack(*this, kDwellSlow)) {
        return 0;
    }
    if (matchesTimingPack(*this, kDwellNormal)) {
        return 1;
    }
    if (matchesTimingPack(*this, kDwellFast)) {
        return 2;
    }
    return 3;
}

void AppSettings::saveDwellCustom()
{
    customTiming = liveTiming(*this);
}

void AppSettings::applyDwellCustom()
{
    clampTimingPack(customTiming);
    applyTimingPack(*this, customTiming);
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
    keys << QStringLiteral("dwellMs") << QStringLiteral("rapidDwellMs");
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
    QColor stored = c;
    if (key == QLatin1String("progressColor") || key == QLatin1String("progressFillColor")
        || key == QLatin1String("customSecondaryColor")) {
        stored.setAlpha(kProgressFillAlpha);
    }
    this->*s->member = colorToHex(stored);
    if (key == QLatin1String("progressColor")) {
        customSecondaryColor = colorToHex(stored);
    } else if (key == QLatin1String("customSecondaryColor")) {
        progressColor = colorToHex(stored);
        progressFillColor = colorToHex(stored);
    }
    if (!rebuildPalette || themeRoleForColorKey(key).isEmpty()) {
        return true;
    }
    applyTheme();
    return true;
}

QString AppSettings::displayValue(const QString& key) const
{
    if (isSequenceKey(key)) {
        return formatSequence(sequenceField(*this, key)) + QStringLiteral(" ms");
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
    if (key == QLatin1String("ltsScrollMode")) {
        return QLatin1String(ltsScrollModeName(ltsScrollMode));
    }
    if (key == QLatin1String("magPickStyle")) {
        return PickStyle::label(magPickStyle);
    }
    if (key == QLatin1String("mousePickStyle")) {
        return PickStyle::label(mousePickStyle);
    }
    if (const StyleToggle* t = findStyleToggle(key)) {
        return styleFlag(*t) ? QStringLiteral("ON") : QStringLiteral("OFF");
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
    if (isRapidSequenceKey(key)) {
        return QStringLiteral("Rapid");
    }
    if (isSequenceKey(key)) {
        return QStringLiteral("Standard");
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
    if (key == QLatin1String("magFollowProfile")) {
        return QStringLiteral("Gaze follow");
    }
    return key;
}

QString AppSettings::settingDescription(const QString& key)
{
    if (isRapidSequenceKey(key)) {
        return QStringLiteral(
            "Comma-separated rapid dwell times in ms (keys, composer typing, "
            "modifiers). Last step repeats. 0 fires immediately after scan grace.");
    }
    if (isSequenceKey(key)) {
        return QStringLiteral(
            "Comma-separated standard dwell times in ms (settings, navigation, mouse, "
            "AHK, composer word chips). Last step repeats.");
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
    if (key == QLatin1String("flashCustom")) {
        return QStringLiteral("Flash a custom color instead of the item foreground.");
    }
    if (key == QLatin1String("hoverColor")) {
        return QStringLiteral("Hover outline when Custom is on.");
    }
    if (key == QLatin1String("hoverCustom")) {
        return QStringLiteral("Hover outline uses a custom color instead of progress.");
    }
    if (key == QLatin1String("customSourceColor")) {
        return QStringLiteral("Unused. Accent is customPrimaryColor; progress is customSecondaryColor.");
    }
    if (key == QLatin1String("customPrimaryColor")) {
        return QStringLiteral("Highlighted foreground (active labels, accent).");
    }
    if (key == QLatin1String("customSecondaryColor")) {
        return QStringLiteral("Progress ring and fill color.");
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
            "Look-to-scroll overlay: Filled (soft stretched disk), Hollow (glass ring "
            "that stretches in the scroll direction), or Pause (center pause/resume only).");
    }
    if (key == QLatin1String("magFollowProfile")) {
        return QStringLiteral(
            "Slow / Sticky / Smooth / Snappy for the gaze indicator, gaze mouse, and "
            "live magnifier. Not mag-pick, mouse-pick, or dwell.");
    }
    return {};
}

QString AppSettings::numericBufferSeed(const QString& key) const
{
    if (isSequenceKey(key)) {
        return formatSequence(sequenceField(*this, key));
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
        sequenceField(*this, key) = seq;
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
        if (key == QLatin1String("themeSaturation")) {
            setThemeSaturation(v);
            return true;
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
