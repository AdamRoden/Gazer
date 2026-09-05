#include "assist/VoiceCatalog.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QtGlobal>
#include <QUrl>
#include <algorithm>

namespace gazer {
namespace VoiceCatalog {
namespace {

QString normalizeGender(QString g)
{
    g = g.trimmed().toLower();
    if (g.startsWith(QLatin1Char('f'))) {
        return QStringLiteral("female");
    }
    if (g.startsWith(QLatin1Char('m'))) {
        return QStringLiteral("male");
    }
    return {};
}

QString normalizeLang(QString lang)
{
    lang = lang.trimmed().toLower();
    if (lang.size() >= 2 && lang[0].isLetter() && lang[1].isLetter()) {
        return lang.left(2);
    }
    return lang;
}

QString languageOf(const QJsonObject& v)
{
    const QJsonObject labels = v.value(QStringLiteral("labels")).toObject();
    QString lang = normalizeLang(labels.value(QStringLiteral("language")).toString());
    if (!lang.isEmpty()) {
        return lang;
    }
    const QJsonArray verified = v.value(QStringLiteral("verified_languages")).toArray();
    if (!verified.isEmpty()) {
        lang = normalizeLang(verified.at(0).toObject().value(QStringLiteral("language")).toString());
        if (!lang.isEmpty()) {
            return lang;
        }
    }
    return normalizeLang(labels.value(QStringLiteral("accent")).toString());
}

} // namespace

QVector<Voice> parseElevenCache(const QJsonObject& cache)
{
    QVector<Voice> out;
    const QJsonArray voices = cache.value(QStringLiteral("voices")).toArray();
    out.reserve(voices.size());
    for (const QJsonValue& item : voices) {
        const QJsonObject v = item.toObject();
        Voice row;
        row.id = v.value(QStringLiteral("voice_id")).toString().trimmed();
        row.name = v.value(QStringLiteral("name")).toString().trimmed();
        if (row.id.isEmpty() || row.name.isEmpty()) {
            continue;
        }
        const QJsonObject labels = v.value(QStringLiteral("labels")).toObject();
        row.gender = normalizeGender(labels.value(QStringLiteral("gender")).toString());
        row.language = languageOf(v);
        row.eleven = true;
        out.push_back(row);
    }
    return out;
}

QVector<Voice> filter(const QVector<Voice>& in, const QString& gender, const QString& language)
{
    const QString g = normalizeGender(gender);
    const QString lang = normalizeLang(language);
    QVector<Voice> out;
    out.reserve(in.size());
    for (const Voice& v : in) {
        if (!g.isEmpty() && v.gender != g) {
            continue;
        }
        if (!lang.isEmpty() && lang != QLatin1String("all") && v.language != lang) {
            continue;
        }
        out.push_back(v);
    }
    return out;
}

void sortFavoritesFirst(QVector<Voice>* voices, const QStringList& favoriteIds)
{
    if (!voices) {
        return;
    }
    QHash<QString, int> rank;
    for (int i = 0; i < favoriteIds.size(); ++i) {
        const QString id = favoriteIds[i].trimmed();
        if (!id.isEmpty() && !rank.contains(id)) {
            rank.insert(id, i);
        }
    }
    std::sort(voices->begin(), voices->end(), [&](const Voice& a, const Voice& b) {
        const int ra = rank.contains(a.id) ? rank.value(a.id) : 1000;
        const int rb = rank.contains(b.id) ? rank.value(b.id) : 1000;
        if (ra != rb) {
            return ra < rb;
        }
        return QString::compare(a.name, b.name, Qt::CaseInsensitive) < 0;
    });
}

QStringList languageChips(const QVector<Voice>& voices, int maxChips)
{
    QHash<QString, int> counts;
    for (const Voice& v : voices) {
        if (!v.language.isEmpty()) {
            counts[v.language] += 1;
        }
    }
    QVector<QPair<int, QString>> ranked;
    ranked.reserve(counts.size());
    for (auto it = counts.constBegin(); it != counts.constEnd(); ++it) {
        ranked.push_back({it.value(), it.key()});
    }
    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
        if (a.first != b.first) {
            return a.first > b.first;
        }
        return a.second < b.second;
    });
    QStringList out;
    const int n = qMin(maxChips, int(ranked.size()));
    for (int i = 0; i < n; ++i) {
        out.push_back(ranked[i].second);
    }
    return out;
}

QVector<Voice> page(const QVector<Voice>& voices, int pageIndex, int* pageCount)
{
    const int pages = qMax(1, (voices.size() + kPageSize - 1) / kPageSize);
    if (pageCount) {
        *pageCount = pages;
    }
    const int p = qBound(0, pageIndex, pages - 1);
    const int start = p * kPageSize;
    return voices.mid(start, kPageSize);
}

QString encodeId(const QString& id)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(id));
}

QString decodeId(const QString& encoded)
{
    return QUrl::fromPercentEncoding(encoded.toLatin1());
}

} // namespace VoiceCatalog
} // namespace gazer
