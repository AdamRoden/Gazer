#include "assist/VoiceStyles.h"

#include <QJsonArray>
#include <QSet>

namespace gazer {
namespace VoiceStyles {
namespace {

Catalog cloneCatalog(const Catalog& catalog)
{
    Catalog out;
    out.styles = catalog.styles;
    out.favorites = catalog.favorites;
    return out;
}

} // namespace

QStringList builtinStyles()
{
    return {QStringLiteral("anxious"),     QStringLiteral("begging"), QStringLiteral("strict"),
            QStringLiteral("dull"),        QStringLiteral("excited"), QStringLiteral("flirt"),
            QStringLiteral("happy"),       QStringLiteral("indignant"), QStringLiteral("sad"),
            QStringLiteral("sing"),        QStringLiteral("sing loud"), QStringLiteral("whisper"),
            QStringLiteral("yell")};
}

QString keyOf(QStringView name)
{
    return name.toString().trimmed().toLower();
}

QString normalizeStyleName(QString raw)
{
    QString t = raw.trimmed();
    if (t.size() >= 2 && t.startsWith(QLatin1Char('[')) && t.endsWith(QLatin1Char(']'))) {
        t = t.mid(1, t.size() - 2).trimmed();
    }
    t.remove(QLatin1Char('['));
    t.remove(QLatin1Char(']'));
    t = t.simplified();
    if (t.isEmpty() || keyOf(t) == kNone) {
        return {};
    }
    if (t.size() > kNameMax) {
        t = t.left(kNameMax).trimmed();
    }
    if (t.isEmpty() || keyOf(t) == kNone) {
        return {};
    }
    return t;
}

Catalog defaultCatalog()
{
    Catalog cat;
    cat.styles = builtinStyles();
    const QStringList starred = {QStringLiteral("begging"), QStringLiteral("strict"),
                                 QStringLiteral("flirt"),   QStringLiteral("happy"),
                                 QStringLiteral("indignant"), QStringLiteral("sing loud")};
    QSet<QString> keys;
    for (const QString& name : starred) {
        keys.insert(keyOf(name));
    }
    for (const QString& name : cat.styles) {
        if (keys.contains(keyOf(name))) {
            cat.favorites.push_back(name);
        }
    }
    return cat;
}

Catalog parseCatalog(const QJsonValue& raw)
{
    if (!raw.isObject()) {
        return defaultCatalog();
    }
    const QJsonObject obj = raw.toObject();
    const QJsonValue stylesVal = obj.value(QStringLiteral("styles"));
    if (!stylesVal.isArray()) {
        return defaultCatalog();
    }
    Catalog cat;
    QSet<QString> seen;
    for (const QJsonValue& item : stylesVal.toArray()) {
        const QString name = normalizeStyleName(item.toString());
        const QString key = keyOf(name);
        if (key.isEmpty() || seen.contains(key)) {
            continue;
        }
        seen.insert(key);
        cat.styles.push_back(name);
    }
    QSet<QString> favKeys;
    const QJsonValue favVal = obj.value(QStringLiteral("favorites"));
    if (favVal.isArray()) {
        for (const QJsonValue& item : favVal.toArray()) {
            const QString key = keyOf(normalizeStyleName(item.toString()));
            if (!key.isEmpty() && seen.contains(key)) {
                favKeys.insert(key);
            }
        }
    }
    for (const QString& name : cat.styles) {
        if (favKeys.contains(keyOf(name))) {
            cat.favorites.push_back(name);
        }
    }
    return cat;
}

QJsonObject serialize(const Catalog& catalog)
{
    const Catalog next = cloneCatalog(catalog);
    QJsonArray styles;
    for (const QString& name : next.styles) {
        styles.append(name);
    }
    QJsonArray favorites;
    for (const QString& name : next.favorites) {
        favorites.append(name);
    }
    QJsonObject out;
    out.insert(QStringLiteral("styles"), styles);
    out.insert(QStringLiteral("favorites"), favorites);
    return out;
}

Result addStyle(Catalog catalog, const QString& raw)
{
    const QString name = normalizeStyleName(raw);
    Result result;
    result.catalog = cloneCatalog(catalog);
    if (name.isEmpty()) {
        result.reason = QStringLiteral("empty");
        return result;
    }
    const QString key = keyOf(name);
    for (const QString& style : result.catalog.styles) {
        if (keyOf(style) == key) {
            result.reason = QStringLiteral("duplicate");
            result.name = name;
            return result;
        }
    }
    result.catalog.styles.push_back(name);
    result.catalog.favorites.push_back(name);
    result.ok = true;
    result.name = name;
    return result;
}

Catalog toggleFavorite(Catalog catalog, const QString& name)
{
    Catalog current = cloneCatalog(catalog);
    const QString key = keyOf(name);
    bool found = false;
    for (const QString& style : current.styles) {
        if (keyOf(style) == key) {
            found = true;
            break;
        }
    }
    if (!found) {
        return current;
    }
    QSet<QString> favKeys;
    for (const QString& style : current.favorites) {
        favKeys.insert(keyOf(style));
    }
    if (favKeys.contains(key)) {
        favKeys.remove(key);
    } else {
        favKeys.insert(key);
    }
    current.favorites.clear();
    for (const QString& style : current.styles) {
        if (favKeys.contains(keyOf(style))) {
            current.favorites.push_back(style);
        }
    }
    return current;
}

Result trashStyle(Catalog catalog, const QString& name)
{
    Result result;
    result.catalog = cloneCatalog(catalog);
    const QString key = keyOf(normalizeStyleName(name));
    bool found = false;
    if (!key.isEmpty()) {
        for (const QString& style : result.catalog.styles) {
            if (keyOf(style) == key) {
                found = true;
                break;
            }
        }
    }
    if (!found) {
        result.reason = QStringLiteral("missing");
        return result;
    }
    QStringList styles;
    for (const QString& style : result.catalog.styles) {
        if (keyOf(style) != key) {
            styles.push_back(style);
        }
    }
    QStringList favorites;
    for (const QString& style : result.catalog.favorites) {
        if (keyOf(style) != key) {
            favorites.push_back(style);
        }
    }
    result.catalog.styles = std::move(styles);
    result.catalog.favorites = std::move(favorites);
    result.ok = true;
    return result;
}

bool isFavorite(const Catalog& catalog, const QString& name)
{
    const QString key = keyOf(name);
    if (key.isEmpty()) {
        return false;
    }
    for (const QString& style : catalog.favorites) {
        if (keyOf(style) == key) {
            return true;
        }
    }
    return false;
}

} // namespace VoiceStyles
} // namespace gazer
