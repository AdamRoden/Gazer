#pragma once

#include "assist/SoundboardStore.h"

#include <QJsonObject>
#include <QtGlobal>

namespace gazer::soundboard_json {

inline QString readString(const QJsonObject& o, const char* key)
{
    return o.value(QLatin1String(key)).toString();
}

inline int readInt(const QJsonObject& o, const char* key, int fallback)
{
    const QJsonValue v = o.value(QLatin1String(key));
    if (v.isDouble() || v.isString()) {
        return v.toInt(fallback);
    }
    return fallback;
}

inline SoundboardButton buttonFromJson(const QJsonObject& o)
{
    SoundboardButton b;
    b.id = readString(o, "id").trimmed();
    b.label = readString(o, "label");
    b.icon = readString(o, "icon").trimmed();
    if (b.icon.isEmpty()) {
        b.icon = readString(o, "symbol").trimmed();
    }
    b.color = readString(o, "color");
    b.sourceText = readString(o, "sourceText");
    b.utteranceText = readString(o, "utteranceText");
    if (b.sourceText.isEmpty() && b.utteranceText.isEmpty()) {
        const QString text = readString(o, "text").trimmed();
        if (!text.isEmpty()) {
            b.sourceText = text;
            b.utteranceText = text;
        }
    }
    b.clipId = readString(o, "clipId").trimmed();
    b.effectsBaked = o.value(QStringLiteral("effectsBaked")).toBool(false);
    if (o.contains(QLatin1String("col"))) {
        b.col = readInt(o, "col", 0);
    } else if (o.contains(QLatin1String("x"))) {
        b.col = qMax(0, int(o.value(QLatin1String("x")).toDouble() / 100.0));
    }
    if (o.contains(QLatin1String("row"))) {
        b.row = readInt(o, "row", 0);
    } else if (o.contains(QLatin1String("y"))) {
        b.row = qMax(0, int(o.value(QLatin1String("y")).toDouble() / 80.0));
    }
    b.colSpan = qMax(1, readInt(o, "colSpan", 1));
    b.rowSpan = qMax(1, readInt(o, "rowSpan", 1));
    return b;
}

} // namespace gazer::soundboard_json
