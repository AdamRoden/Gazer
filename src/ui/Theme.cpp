#include "ui/Theme.h"

namespace gazer {

QString themeModeToString(ThemeMode m)
{
    switch (m) {
    case ThemeMode::Light:
        return QStringLiteral("light");
    case ThemeMode::Custom:
        return QStringLiteral("custom");
    case ThemeMode::Dark:
    default:
        return QStringLiteral("dark");
    }
}

ThemeMode themeModeFromString(const QString& s)
{
    const QString t = s.toLower();
    if (t == QLatin1String("light")) {
        return ThemeMode::Light;
    }
    if (t == QLatin1String("custom")) {
        return ThemeMode::Custom;
    }
    return ThemeMode::Dark;
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
