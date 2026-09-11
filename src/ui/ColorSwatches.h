#pragma once

#include <QStringList>

namespace gazer {

/// Fixed composer / color-picker palette (document colors).
inline const QStringList& paletteColors()
{
    static const QStringList k = {
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
    return k;
}

} // namespace gazer
