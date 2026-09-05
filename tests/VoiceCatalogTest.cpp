#include "assist/VoiceCatalog.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QtTest>

using namespace gazer;

class VoiceCatalogTest final : public QObject {
    Q_OBJECT

private slots:
    void parseAndFilter();
    void favoritesSortAndPage();
    void encodeRoundTrip();
};

QJsonObject sampleCache()
{
    QJsonArray voices;
    auto add = [&](const char* id, const char* name, const char* gender, const char* lang) {
        QJsonObject labels;
        labels.insert(QStringLiteral("gender"), QLatin1String(gender));
        labels.insert(QStringLiteral("language"), QLatin1String(lang));
        QJsonObject v;
        v.insert(QStringLiteral("voice_id"), QLatin1String(id));
        v.insert(QStringLiteral("name"), QLatin1String(name));
        v.insert(QStringLiteral("labels"), labels);
        voices.append(v);
    };
    add("aaa", "Alice", "female", "en");
    add("bbb", "Bob", "male", "en");
    add("ccc", "Carlos", "male", "es");
    add("ddd", "Dina", "female", "de");
    QJsonObject root;
    root.insert(QStringLiteral("voices"), voices);
    return root;
}

void VoiceCatalogTest::parseAndFilter()
{
    const auto all = VoiceCatalog::parseElevenCache(sampleCache());
    QCOMPARE(all.size(), 4);
    QCOMPARE(all[0].name, QStringLiteral("Alice"));
    QCOMPARE(all[0].gender, QStringLiteral("female"));
    QCOMPARE(all[0].language, QStringLiteral("en"));

    const auto females = VoiceCatalog::filter(all, QStringLiteral("female"), {});
    QCOMPARE(females.size(), 2);
    const auto enMale = VoiceCatalog::filter(all, QStringLiteral("male"), QStringLiteral("en"));
    QCOMPARE(enMale.size(), 1);
    QCOMPARE(enMale[0].id, QStringLiteral("bbb"));

    const QStringList chips = VoiceCatalog::languageChips(all, 2);
    QCOMPARE(chips.size(), 2);
    QCOMPARE(chips[0], QStringLiteral("en"));
}

void VoiceCatalogTest::favoritesSortAndPage()
{
    auto all = VoiceCatalog::parseElevenCache(sampleCache());
    VoiceCatalog::sortFavoritesFirst(&all, {QStringLiteral("ccc"), QStringLiteral("aaa")});
    QCOMPARE(all[0].id, QStringLiteral("ccc"));
    QCOMPARE(all[1].id, QStringLiteral("aaa"));

    QVector<VoiceCatalog::Voice> many;
    for (int i = 0; i < 25; ++i) {
        VoiceCatalog::Voice v;
        v.id = QString::number(i);
        v.name = QStringLiteral("V%1").arg(i, 2, 10, QLatin1Char('0'));
        many.push_back(v);
    }
    int pages = 0;
    const auto p0 = VoiceCatalog::page(many, 0, &pages);
    QCOMPARE(pages, 3);
    QCOMPARE(p0.size(), 12);
    QCOMPARE(p0[0].id, QStringLiteral("0"));
    const auto p2 = VoiceCatalog::page(many, 9, &pages);
    QCOMPARE(p2.size(), 1);
    QCOMPARE(p2[0].id, QStringLiteral("24"));
}

void VoiceCatalogTest::encodeRoundTrip()
{
    const QString raw = QStringLiteral("HKEY\\Tokens\\David");
    const QString enc = VoiceCatalog::encodeId(raw);
    QVERIFY(!enc.contains(QLatin1Char('\\')));
    QCOMPARE(VoiceCatalog::decodeId(enc), raw);
}

QObject* createVoiceCatalogTest()
{
    return new VoiceCatalogTest;
}

#include "VoiceCatalogTest.moc"
