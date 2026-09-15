#include "ui/Theme.h"
#include "ui/PickerPalette.h"

#include <QtGlobal>

#include <cmath>
#include <optional>

namespace gazer {
namespace {

int indexOfToneWeight(int weight)
{
    for (int i = 0; i < kToneCount; ++i) {
        if (kToneWeights[i] == weight) {
            return i;
        }
    }
    return -1;
}

QString trimmedLower(const QString& s)
{
    return s.trimmed().toLower();
}

} // namespace

int ThemeColors::toneIndex(int weight)
{
    const int i = indexOfToneWeight(weight);
    return i >= 0 ? i : 0;
}

int ThemeColors::steppedWeight(int weight, int steps)
{
    return kToneWeights[qBound(0, toneIndex(weight) + steps, kToneCount - 1)];
}

std::optional<ToneRef> ThemeColors::parseToneToken(const QString& name)
{
    const QString t = trimmedLower(name);
    auto take = [&](const QLatin1String& prefix) -> std::optional<ToneRef> {
        if (!t.startsWith(prefix) || t.size() == prefix.size()) {
            return std::nullopt;
        }
        const QString rest = t.mid(prefix.size());
        bool ok = false;
        const int w = rest.toInt(&ok);
        if (!ok || indexOfToneWeight(w) < 0) {
            return std::nullopt;
        }
        return ToneRef{QString(prefix), w};
    };
    if (const std::optional<ToneRef> bg = take(QLatin1String("bg"))) {
        return bg;
    }
    if (const std::optional<ToneRef> accent = take(QLatin1String("accent"))) {
        return accent;
    }
    for (int i = 0; i < kPickerFamilyCount; ++i) {
        if (const std::optional<ToneRef> pal = take(QLatin1String(kPickerFamilies[i]))) {
            if (pickerShadeIndexForWeight(pal->weight) < 0) {
                return std::nullopt;
            }
            return pal;
        }
    }
    return std::nullopt;
}

ToneRef ThemeColors::toneRef(const QString& token)
{
    const QString t = trimmedLower(token);
    if (t.isEmpty() || t == QLatin1String("background")) {
        return {QStringLiteral("bg"), kRestTone};
    }
    if (t == QLatin1String("accent")) {
        return {QStringLiteral("accent"), kRestTone};
    }
    if (const std::optional<ToneRef> parsed = parseToneToken(t)) {
        return *parsed;
    }
    return {};
}

QColor ThemeColors::mixTone(const QColor& seed, int weight)
{
    const QColor base = seed.isValid() ? seed : QColor(10, 10, 11);
    const int w = indexOfToneWeight(weight) >= 0 ? weight : kRestTone;
    const QColor toward = base.lightness() <= 140 ? QColor(255, 255, 255) : QColor(0, 0, 0);
    QColor out = mix(base, toward, (100 - w) / 100.0);
    out.setAlpha(base.alpha());
    return out;
}

QColor ThemeColors::shadeTowardBlack(const QColor& fill, int steps, int fromWeight)
{
    const int w = indexOfToneWeight(fromWeight) >= 0 ? fromWeight : kRestTone;
    const int darker = steppedWeight(w, qMax(0, steps));
    return mix(fill, QColor(0, 0, 0), (w - darker) / 100.0);
}

bool ThemeColors::isNamedColor(const QString& name)
{
    const QString t = trimmedLower(name);
    if (t == QLatin1String("background") || t == QLatin1String("accent")
        || t == QLatin1String("progress") || t == QLatin1String("foreground")
        || t == QLatin1String("danger") || t == QLatin1String("border")) {
        return true;
    }
    return parseToneToken(t).has_value();
}

std::optional<QColor> ThemeColors::namedColor(const QString& name, const QColor& seed) const
{
    const QString t = trimmedLower(name);
    const QColor canvas = seed.isValid() ? seed : bgMain;
    if (t == QLatin1String("background")) {
        return canvas;
    }
    if (t == QLatin1String("accent")) {
        return accent;
    }
    if (t == QLatin1String("progress")) {
        return progress.isValid() ? progress : defaultProgressColor();
    }
    if (t == QLatin1String("foreground")) {
        return text;
    }
    if (t == QLatin1String("danger")) {
        return danger;
    }
    if (t == QLatin1String("border")) {
        return border;
    }
    const std::optional<ToneRef> tone = parseToneToken(t);
    if (!tone) {
        return std::nullopt;
    }
    if (tone->family == QLatin1String("accent")) {
        return mixTone(accent.isValid() ? accent : canvas, tone->weight);
    }
    if (tone->family == QLatin1String("bg")) {
        return mixTone(canvas, tone->weight);
    }
    const int fam = pickerFamilyIndex(tone->family);
    if (fam >= 0) {
        const QColor c = pickerPaletteColorAt(fam, tone->weight);
        if (c.isValid()) {
            return c;
        }
    }
    return std::nullopt;
}

std::optional<QColor> ThemeColors::resolveToken(const QString& token, const QColor& seed) const
{
    const QString t = token.trimmed();
    if (t.isEmpty()) {
        return std::nullopt;
    }
    if (const std::optional<QColor> named = namedColor(t, seed)) {
        return named;
    }
    const QColor c(t);
    if (c.isValid()) {
        return c;
    }
    return std::nullopt;
}

QColor ThemeColors::pageCanvas(const QString& pageBgToken) const
{
    const QColor fallback = bgMain.isValid() ? bgMain : QColor(10, 10, 11);
    if (pageBgToken.trimmed().isEmpty()) {
        return fallback;
    }
    return resolveToken(pageBgToken).value_or(fallback);
}

QColor ThemeColors::resolveFill(const QString& token, const QColor& seed, bool hovered,
                                bool active) const
{
    const QColor canvas = seed.isValid() ? seed : (bgMain.isValid() ? bgMain : QColor(10, 10, 11));
    const QString t = token.trimmed();
    const QColor rest = t.isEmpty() ? canvas : resolveToken(t, canvas).value_or(canvas);
    if (!hovered && !active) {
        return rest;
    }

    const ToneRef tone = toneRef(t);
    if (active) {
        if (tone.family == QLatin1String("bg") || tone.family == QLatin1String("neutral")) {
            const QColor shaded = shadeTowardBlack(rest, kHoverToneSteps, tone.weight);
            const QColor acc = accent.isValid() ? accent : rest;
            return mix(shaded, acc, kActiveAccentMix);
        }
        return shadeTowardBlack(canvas, kHoverToneSteps);
    }
    const int darker = steppedWeight(tone.weight, kHoverToneSteps);
    if (tone.family == QLatin1String("accent")) {
        return mixTone(accent.isValid() ? accent : canvas, darker);
    }
    if (tone.family == QLatin1String("bg")) {
        return mixTone(canvas, darker);
    }
    const int fam = pickerFamilyIndex(tone.family);
    if (fam >= 0) {
        const QColor stepped = pickerPaletteColorAt(fam, darker);
        return stepped.isValid() ? stepped : rest;
    }
    return mixTone(rest, darker);
}

QColor ThemeColors::readableForeground(const QColor& fill, const QString& authoredToken,
                                       const QColor& canvas) const
{
    const QColor behind = canvas.isValid() ? canvas : (bgMain.isValid() ? bgMain : QColor(10, 10, 11));
    QColor surface = fill;
    if (!surface.isValid() || surface.alpha() == 0) {
        surface = behind;
    } else if (surface.alpha() < 255) {
        QColor over = surface;
        over.setAlpha(255);
        surface = mix(behind, over, fill.alpha() / 255.0);
    }
    const QColor judged = shadeTowardBlack(surface, kFgHoldSteps, kFgHoldFromTone);
    auto pick = [&](const QColor& preferred) {
        if (preferred.isValid() && contrastRatio(preferred, judged) >= kReadableContrast) {
            return preferred;
        }
        return contrastOn(surface);
    };
    const QString t = authoredToken.trimmed();
    if (t.isEmpty() || t.compare(QLatin1String("foreground"), Qt::CaseInsensitive) == 0) {
        return pick(text);
    }
    const QColor seed = canvas.isValid() ? canvas : bgMain;
    if (const std::optional<QColor> authored = resolveToken(t, seed)) {
        return pick(*authored);
    }
    return pick(text);
}

QString themeAppearanceToString(ThemeAppearance a)
{
    switch (a) {
    case ThemeAppearance::Light:
        return QStringLiteral("light");
    case ThemeAppearance::LightTinted:
        return QStringLiteral("lightTinted");
    case ThemeAppearance::DarkTinted:
        return QStringLiteral("darkTinted");
    case ThemeAppearance::Dark:
    default:
        return QStringLiteral("dark");
    }
}

ThemeAppearance themeAppearanceFromString(const QString& s)
{
    const QString t = s.toLower();
    if (t == QLatin1String("light")) {
        return ThemeAppearance::Light;
    }
    if (t == QLatin1String("lighttinted") || t == QLatin1String("light_tinted")
        || t == QLatin1String("light-tinted")) {
        return ThemeAppearance::LightTinted;
    }
    if (t == QLatin1String("darktinted") || t == QLatin1String("dark_tinted")
        || t == QLatin1String("dark-tinted")) {
        return ThemeAppearance::DarkTinted;
    }
    return ThemeAppearance::Dark;
}

QString themeTintFamilyToString(ThemeTintFamily f)
{
    switch (f) {
    case ThemeTintFamily::Primary:
        return QStringLiteral("primary");
    case ThemeTintFamily::Complementary:
        return QStringLiteral("complementary");
    case ThemeTintFamily::Analogous1:
        return QStringLiteral("analogous1");
    case ThemeTintFamily::Analogous2:
        return QStringLiteral("analogous2");
    case ThemeTintFamily::Tertiary1:
        return QStringLiteral("tertiary1");
    case ThemeTintFamily::Tertiary2:
        return QStringLiteral("tertiary2");
    case ThemeTintFamily::None:
    default:
        return QStringLiteral("none");
    }
}

ThemeTintFamily themeTintFamilyFromString(const QString& s)
{
    const QString t = s.toLower();
    if (t == QLatin1String("primary")) {
        return ThemeTintFamily::Primary;
    }
    if (t == QLatin1String("complementary") || t == QLatin1String("complimentary")) {
        return ThemeTintFamily::Complementary;
    }
    if (t == QLatin1String("analogous") || t == QLatin1String("analogous1")) {
        return ThemeTintFamily::Analogous1;
    }
    if (t == QLatin1String("analogous2")) {
        return ThemeTintFamily::Analogous2;
    }
    if (t == QLatin1String("tertiary1") || t == QLatin1String("triadic1")) {
        return ThemeTintFamily::Tertiary1;
    }
    if (t == QLatin1String("tertiary2") || t == QLatin1String("tertiary")
        || t == QLatin1String("triadic") || t == QLatin1String("triadic2")) {
        return ThemeTintFamily::Tertiary2;
    }
    return ThemeTintFamily::None;
}

ThemeColors ThemeColors::darkPreset()
{
    ThemeColors c;
    c.bgMain = QColor(QStringLiteral("#0a0a0b"));
    c.border = QColor(QStringLiteral("#2a2c2e"));
    c.accent = QColor(QStringLiteral("#8ab4f8"));
    c.accentHover = QColor(QStringLiteral("#aecbfa"));
    c.text = QColor(QStringLiteral("#e3e3e3"));
    c.textSecondary = QColor(QStringLiteral("#7e8285"));
    c.danger = QColor(QStringLiteral("#f2b8b5"));
    c.progress = defaultProgressColor();
    c.appearance = ThemeAppearance::Dark;
    return c;
}

ThemeColors ThemeColors::lightPreset()
{
    ThemeColors c;
    c.bgMain = QColor(QStringLiteral("#f0f4f9"));
    c.border = QColor(QStringLiteral("#c4c7c5"));
    c.accent = QColor(QStringLiteral("#0b57d0"));
    c.accentHover = QColor(QStringLiteral("#0842a0"));
    c.text = QColor(QStringLiteral("#1f1f1f"));
    c.textSecondary = QColor(QStringLiteral("#444746"));
    c.danger = QColor(QStringLiteral("#b3261e"));
    c.progress = defaultProgressColor();
    c.appearance = ThemeAppearance::Light;
    return c;
}

QColor ThemeColors::parseColor(const QString& hex, const QColor& fallback)
{
    QColor c(hex);
    return c.isValid() ? c : fallback;
}

QColor ThemeColors::contrastOn(const QColor& fill)
{
    if (!fill.isValid() || fill.alpha() == 0) {
        return darkPreset().text;
    }
    const QColor judged = shadeTowardBlack(fill, kFgHoldSteps, kFgHoldFromTone);
    const QColor darkInk = lightPreset().text;
    const QColor lightInk = darkPreset().text;
    return contrastRatio(lightInk, judged) >= contrastRatio(darkInk, judged) ? lightInk : darkInk;
}

double ThemeColors::contrastRatio(const QColor& a, const QColor& b)
{
    auto lin = [](int ch) {
        const double s = ch / 255.0;
        return s <= 0.04045 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4);
    };
    auto lum = [&](const QColor& c) {
        const QColor x = c.isValid() ? c : QColor(0, 0, 0);
        return 0.2126 * lin(x.red()) + 0.7152 * lin(x.green()) + 0.0722 * lin(x.blue());
    };
    const double l1 = lum(a);
    const double l2 = lum(b);
    const double hi = l1 > l2 ? l1 : l2;
    const double lo = l1 > l2 ? l2 : l1;
    return (hi + 0.05) / (lo + 0.05);
}

QColor ThemeColors::mix(const QColor& a, const QColor& b, double t)
{
    t = qBound(0.0, t, 1.0);
    auto ch = [t](int x, int y) {
        return qBound(0, qRound(x + (y - x) * t), 255);
    };
    const QColor a2 = a.isValid() ? a : QColor(28, 28, 28);
    const QColor b2 = b.isValid() ? b : a2;
    return QColor(ch(a2.red(), b2.red()), ch(a2.green(), b2.green()), ch(a2.blue(), b2.blue()),
                  ch(a2.alpha(), b2.alpha()));
}

QString ThemeColors::colorToHex(const QColor& c)
{
    if (!c.isValid()) {
        return QStringLiteral("#000000");
    }
    if (c.alpha() < 255) {
        return QStringLiteral("#%1%2%3%4")
            .arg(c.alpha(), 2, 16, QLatin1Char('0'))
            .arg(c.red(), 2, 16, QLatin1Char('0'))
            .arg(c.green(), 2, 16, QLatin1Char('0'))
            .arg(c.blue(), 2, 16, QLatin1Char('0'))
            .toUpper();
    }
    return QStringLiteral("#%1%2%3")
        .arg(c.red(), 2, 16, QLatin1Char('0'))
        .arg(c.green(), 2, 16, QLatin1Char('0'))
        .arg(c.blue(), 2, 16, QLatin1Char('0'))
        .toUpper();
}

} // namespace gazer
