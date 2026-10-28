#include "assist/ElevenRequest.h"
#include "assist/VoiceStyles.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QtTest>

using namespace gazer;

class VoiceStylesTest final : public QObject {
    Q_OBJECT

private slots:
    void builtinsAndDefaults();
    void normalizeAndParse();
    void addFavoriteTrash();
    void standingTag();
};

void VoiceStylesTest::builtinsAndDefaults()
{
    const QStringList styles = VoiceStyles::builtinStyles();
    QCOMPARE(styles.size(), 13);
    QCOMPARE(styles.first(), QStringLiteral("anxious"));
    QCOMPARE(styles.at(10), QStringLiteral("sing loud"));
    const VoiceStyles::Catalog catalog = VoiceStyles::defaultCatalog();
    QCOMPARE(catalog.favorites,
             (QStringList{QStringLiteral("begging"), QStringLiteral("strict"),
                          QStringLiteral("flirt"), QStringLiteral("happy"),
                          QStringLiteral("indignant"), QStringLiteral("sing loud")}));
    QVERIFY(VoiceStyles::parseCatalog(QJsonValue()).styles.size() == styles.size());
    QVERIFY(VoiceStyles::parseCatalog(QJsonObject()).styles.size() == styles.size());
}

void VoiceStylesTest::normalizeAndParse()
{
    QCOMPARE(VoiceStyles::normalizeStyleName(QStringLiteral("  [Happy]  ")), QStringLiteral("Happy"));
    QVERIFY(VoiceStyles::normalizeStyleName(QStringLiteral("none")).isEmpty());
    QVERIFY(VoiceStyles::normalizeStyleName(QStringLiteral("   ")).isEmpty());
    QCOMPARE(VoiceStyles::normalizeStyleName(QString(50, QLatin1Char('x'))).size(),
             VoiceStyles::kNameMax);
    QCOMPARE(VoiceStyles::keyOf(QStringLiteral(" Happy ")), QStringLiteral("happy"));

    QJsonObject raw;
    raw.insert(QStringLiteral("styles"),
               QJsonArray{QStringLiteral("  Happy "), QStringLiteral("[happy]"),
                          QStringLiteral("none"), QString(), QStringLiteral("soft  night")});
    raw.insert(QStringLiteral("favorites"), QJsonArray{QStringLiteral("HAPPY")});
    const VoiceStyles::Catalog parsed = VoiceStyles::parseCatalog(raw);
    QCOMPARE(parsed.styles, (QStringList{QStringLiteral("Happy"), QStringLiteral("soft night")}));
    QCOMPARE(parsed.favorites, QStringList{QStringLiteral("Happy")});

    QJsonObject empty;
    empty.insert(QStringLiteral("styles"), QJsonArray{});
    empty.insert(QStringLiteral("favorites"), QJsonArray{QStringLiteral("happy")});
    const VoiceStyles::Catalog blank = VoiceStyles::parseCatalog(empty);
    QVERIFY(blank.styles.isEmpty());
    QVERIFY(blank.favorites.isEmpty());

    const QJsonObject saved = VoiceStyles::serialize(parsed);
    QVERIFY(!saved.contains(QStringLiteral("noneFavorite")));
    const VoiceStyles::Catalog round = VoiceStyles::parseCatalog(saved);
    QCOMPARE(round.styles, parsed.styles);
    QCOMPARE(round.favorites, parsed.favorites);
}

void VoiceStylesTest::addFavoriteTrash()
{
    VoiceStyles::Catalog catalog;
    const VoiceStyles::Result added = VoiceStyles::addStyle(catalog, QStringLiteral(" [soft] "));
    QVERIFY(added.ok);
    QCOMPARE(added.name, QStringLiteral("soft"));
    QCOMPARE(added.catalog.styles, QStringList{QStringLiteral("soft")});
    QCOMPARE(added.catalog.favorites, QStringList{QStringLiteral("soft")});

    const VoiceStyles::Result duplicate =
        VoiceStyles::addStyle(added.catalog, QStringLiteral("SOFT"));
    QVERIFY(!duplicate.ok);
    QCOMPARE(duplicate.reason, QStringLiteral("duplicate"));
    const VoiceStyles::Result empty = VoiceStyles::addStyle(added.catalog, QStringLiteral("none"));
    QVERIFY(!empty.ok);
    QCOMPARE(empty.reason, QStringLiteral("empty"));

    VoiceStyles::Catalog starred = VoiceStyles::toggleFavorite(added.catalog, QStringLiteral(" Happy "));
    QCOMPARE(starred.favorites, QStringList{QStringLiteral("soft")});
    starred = VoiceStyles::toggleFavorite(added.catalog, QStringLiteral(" soft "));
    QVERIFY(starred.favorites.isEmpty());
    starred = VoiceStyles::toggleFavorite(starred, QStringLiteral("soft"));
    QVERIFY(VoiceStyles::isFavorite(starred, QStringLiteral("SOFT")));

    const VoiceStyles::Result trashed =
        VoiceStyles::trashStyle(starred, QStringLiteral("[soft]"));
    QVERIFY(trashed.ok);
    QVERIFY(trashed.catalog.styles.isEmpty());
    const VoiceStyles::Result missing =
        VoiceStyles::trashStyle(starred, QStringLiteral("none"));
    QVERIFY(!missing.ok);
    QCOMPARE(missing.reason, QStringLiteral("missing"));
}

void VoiceStylesTest::standingTag()
{
    QCOMPARE(ElevenRequest::applyStandingTag(QStringLiteral("hello"), QString()),
             QStringLiteral("hello"));
    QCOMPARE(ElevenRequest::applyStandingTag(QStringLiteral("hello"), QStringLiteral("happy")),
             QStringLiteral("[happy] hello"));
    QCOMPARE(ElevenRequest::applyStandingTag(QStringLiteral("  [Happy] hello"),
                                             QStringLiteral("happy")),
             QStringLiteral("  [Happy] hello"));
    QCOMPARE(ElevenRequest::applyStandingTag(QStringLiteral("hello"), QStringLiteral("[soft night]")),
             QStringLiteral("[soft night] hello"));
    QCOMPARE(ElevenRequest::applyStandingTag(QStringLiteral("hello"), QStringLiteral("a  b")),
             QStringLiteral("[a b] hello"));
    QCOMPARE(ElevenRequest::applyStandingTag(QString(), QStringLiteral("yell")),
             QStringLiteral("[yell] "));
}

QObject* createVoiceStylesTest()
{
    return new VoiceStylesTest;
}

#include "VoiceStylesTest.moc"
