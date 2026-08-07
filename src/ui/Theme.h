#pragma once

#include <QColor>
#include <QString>
#include <QVector>

namespace gazer {

/// Voice-aligned chrome palette (light / dark / custom bags).
struct ThemeColors {
    QColor bgMain;
    QColor bgSurface;
    QColor bgSurfaceHover;
    QColor bgSurfaceActive;
    QColor border;
    QColor accent;
    QColor accentHover;
    QColor text;
    QColor textSecondary;
    QColor cellBg;
    QColor cellHover;
    QColor cellActive;
    QColor danger;

    [[nodiscard]] static ThemeColors darkPreset();
    [[nodiscard]] static ThemeColors lightPreset();
};

enum class ThemeMode {
    Dark,
    Light,
    Custom
};

[[nodiscard]] inline QString themeModeToString(ThemeMode m)
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

[[nodiscard]] inline ThemeMode themeModeFromString(const QString& s)
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

/// Voice sample palette (from Voice/js/app.js COLOR_PALETTE).
[[nodiscard]] inline QVector<QString> voiceColorPalette()
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

inline ThemeColors ThemeColors::darkPreset()
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

inline ThemeColors ThemeColors::lightPreset()
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

} // namespace gazer
