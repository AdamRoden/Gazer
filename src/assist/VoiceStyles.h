#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QStringView>

namespace gazer {
namespace VoiceStyles {

/// Voice (`js/voice-styles.js`): "none" is not a style name.
inline constexpr int kNameMax = 40;
inline constexpr QStringView kNone = u"none";

struct Catalog {
    QStringList styles;
    QStringList favorites;
};

struct Result {
    bool ok = false;
    QString reason;
    QString name;
    Catalog catalog;
};

[[nodiscard]] QStringList builtinStyles();
[[nodiscard]] QString keyOf(QStringView name);
/// Trim, drop one wrapping bracket pair, drop leftover brackets, collapse
/// whitespace. "none" and an empty result are not style names. Cap is 40.
[[nodiscard]] QString normalizeStyleName(QString raw);
[[nodiscard]] Catalog defaultCatalog();
/// Null, or an object without a `styles` array, is the built-in catalog.
/// An empty `styles` array is a real empty catalog.
[[nodiscard]] Catalog parseCatalog(const QJsonValue& raw);
/// `{styles, favorites}` only.
[[nodiscard]] QJsonObject serialize(const Catalog& catalog);
[[nodiscard]] Result addStyle(Catalog catalog, const QString& raw);
[[nodiscard]] Catalog toggleFavorite(Catalog catalog, const QString& name);
[[nodiscard]] Result trashStyle(Catalog catalog, const QString& name);
[[nodiscard]] bool isFavorite(const Catalog& catalog, const QString& name);

} // namespace VoiceStyles
} // namespace gazer
