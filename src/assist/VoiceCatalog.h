#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

class QJsonObject;

namespace gazer {
namespace VoiceCatalog {

constexpr int kPageSize = 12;
constexpr int kLangChips = 2;

struct Voice {
    QString id;
    QString name;
    QString gender;
    QString language;
    bool eleven = true;
};

[[nodiscard]] QVector<Voice> parseElevenCache(const QJsonObject& cache);
[[nodiscard]] QVector<Voice> filter(const QVector<Voice>& in, const QString& gender,
                                    const QString& language);
void sortFavoritesFirst(QVector<Voice>* voices, const QStringList& favoriteIds);
[[nodiscard]] QStringList languageChips(const QVector<Voice>& voices, int maxChips = kLangChips);
[[nodiscard]] QVector<Voice> page(const QVector<Voice>& voices, int pageIndex, int* pageCount);
[[nodiscard]] QString encodeId(const QString& id);
[[nodiscard]] QString decodeId(const QString& encoded);

} // namespace VoiceCatalog
} // namespace gazer
