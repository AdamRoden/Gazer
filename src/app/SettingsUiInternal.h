#pragma once

#include "layout/PageHit.h"

#include <QString>
#include <QtGlobal>

namespace gazer {
namespace SettingsUiInternal {

constexpr auto kLiveNumpad = "settings_numpad_live";
constexpr auto kLiveArray = "settings_array_live";
constexpr auto kLiveColor = "settings_color_live";
constexpr auto kLiveOpacity = "settings_opacity_live";
constexpr auto kLiveHex = "settings_hex_live";
constexpr auto kLiveSpeechKey = "settings_speech_key_live";

constexpr int kMaxArraySteps = 12;
constexpr int kStepNudge = 50;

inline QString localIdOf(const PageTarget& t)
{
    if (!t.pageId.isEmpty() && t.id.startsWith(t.pageId + QLatin1Char('/'))) {
        return t.id.mid(t.pageId.size() + 1);
    }
    return t.id;
}

struct ColorAxis {
    const char* id;
    const char* label;
    const char* title;
    const char* hint;
    enum class Kind { Hue, Sat, Light, Red, Green, Blue, Alpha } kind;
};

constexpr ColorAxis kColorAxes[] = {
    {"h", "H", "Hue", "Hue in degrees (0–359).", ColorAxis::Kind::Hue},
    {"s", "S", "Saturation", "Saturation percent (0–100).", ColorAxis::Kind::Sat},
    {"l", "L", "Lightness", "Lightness percent (0–100).", ColorAxis::Kind::Light},
    {"r", "R", "R", "R channel (0–255).", ColorAxis::Kind::Red},
    {"g", "G", "G", "G channel (0–255).", ColorAxis::Kind::Green},
    {"b", "B", "B", "B channel (0–255).", ColorAxis::Kind::Blue},
    {"a", "A", "Alpha", "Opacity percent (0–100).", ColorAxis::Kind::Alpha},
};

inline const ColorAxis* findColorAxis(const QString& channel)
{
    for (const ColorAxis& a : kColorAxes) {
        if (channel.compare(QLatin1String(a.id), Qt::CaseInsensitive) == 0) {
            return &a;
        }
    }
    return nullptr;
}

inline int pct255(int raw)
{
    return qBound(0, qRound(raw / 2.55), 100);
}

inline int fromPct255(int shown)
{
    return qBound(0, int(qRound(shown * 2.55)), 255);
}

inline void colorChannelRange(const QString& channel, int* minV, int* maxV)
{
    const QString ch = channel.toLower();
    if (ch == QLatin1String("h") || ch == QLatin1String("hue")) {
        *minV = 0;
        *maxV = 359;
        return;
    }
    if (ch == QLatin1String("s") || ch == QLatin1String("sat") || ch == QLatin1String("l")
        || ch == QLatin1String("light") || ch == QLatin1String("lightness")
        || ch == QLatin1String("a") || ch == QLatin1String("alpha")
        || ch == QLatin1String("opacity")) {
        *minV = 0;
        *maxV = 100;
        return;
    }
    *minV = 0;
    *maxV = 255;
}

inline QString colorChannelValueText(const QString& channel, int shown)
{
    const QString ch = channel.toLower();
    if (ch == QLatin1String("s") || ch == QLatin1String("sat") || ch == QLatin1String("l")
        || ch == QLatin1String("light") || ch == QLatin1String("lightness")
        || ch == QLatin1String("a") || ch == QLatin1String("alpha")
        || ch == QLatin1String("opacity")) {
        return QStringLiteral("%1%").arg(shown);
    }
    return QString::number(shown);
}

} // namespace SettingsUiInternal
} // namespace gazer
