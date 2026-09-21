#pragma once

#include <QColor>
#include <QString>
#include <QtGlobal>

namespace gazer {
namespace SettingsUiInternal {

constexpr auto kLiveNumpad = "settings_numpad_live";
constexpr auto kLiveArray = "settings_array_live";
constexpr auto kLiveColor = "settings_color_live";
constexpr auto kLiveHex = "settings_hex_live";
constexpr auto kLiveSpeechKey = "settings_speech_key_live";
constexpr auto kLiveHeadPoseMap = "headpose_map_live";
constexpr auto kLiveHeadPoseCmd = "headpose_cmd_live";
constexpr auto kLiveLookToMap = "lookto_map_live";

constexpr int kMaxArraySteps = 12;
constexpr int kStepNudge = 50;

struct ColorAxis {
    const char* id;
    const char* label;
    const char* title;
    const char* hint;
    enum class Kind { Hue, Alpha } kind;
};

constexpr ColorAxis kColorAxes[] = {
    {"h", "H", "Hue", "Hue in degrees (0–359).", ColorAxis::Kind::Hue},
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

inline QString normalizeHexDigits(const QString& raw)
{
    QString hex = raw.trimmed();
    if (hex.startsWith(QLatin1Char('#'))) {
        hex = hex.mid(1);
    }
    QString out;
    out.reserve(hex.size());
    for (const QChar c : hex) {
        if (c.isSpace()) {
            continue;
        }
        const QChar u = c.toUpper();
        if ((u >= QLatin1Char('0') && u <= QLatin1Char('9'))
            || (u >= QLatin1Char('A') && u <= QLatin1Char('F'))) {
            out += u;
        }
    }
    return out;
}

inline QString hexSeedFromColor(const QColor& c)
{
    const QColor src = c.isValid() ? c : QColor(0, 220, 255);
    const QString hex = pct255(src.alpha()) < 100 ? src.name(QColor::HexArgb) : src.name(QColor::HexRgb);
    return hex.startsWith(QLatin1Char('#')) ? hex.mid(1).toUpper() : hex.toUpper();
}

struct HexApplyResult {
    bool ok = false;
    QColor rgb;
    bool setAlpha = false;
    int alpha = 255;
};

inline HexApplyResult parseHexDraft(const QString& raw)
{
    const QString d = normalizeHexDigits(raw);
    HexApplyResult r;
    if (d.size() < 6 || d.size() > 8) {
        return r;
    }
    auto nib = [](QChar c) -> int {
        const int n = c.toLatin1();
        if (n >= '0' && n <= '9') {
            return n - '0';
        }
        return 10 + (n - 'A');
    };
    auto byteAt = [&](int i) { return nib(d.at(i)) * 16 + nib(d.at(i + 1)); };
    if (d.size() == 6) {
        r.rgb = QColor(byteAt(0), byteAt(2), byteAt(4));
        r.ok = r.rgb.isValid();
        return r;
    }
    if (d.size() == 7) {
        r.rgb = QColor(byteAt(0), byteAt(2), byteAt(4));
        const int n = nib(d.at(6));
        r.setAlpha = true;
        r.alpha = n * 16 + n;
        r.ok = r.rgb.isValid();
        return r;
    }
    r.alpha = byteAt(0);
    r.rgb = QColor(byteAt(2), byteAt(4), byteAt(6));
    r.setAlpha = true;
    r.ok = r.rgb.isValid();
    return r;
}

} // namespace SettingsUiInternal
} // namespace gazer
