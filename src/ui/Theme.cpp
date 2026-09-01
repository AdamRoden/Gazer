#include "ui/Theme.h"
#include "ui/ThemeScheme.h"

#include <QtGlobal>

#include <cmath>

namespace gazer {

bool ThemeColors::isNamedColor(const QString& name)
{
    const QString t = name.trimmed().toLower();
    if (t == QLatin1String("background") || t == QLatin1String("surface")
        || t == QLatin1String("accent") || t == QLatin1String("progress")
        || t == QLatin1String("tertiary") || t == QLatin1String("foreground")
        || t == QLatin1String("danger")) {
        return true;
    }
    for (int i = 0; i < ThemeScheme::brandCount(); ++i) {
        if (t == QLatin1String(ThemeScheme::brands()[i].key)) {
            return true;
        }
    }
    return false;
}

std::optional<QColor> ThemeColors::namedColor(const QString& name) const
{
    const QString t = name.trimmed().toLower();
    if (t == QLatin1String("background")) {
        return bgMain;
    }
    if (t == QLatin1String("surface")) {
        return cellBg.isValid() ? cellBg : bgSurface;
    }
    if (t == QLatin1String("accent")) {
        return accent;
    }
    if (t == QLatin1String("progress")) {
        return progress.isValid() ? progress : defaultProgressColor();
    }
    if (t == QLatin1String("tertiary")) {
        return cellActive.isValid() ? cellActive : bgSurfaceActive;
    }
    if (t == QLatin1String("foreground")) {
        return text;
    }
    if (t == QLatin1String("danger")) {
        return danger;
    }
    for (int i = 0; i < ThemeScheme::brandCount(); ++i) {
        if (t == QLatin1String(ThemeScheme::brands()[i].key)) {
            QColor c = ThemeScheme::brandAccent(i, appearance);
            c.setAlpha(kNamedBrandFillAlpha);
            return c;
        }
    }
    return std::nullopt;
}

std::optional<QColor> ThemeColors::resolveToken(const QString& token) const
{
    const QString t = token.trimmed();
    if (t.isEmpty()) {
        return std::nullopt;
    }
    if (const std::optional<QColor> named = namedColor(t)) {
        return named;
    }
    const QColor c(t);
    if (c.isValid()) {
        return c;
    }
    return std::nullopt;
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

QVector<QString> voiceColorPalette()
{
    return {
        QStringLiteral("#a0a0a0"), QStringLiteral("#cc0000"), QStringLiteral("#e69138"),
        QStringLiteral("#f1c232"), QStringLiteral("#6aa84f"), QStringLiteral("#45818e"),
        QStringLiteral("#3c78d8"), QStringLiteral("#3d85c6"), QStringLiteral("#674ea7"),
        QStringLiteral("#a64d79"), QStringLiteral("#808080"), QStringLiteral("#990000"),
        QStringLiteral("#b45f06"), QStringLiteral("#bf9000"), QStringLiteral("#38761d"),
        QStringLiteral("#134f5c"), QStringLiteral("#1155cc"), QStringLiteral("#0b5394"),
        QStringLiteral("#351c75"), QStringLiteral("#741b47"), QStringLiteral("#606060"),
        QStringLiteral("#660000"), QStringLiteral("#783f04"), QStringLiteral("#7f6000"),
        QStringLiteral("#274e13"), QStringLiteral("#0c343d"), QStringLiteral("#1c4587"),
        QStringLiteral("#073763"), QStringLiteral("#20124d"), QStringLiteral("#4c1130"),
        QStringLiteral("#000000"), QStringLiteral("#ffffff"), QStringLiteral("#dddddd"),
        QStringLiteral("#bbbbbb"), QStringLiteral("#999999"), QStringLiteral("#777777"),
        QStringLiteral("#555555"), QStringLiteral("#333333"), QStringLiteral("#111111"),
    };
}

ThemeColors ThemeColors::darkPreset()
{
    ThemeColors c;
    c.bgMain = QColor(QStringLiteral("#0a0a0b"));
    c.bgSurface = QColor(QStringLiteral("#121314"));
    c.bgSurfaceHover = QColor(QStringLiteral("#1a1b1c"));
    c.bgSurfaceActive = QColor(QStringLiteral("#222426"));
    c.border = QColor(QStringLiteral("#2a2c2e"));
    c.accent = QColor(QStringLiteral("#8ab4f8"));
    c.accentHover = QColor(QStringLiteral("#aecbfa"));
    c.text = QColor(QStringLiteral("#e3e3e3"));
    c.textSecondary = QColor(QStringLiteral("#7e8285"));
    c.cellBg = QColor(QStringLiteral("#1a1b1c"));
    c.cellHover = QColor(QStringLiteral("#222426"));
    c.cellActive = QColor(QStringLiteral("#2c2e30"));
    c.danger = QColor(QStringLiteral("#f2b8b5"));
    c.progress = defaultProgressColor();
    c.appearance = ThemeAppearance::Dark;
    return c;
}

ThemeColors ThemeColors::lightPreset()
{
    ThemeColors c;
    c.bgMain = QColor(QStringLiteral("#f0f4f9"));
    c.bgSurface = QColor(QStringLiteral("#ffffff"));
    c.bgSurfaceHover = QColor(QStringLiteral("#e9eef6"));
    c.bgSurfaceActive = QColor(QStringLiteral("#dde3ea"));
    c.border = QColor(QStringLiteral("#c4c7c5"));
    c.accent = QColor(QStringLiteral("#0b57d0"));
    c.accentHover = QColor(QStringLiteral("#0842a0"));
    c.text = QColor(QStringLiteral("#1f1f1f"));
    c.textSecondary = QColor(QStringLiteral("#444746"));
    c.cellBg = QColor(QStringLiteral("#e9eef6"));
    c.cellHover = QColor(QStringLiteral("#dde3ea"));
    c.cellActive = QColor(QStringLiteral("#c4c7c5"));
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
    return fill.lightness() > 140 ? lightPreset().text : darkPreset().text;
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

QJsonObject ThemeColors::toJson() const
{
    QJsonObject o;
    o.insert(QStringLiteral("bgMain"), colorToHex(bgMain));
    o.insert(QStringLiteral("bgSurface"), colorToHex(bgSurface));
    o.insert(QStringLiteral("bgSurfaceHover"), colorToHex(bgSurfaceHover));
    o.insert(QStringLiteral("bgSurfaceActive"), colorToHex(bgSurfaceActive));
    o.insert(QStringLiteral("border"), colorToHex(border));
    o.insert(QStringLiteral("accent"), colorToHex(accent));
    o.insert(QStringLiteral("accentHover"), colorToHex(accentHover));
    o.insert(QStringLiteral("text"), colorToHex(text));
    o.insert(QStringLiteral("textSecondary"), colorToHex(textSecondary));
    o.insert(QStringLiteral("cellBg"), colorToHex(cellBg));
    o.insert(QStringLiteral("cellHover"), colorToHex(cellHover));
    o.insert(QStringLiteral("cellActive"), colorToHex(cellActive));
    o.insert(QStringLiteral("danger"), colorToHex(danger));
    return o;
}

void ThemeColors::fromJson(const QJsonObject& o)
{
    auto set = [&](QColor& dest, const char* key) {
        if (!o.contains(QLatin1String(key))) {
            return;
        }
        dest = parseColor(o.value(QLatin1String(key)).toString(), dest);
    };
    set(bgMain, "bgMain");
    set(bgSurface, "bgSurface");
    set(bgSurfaceHover, "bgSurfaceHover");
    set(bgSurfaceActive, "bgSurfaceActive");
    set(border, "border");
    set(accent, "accent");
    set(accentHover, "accentHover");
    set(text, "text");
    set(textSecondary, "textSecondary");
    set(cellBg, "cellBg");
    set(cellHover, "cellHover");
    set(cellActive, "cellActive");
    set(danger, "danger");
}

} // namespace gazer
