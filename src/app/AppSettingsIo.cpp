#include "app/AppSettings.h"

#include "assist/GazeFollowProfile.h"
#include "assist/LookToMap.h"
#include "assist/LtsIndicator.h"
#include "assist/LtsScrollMode.h"
#include "mapping/HeadPoseCurve.h"
#include "ui/Theme.h"
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

namespace {

[[nodiscard]] ScreenCaptureMode screenCaptureModeFromInt(int v)
{
    if (v == int(ScreenCaptureMode::All)) {
        return ScreenCaptureMode::All;
    }
    if (v == int(ScreenCaptureMode::None)) {
        return ScreenCaptureMode::None;
    }
    return ScreenCaptureMode::Pages;
}

} // namespace

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

QString AppSettings::rapidDwellSequenceString() const
{
    QStringList parts;
    for (int ms : rapidDwellSequence) {
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
        if (!ok || v < 0 || v > 10000) {
            if (error) {
                *error = QStringLiteral("Each step must be an integer 0–10000 ms");
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

    auto loadSeq = [](const QJsonValue& v) {
        QVector<int> out;
        if (v.isArray()) {
            for (const QJsonValue& n : v.toArray()) {
                if (!n.isDouble() && !n.isString()) {
                    continue;
                }
                const int ms = n.toInt(-1);
                if (ms >= 0 && ms <= 10000) {
                    out.push_back(ms);
                }
            }
        } else if (v.isString()) {
            out = parseDwellSequence(v.toString());
        } else if (v.isDouble()) {
            const int ms = v.toInt(-1);
            if (ms >= 0 && ms <= 10000) {
                out = {ms};
            }
        }
        return out;
    };

    if (o.contains(QStringLiteral("dwellSequence"))) {
        dwellSequence = loadSeq(o.value(QStringLiteral("dwellSequence")));
    }

    if (o.contains(QStringLiteral("rapidDwellSequence"))) {
        rapidDwellSequence = loadSeq(o.value(QStringLiteral("rapidDwellSequence")));
    }

    scanGraceMs = o.value(QStringLiteral("scanGraceMs")).toInt(scanGraceMs);
    dwellGraceMs = o.value(QStringLiteral("dwellGraceMs")).toInt(dwellGraceMs);
    mouseMoveDwellMs = o.value(QStringLiteral("mouseMoveDwellMs")).toInt(mouseMoveDwellMs);
    magPickDwellMs = o.value(QStringLiteral("magPickDwellMs")).toInt(magPickDwellMs);
    if (o.contains(QStringLiteral("customDwellSequence"))) {
        customTiming.sequence = loadSeq(o.value(QStringLiteral("customDwellSequence")));
    }
    if (o.contains(QStringLiteral("customRapidDwellSequence"))) {
        customTiming.rapidSequence = loadSeq(o.value(QStringLiteral("customRapidDwellSequence")));
    }
    customTiming.blinkGraceMs =
        o.value(QStringLiteral("customDwellGraceMs")).toInt(customTiming.blinkGraceMs);
    customTiming.mouseMoveDwellMs =
        o.value(QStringLiteral("customMouseMoveDwellMs")).toInt(customTiming.mouseMoveDwellMs);
    customTiming.magPickDwellMs =
        o.value(QStringLiteral("customMagPickDwellMs")).toInt(customTiming.magPickDwellMs);
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
    auto readLookToMap = [](const QJsonObject& mo, LookToDest dest) {
        LookToMapSettings c = defaultLookToMapSettings(dest);
        c.deadzonePx = mo.value(QStringLiteral("deadzonePx")).toInt(c.deadzonePx);
        c.rampEndPx = mo.value(QStringLiteral("rampEndPx")).toInt(c.rampEndPx);
        c.fullOuterPx = mo.value(QStringLiteral("fullOuterPx")).toInt(c.fullOuterPx);
        c.outerDeadzonePx = mo.value(QStringLiteral("outerDeadzonePx")).toInt(c.outerDeadzonePx);
        c.outerDeadzoneEnabled =
            mo.value(QStringLiteral("outerDeadzoneEnabled")).toBool(c.outerDeadzoneEnabled);
        c.hubEnabled = mo.value(QStringLiteral("hubEnabled")).toBool(c.hubEnabled);
        c.maxSpeed = mo.value(QStringLiteral("maxSpeed")).toDouble(c.maxSpeed);
        c.accelPerSec = mo.value(QStringLiteral("accelPerSec")).toDouble(c.accelPerSec);
        c.centerDwellMs = mo.value(QStringLiteral("centerDwellMs")).toInt(c.centerDwellMs);
        c.axisMode =
            ltsScrollModeFromInt(mo.value(QStringLiteral("axisMode")).toInt(int(c.axisMode)));
        if (mo.contains(QStringLiteral("showPause")) || mo.contains(QStringLiteral("showFill"))
            || mo.contains(QStringLiteral("showBorder"))) {
            c.showPause = mo.value(QStringLiteral("showPause")).toBool(c.showPause);
            c.showInnerDeadzone =
                mo.value(QStringLiteral("showInnerDeadzone")).toBool(c.showInnerDeadzone);
            c.showMax = mo.value(QStringLiteral("showMax")).toBool(c.showMax);
            c.showOuterDeadzone =
                mo.value(QStringLiteral("showOuterDeadzone")).toBool(c.showOuterDeadzone);
            c.showBorder = mo.value(QStringLiteral("showBorder")).toBool(c.showBorder);
            c.showFill = mo.value(QStringLiteral("showFill")).toBool(c.showFill);
        } else if (mo.contains(QStringLiteral("indicatorStyle"))) {
            applyLegacyLookToIndicator(
                c, ltsIndicatorFromInt(mo.value(QStringLiteral("indicatorStyle")).toInt(0)));
        }
        clampLookToMapSettings(dest, c);
        return c;
    };
    if (o.value(QStringLiteral("lookToMaps")).isObject()) {
        const QJsonObject maps = o.value(QStringLiteral("lookToMaps")).toObject();
        if (maps.value(QStringLiteral("scroll")).isObject()) {
            lookToScroll = readLookToMap(maps.value(QStringLiteral("scroll")).toObject(),
                                         LookToDest::Scroll);
        }
        if (maps.value(QStringLiteral("mouse")).isObject()) {
            lookToMouse =
                readLookToMap(maps.value(QStringLiteral("mouse")).toObject(), LookToDest::Mouse);
        }
        if (maps.value(QStringLiteral("leftStick")).isObject()) {
            lookToLeftStick = readLookToMap(maps.value(QStringLiteral("leftStick")).toObject(),
                                            LookToDest::LeftStick);
        }
        if (maps.value(QStringLiteral("rightStick")).isObject()) {
            lookToRightStick = readLookToMap(maps.value(QStringLiteral("rightStick")).toObject(),
                                             LookToDest::RightStick);
        }
    } else if (o.contains(QStringLiteral("ltsDeadzonePx"))
               || o.contains(QStringLiteral("ltsFalloffPx"))) {
        lookToScroll = lookToMapFromLegacyLts(
            o.value(QStringLiteral("ltsDeadzonePx")).toInt(80),
            o.value(QStringLiteral("ltsFalloffPx")).toInt(300),
            o.value(QStringLiteral("ltsMaxNotchesPerSec")).toDouble(kLtsSpeedDefault),
            o.value(QStringLiteral("ltsAccelPerSec")).toDouble(kLtsAccelDefault),
            o.value(QStringLiteral("ltsCenterDwellMs")).toInt(700),
            ltsIndicatorFromInt(o.value(QStringLiteral("ltsIndicatorStyle")).toInt(0)),
            ltsScrollModeFromInt(o.value(QStringLiteral("ltsScrollMode")).toInt(2)));
        lookToMouse = defaultLookToMapSettings(LookToDest::Mouse);
        lookToLeftStick = defaultLookToMapSettings(LookToDest::LeftStick);
        lookToRightStick = defaultLookToMapSettings(LookToDest::RightStick);
    }
    comboInnerRadiusPx = o.value(QStringLiteral("comboInnerRadiusPx")).toInt(comboInnerRadiusPx);
    comboSharedRadiusPx = o.value(QStringLiteral("comboSharedRadiusPx")).toInt(comboSharedRadiusPx);
    comboOuterRadiusPx = o.value(QStringLiteral("comboOuterRadiusPx")).toInt(comboOuterRadiusPx);
    comboInnerColor = o.value(QStringLiteral("comboInnerColor")).toString(comboInnerColor);
    comboOuterColor = o.value(QStringLiteral("comboOuterColor")).toString(comboOuterColor);
    autoCollapseMain = o.value(QStringLiteral("autoCollapseMain")).toBool(autoCollapseMain);
    startDocked = o.value(QStringLiteral("startDocked")).toBool(startDocked);
    showSplash = o.value(QStringLiteral("showSplash")).toBool(showSplash);
    layoutAutoClose = o.value(QStringLiteral("layoutAutoClose")).toBool(layoutAutoClose);
    layoutAutoCloseIdleMs =
        o.value(QStringLiteral("layoutAutoCloseIdleMs")).toInt(layoutAutoCloseIdleMs);
    layoutAutoCloseFadeMs =
        o.value(QStringLiteral("layoutAutoCloseFadeMs")).toInt(layoutAutoCloseFadeMs);
    trackerPref = o.value(QStringLiteral("trackerPref")).toInt(trackerPref);
    screenCapture = screenCaptureModeFromInt(
        o.value(QStringLiteral("screenCapture")).toInt(int(screenCapture)));
    headPoseEnabled = o.value(QStringLiteral("headPoseEnabled")).toBool(headPoseEnabled);
    headPoseOriginSet = o.value(QStringLiteral("headPoseOriginSet")).toBool(false);
    if (o.value(QStringLiteral("headPoseOrigin")).isObject()) {
        const QJsonObject ho = o.value(QStringLiteral("headPoseOrigin")).toObject();
        headPoseOrigin.yaw = ho.value(QStringLiteral("yaw")).toDouble();
        headPoseOrigin.pitch = ho.value(QStringLiteral("pitch")).toDouble();
        headPoseOrigin.roll = ho.value(QStringLiteral("roll")).toDouble();
        headPoseOrigin.x = ho.value(QStringLiteral("x")).toDouble();
        headPoseOrigin.y = ho.value(QStringLiteral("y")).toDouble();
        headPoseOrigin.z = ho.value(QStringLiteral("z")).toDouble();
        headPoseOrigin.rotationValid = true;
        headPoseOrigin.positionValid = true;
        headPoseOriginSet = true;
    }
    headPoseMaps.clear();
    if (o.value(QStringLiteral("headPoseMaps")).isArray()) {
        for (const QJsonValue& v : o.value(QStringLiteral("headPoseMaps")).toArray()) {
            if (!v.isObject()) {
                continue;
            }
            const QJsonObject mo = v.toObject();
            HeadPoseMap m;
            m.id = mo.value(QStringLiteral("id")).toString();
            m.enabled = mo.value(QStringLiteral("enabled")).toBool(true);
            bool ok = false;
            m.source = headPoseAxisFromId(mo.value(QStringLiteral("source")).toString(), &ok);
            if (!ok) {
                m.source = HeadPoseAxis::Yaw;
            }
            m.dest = headPoseDestFromId(mo.value(QStringLiteral("dest")).toString(), &ok);
            if (!ok) {
                continue;
            }
            m.command = mo.value(QStringLiteral("command")).toString();
            m.commandAt = mo.value(QStringLiteral("commandAt")).toDouble(15.0);
            m.hysteresis = mo.value(QStringLiteral("hysteresis")).toDouble(2.0);
            if (mo.value(QStringLiteral("points")).isArray()) {
                for (const QJsonValue& pv : mo.value(QStringLiteral("points")).toArray()) {
                    if (!pv.isArray()) {
                        continue;
                    }
                    const QJsonArray pa = pv.toArray();
                    if (pa.size() < 2) {
                        continue;
                    }
                    HeadPoseCurvePoint pt;
                    pt.in = pa.at(0).toDouble();
                    pt.out = pa.at(1).toDouble();
                    m.points.push_back(pt);
                }
            }
            if (m.points.size() < 2) {
                m.points = defaultHeadPoseMap().points;
            }
            if (!m.id.isEmpty()) {
                headPoseMaps.push_back(m);
            }
        }
    }
    speechModel = o.value(QStringLiteral("speechModel")).toString(speechModel);
    elevenVoiceId = o.value(QStringLiteral("elevenVoiceId")).toString(elevenVoiceId);
    sapiVoiceToken = o.value(QStringLiteral("sapiVoiceToken")).toString(sapiVoiceToken);
    speechSpeed = o.value(QStringLiteral("speechSpeed")).toDouble(speechSpeed);
    speechPitch = o.value(QStringLiteral("speechPitch")).toDouble(speechPitch);
    speechVolume = o.value(QStringLiteral("speechVolume")).toDouble(speechVolume);
    speechLangFilter = o.value(QStringLiteral("speechLangFilter")).toString(speechLangFilter);
    elevenApiKeySet = false;
    auto readStringList = [](const QJsonObject& obj, const QString& key, const QStringList& fallback) {
        if (!obj.contains(key) || !obj.value(key).isArray()) {
            return fallback;
        }
        QStringList out;
        for (const QJsonValue& v : obj.value(key).toArray()) {
            const QString s = v.toString().trimmed();
            if (!s.isEmpty()) {
                out.push_back(s);
            }
        }
        return out;
    };
    elevenFavoriteVoiceIds =
        readStringList(o, QStringLiteral("elevenFavoriteVoiceIds"), {});
    savedSpeechTags.clear();
    if (o.value(QStringLiteral("savedSpeechTags")).isArray()) {
        for (const QJsonValue& v : o.value(QStringLiteral("savedSpeechTags")).toArray()) {
            SavedSpeechTag tag;
            if (v.isString()) {
                tag.name = v.toString();
            } else if (v.isObject()) {
                const QJsonObject to = v.toObject();
                tag.name = to.value(QStringLiteral("name")).toString();
                tag.color = to.value(QStringLiteral("color")).toString();
                tag.icon = to.value(QStringLiteral("icon")).toString();
            }
            if (!tag.name.trimmed().isEmpty()) {
                savedSpeechTags.push_back(tag);
            }
        }
    } else {
        savedSpeechTags = defaultSavedSpeechTags();
    }
    savedSpeechVoices.clear();
    if (o.value(QStringLiteral("savedSpeechVoices")).isArray()) {
        for (const QJsonValue& v : o.value(QStringLiteral("savedSpeechVoices")).toArray()) {
            if (!v.isObject()) {
                continue;
            }
            const QJsonObject vo = v.toObject();
            SavedSpeechVoice item;
            item.id = vo.value(QStringLiteral("id")).toString();
            item.name = vo.value(QStringLiteral("name")).toString();
            item.model = vo.value(QStringLiteral("model")).toString();
            item.voiceId = vo.value(QStringLiteral("voiceId")).toString();
            item.speed = vo.value(QStringLiteral("speed")).toDouble(1.0);
            item.volume = vo.value(QStringLiteral("volume")).toDouble(1.0);
            item.color = vo.value(QStringLiteral("color")).toString();
            item.icon = vo.value(QStringLiteral("icon")).toString();
            savedSpeechVoices.push_back(item);
        }
    }
    if (o.contains(QStringLiteral("themeAppearance"))) {
        themeAppearance = themeAppearanceFromString(
            o.value(QStringLiteral("themeAppearance")).toString());
    }
    themeSaturation = o.value(QStringLiteral("themeSaturation")).toInt(themeSaturation);
    customPrimaryColor = o.value(QStringLiteral("customPrimaryColor")).toString(customPrimaryColor);
    customSourceColor = o.value(QStringLiteral("customSourceColor")).toString(customSourceColor);
    customSecondaryColor =
        o.value(QStringLiteral("customSecondaryColor")).toString(customSecondaryColor);
    customTextColor = o.value(QStringLiteral("customTextColor")).toString(customTextColor);
    customDangerColor = o.value(QStringLiteral("customDangerColor")).toString(customDangerColor);
    themeBrightness = o.value(QStringLiteral("themeBrightness")).toInt(themeBrightness);
    if (o.contains(QStringLiteral("themeTintFamily"))) {
        themeTintFamily = themeTintFamilyFromString(
            o.value(QStringLiteral("themeTintFamily")).toString());
    }
    for (const StyleToggle& t : kStyleToggles) {
        styleFlag(t) = o.value(QLatin1String(t.jsonKey)).toBool(styleFlag(t));
    }
    progressColor = o.value(QStringLiteral("progressColor")).toString(progressColor);
    progressFillColor = o.value(QStringLiteral("progressFillColor")).toString(progressFillColor);
    if (o.contains(QStringLiteral("hoverColor"))) {
        hoverColor = o.value(QStringLiteral("hoverColor")).toString(hoverColor);
    } else if (o.contains(QStringLiteral("progressBorderColor"))) {
        hoverColor = o.value(QStringLiteral("progressBorderColor")).toString(hoverColor);
    }
    hoverBorderWeight = o.value(QStringLiteral("hoverBorderWeight")).toInt(hoverBorderWeight);
    if (o.contains(QStringLiteral("hoverCustom"))) {
        hoverCustom = o.value(QStringLiteral("hoverCustom")).toBool(hoverCustom);
    } else if (o.contains(QStringLiteral("hoverUseProgressColor"))) {
        hoverCustom = !o.value(QStringLiteral("hoverUseProgressColor")).toBool(true);
    }
    if (o.contains(QStringLiteral("flashCustom"))) {
        flashCustom = o.value(QStringLiteral("flashCustom")).toBool(flashCustom);
    } else if (o.contains(QStringLiteral("flashUseForeground"))) {
        flashCustom = !o.value(QStringLiteral("flashUseForeground")).toBool(true);
    }
    flashForegroundOpacity =
        o.value(QStringLiteral("flashForegroundOpacity")).toInt(flashForegroundOpacity);
    flashColor = o.value(QStringLiteral("flashColor")).toString(flashColor);
    flashMs = o.value(QStringLiteral("flashMs")).toInt(flashMs);

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
    QJsonArray rapidSeq;
    for (int ms : copy.rapidDwellSequence) {
        rapidSeq.append(ms);
    }
    o.insert(QStringLiteral("rapidDwellSequence"), rapidSeq);
    o.insert(QStringLiteral("scanGraceMs"), copy.scanGraceMs);
    o.insert(QStringLiteral("dwellGraceMs"), copy.dwellGraceMs);
    o.insert(QStringLiteral("mouseMoveDwellMs"), copy.mouseMoveDwellMs);
    o.insert(QStringLiteral("magPickDwellMs"), copy.magPickDwellMs);
    QJsonArray customSeq;
    for (int ms : copy.customTiming.sequence) {
        customSeq.append(ms);
    }
    o.insert(QStringLiteral("customDwellSequence"), customSeq);
    QJsonArray customRapidSeq;
    for (int ms : copy.customTiming.rapidSequence) {
        customRapidSeq.append(ms);
    }
    o.insert(QStringLiteral("customRapidDwellSequence"), customRapidSeq);
    o.insert(QStringLiteral("customDwellGraceMs"), copy.customTiming.blinkGraceMs);
    o.insert(QStringLiteral("customMouseMoveDwellMs"), copy.customTiming.mouseMoveDwellMs);
    o.insert(QStringLiteral("customMagPickDwellMs"), copy.customTiming.magPickDwellMs);
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
    auto writeLookToMap = [](const LookToMapSettings& c) {
        QJsonObject mo;
        mo.insert(QStringLiteral("deadzonePx"), c.deadzonePx);
        mo.insert(QStringLiteral("rampEndPx"), c.rampEndPx);
        mo.insert(QStringLiteral("fullOuterPx"), c.fullOuterPx);
        mo.insert(QStringLiteral("outerDeadzonePx"), c.outerDeadzonePx);
        mo.insert(QStringLiteral("outerDeadzoneEnabled"), c.outerDeadzoneEnabled);
        mo.insert(QStringLiteral("hubEnabled"), c.hubEnabled);
        mo.insert(QStringLiteral("maxSpeed"), c.maxSpeed);
        mo.insert(QStringLiteral("accelPerSec"), c.accelPerSec);
        mo.insert(QStringLiteral("centerDwellMs"), c.centerDwellMs);
        mo.insert(QStringLiteral("axisMode"), int(c.axisMode));
        mo.insert(QStringLiteral("showPause"), c.showPause);
        mo.insert(QStringLiteral("showInnerDeadzone"), c.showInnerDeadzone);
        mo.insert(QStringLiteral("showMax"), c.showMax);
        mo.insert(QStringLiteral("showOuterDeadzone"), c.showOuterDeadzone);
        mo.insert(QStringLiteral("showBorder"), c.showBorder);
        mo.insert(QStringLiteral("showFill"), c.showFill);
        return mo;
    };
    QJsonObject lookToMaps;
    lookToMaps.insert(QStringLiteral("scroll"), writeLookToMap(copy.lookToScroll));
    lookToMaps.insert(QStringLiteral("mouse"), writeLookToMap(copy.lookToMouse));
    lookToMaps.insert(QStringLiteral("leftStick"), writeLookToMap(copy.lookToLeftStick));
    lookToMaps.insert(QStringLiteral("rightStick"), writeLookToMap(copy.lookToRightStick));
    o.insert(QStringLiteral("lookToMaps"), lookToMaps);
    o.insert(QStringLiteral("comboInnerRadiusPx"), copy.comboInnerRadiusPx);
    o.insert(QStringLiteral("comboSharedRadiusPx"), copy.comboSharedRadiusPx);
    o.insert(QStringLiteral("comboOuterRadiusPx"), copy.comboOuterRadiusPx);
    o.insert(QStringLiteral("comboInnerColor"), copy.comboInnerColor);
    o.insert(QStringLiteral("comboOuterColor"), copy.comboOuterColor);
    o.insert(QStringLiteral("autoCollapseMain"), copy.autoCollapseMain);
    o.insert(QStringLiteral("startDocked"), copy.startDocked);
    o.insert(QStringLiteral("showSplash"), copy.showSplash);
    o.insert(QStringLiteral("layoutAutoClose"), copy.layoutAutoClose);
    o.insert(QStringLiteral("layoutAutoCloseIdleMs"), copy.layoutAutoCloseIdleMs);
    o.insert(QStringLiteral("layoutAutoCloseFadeMs"), copy.layoutAutoCloseFadeMs);
    o.insert(QStringLiteral("trackerPref"), copy.trackerPref);
    o.insert(QStringLiteral("screenCapture"), int(copy.screenCapture));
    o.insert(QStringLiteral("headPoseEnabled"), copy.headPoseEnabled);
    o.insert(QStringLiteral("headPoseOriginSet"), copy.headPoseOriginSet);
    if (copy.headPoseOriginSet) {
        QJsonObject ho;
        ho.insert(QStringLiteral("yaw"), copy.headPoseOrigin.yaw);
        ho.insert(QStringLiteral("pitch"), copy.headPoseOrigin.pitch);
        ho.insert(QStringLiteral("roll"), copy.headPoseOrigin.roll);
        ho.insert(QStringLiteral("x"), copy.headPoseOrigin.x);
        ho.insert(QStringLiteral("y"), copy.headPoseOrigin.y);
        ho.insert(QStringLiteral("z"), copy.headPoseOrigin.z);
        o.insert(QStringLiteral("headPoseOrigin"), ho);
    }
    {
        QJsonArray maps;
        for (const HeadPoseMap& m : copy.headPoseMaps) {
            QJsonObject mo;
            mo.insert(QStringLiteral("id"), m.id);
            mo.insert(QStringLiteral("enabled"), m.enabled);
            mo.insert(QStringLiteral("source"), QLatin1String(headPoseAxisId(m.source)));
            mo.insert(QStringLiteral("dest"), QLatin1String(headPoseDestId(m.dest)));
            mo.insert(QStringLiteral("command"), m.command);
            mo.insert(QStringLiteral("commandAt"), m.commandAt);
            mo.insert(QStringLiteral("hysteresis"), m.hysteresis);
            QJsonArray pts;
            for (const HeadPoseCurvePoint& p : m.points) {
                QJsonArray pair;
                pair.append(p.in);
                pair.append(p.out);
                pts.append(pair);
            }
            mo.insert(QStringLiteral("points"), pts);
            maps.append(mo);
        }
        o.insert(QStringLiteral("headPoseMaps"), maps);
    }
    o.insert(QStringLiteral("speechModel"), copy.speechModel);
    o.insert(QStringLiteral("elevenVoiceId"), copy.elevenVoiceId);
    o.insert(QStringLiteral("sapiVoiceToken"), copy.sapiVoiceToken);
    o.insert(QStringLiteral("speechSpeed"), copy.speechSpeed);
    o.insert(QStringLiteral("speechPitch"), copy.speechPitch);
    o.insert(QStringLiteral("speechVolume"), copy.speechVolume);
    o.insert(QStringLiteral("speechLangFilter"), copy.speechLangFilter);
    auto writeStringList = [](const QStringList& v) {
        QJsonArray a;
        for (const QString& s : v) {
            a.append(s);
        }
        return a;
    };
    o.insert(QStringLiteral("elevenFavoriteVoiceIds"), writeStringList(copy.elevenFavoriteVoiceIds));
    {
        QJsonArray tags;
        for (const SavedSpeechTag& t : copy.savedSpeechTags) {
            QJsonObject to;
            to.insert(QStringLiteral("name"), t.name);
            to.insert(QStringLiteral("color"), t.color);
            to.insert(QStringLiteral("icon"), t.icon);
            tags.append(to);
        }
        o.insert(QStringLiteral("savedSpeechTags"), tags);
    }
    {
        QJsonArray voices;
        for (const SavedSpeechVoice& v : copy.savedSpeechVoices) {
            QJsonObject vo;
            vo.insert(QStringLiteral("id"), v.id);
            vo.insert(QStringLiteral("name"), v.name);
            vo.insert(QStringLiteral("model"), v.model);
            vo.insert(QStringLiteral("voiceId"), v.voiceId);
            vo.insert(QStringLiteral("speed"), v.speed);
            vo.insert(QStringLiteral("volume"), v.volume);
            vo.insert(QStringLiteral("color"), v.color);
            vo.insert(QStringLiteral("icon"), v.icon);
            voices.append(vo);
        }
        o.insert(QStringLiteral("savedSpeechVoices"), voices);
    }
    o.insert(QStringLiteral("themeAppearance"), themeAppearanceToString(copy.themeAppearance));
    o.insert(QStringLiteral("themeSaturation"), copy.themeSaturation);
    o.insert(QStringLiteral("customSourceColor"), copy.customSourceColor);
    o.insert(QStringLiteral("customPrimaryColor"), copy.customPrimaryColor);
    o.insert(QStringLiteral("customSecondaryColor"), copy.customSecondaryColor);
    o.insert(QStringLiteral("customTextColor"), copy.customTextColor);
    o.insert(QStringLiteral("customDangerColor"), copy.customDangerColor);
    o.insert(QStringLiteral("themeBrightness"), copy.themeBrightness);
    o.insert(QStringLiteral("themeTintFamily"), themeTintFamilyToString(copy.themeTintFamily));
    for (const StyleToggle& t : kStyleToggles) {
        o.insert(QLatin1String(t.jsonKey), copy.styleFlag(t));
    }
    o.insert(QStringLiteral("progressColor"), copy.progressColor);
    o.insert(QStringLiteral("progressFillColor"), copy.progressFillColor);
    o.insert(QStringLiteral("hoverColor"), copy.hoverColor);
    o.insert(QStringLiteral("hoverBorderWeight"), copy.hoverBorderWeight);
    o.insert(QStringLiteral("hoverCustom"), copy.hoverCustom);
    o.insert(QStringLiteral("flashCustom"), copy.flashCustom);
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
