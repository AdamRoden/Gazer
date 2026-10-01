#include "assist/ElevenClient.h"
#include "assist/ElevenRequest.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>
#include <limits>

using namespace gazer;
using namespace gazer::ElevenRequest;

class ElevenRequestTest final : public QObject {
    Q_OBJECT

private slots:
    void normalizeAliasesAndUnknown();
    void isElevenAfterNormalize();
    void tagsDetectAndStrip();
    void tagsStayOnTurbo();
    void localSpeedClamp_data();
    void localSpeedClamp();
    void sapiStripsTags();
    void nonFiniteSpeedDefaults();
    void dialogueMessages();
    void voicesCacheRoundTrip();
    void voicesCacheTtl();
};

void ElevenRequestTest::normalizeAliasesAndUnknown()
{
    QCOMPARE(normalizeModelId(QStringLiteral("eleven_v4_turbo")), QString(kModelTurbo));
    QCOMPARE(normalizeModelId(QStringLiteral("eleven_v4")), QString(kModelTurbo));
    QCOMPARE(normalizeModelId(QStringLiteral("eleven_v3")), QString(kModelTurbo));
    QCOMPARE(normalizeModelId(QStringLiteral("eleven_flash_v2_5")), QString(kModelTurbo));
    QCOMPARE(normalizeModelId(QStringLiteral("eleven_flash_v2")), QString(kModelTurbo));
    QCOMPARE(normalizeModelId(QStringLiteral("eleven_multilingual_v2")), QString(kModelTurbo));
    QCOMPARE(normalizeModelId(QStringLiteral("sapi")), QString(kModelSapi));
    QCOMPARE(normalizeModelId(QString()), QString(kModelSapi));
    QCOMPARE(normalizeModelId(QStringLiteral("browser_tts")), QString(kModelSapi));
    QCOMPARE(normalizeModelId(QStringLiteral("piper_tts")), QString(kModelSapi));
    QCOMPARE(normalizeModelId(QStringLiteral("nope")), QString(kModelSapi));
}

void ElevenRequestTest::isElevenAfterNormalize()
{
    QVERIFY(isElevenModel(QStringLiteral("eleven_v4_turbo")));
    QVERIFY(isElevenModel(QStringLiteral("eleven_v3")));
    QVERIFY(isElevenModel(QStringLiteral("eleven_flash_v2")));
    QVERIFY(!isElevenModel(QStringLiteral("sapi")));
    QVERIFY(!isElevenModel(QStringLiteral("browser_tts")));
}

void ElevenRequestTest::tagsDetectAndStrip()
{
    QVERIFY(!phraseHasInlineTags(QStringLiteral("Hello there")));
    QVERIFY(phraseHasInlineTags(QStringLiteral("Hello [laugh] there")));
    QVERIFY(phraseHasInlineTags(QStringLiteral("[]")));
    QCOMPARE(stripInlineTags(QStringLiteral("Hello [laugh] there")),
             QStringLiteral("Hello there"));
    QCOMPARE(stripInlineTags(QStringLiteral("  Hello   [x]  world  ")),
             QStringLiteral("Hello world"));
    QVERIFY(hasNonTagSpeechContent(QStringLiteral("Hello [laugh]")));
    QVERIFY(!hasNonTagSpeechContent(QStringLiteral("[laugh]")));
    QVERIFY(!hasNonTagSpeechContent(QString()));
}

void ElevenRequestTest::tagsStayOnTurbo()
{
    QVERIFY(!hasNonTagSpeechContent(QStringLiteral("[laugh][cry]")));
    const Prepared tags = prepareSpeakRequest(QStringLiteral("Hello [laugh] there"),
                                              QStringLiteral("eleven_flash_v2_5"), 1.4);
    QCOMPARE(tags.modelId, QString(kModelTurbo));
    QCOMPARE(tags.text, QStringLiteral("Hello [laugh] there"));
    QCOMPARE(tags.localSpeed, 1.4);
}

void ElevenRequestTest::localSpeedClamp_data()
{
    QTest::addColumn<double>("desired");
    QTest::addColumn<QString>("model");
    QTest::addColumn<double>("local");

    const char* models[] = {"eleven_v4_turbo", "eleven_v3", "eleven_flash_v2_5", "sapi"};
    for (const char* model : models) {
        QTest::newRow(model) << 1.4 << QString::fromLatin1(model) << 1.4;
    }
    QTest::newRow("below_min") << 0.1 << QString(kModelTurbo) << 0.25;
    QTest::newRow("above_max") << 8.0 << QString(kModelTurbo) << 4.0;
    QTest::newRow("legacy_flash") << 1.4 << QStringLiteral("eleven_flash_v2") << 1.4;
}

void ElevenRequestTest::localSpeedClamp()
{
    QFETCH(double, desired);
    QFETCH(QString, model);
    QFETCH(double, local);

    QCOMPARE(prepareSpeakRequest(QStringLiteral("Hi"), model, desired).localSpeed, local);
}

void ElevenRequestTest::sapiStripsTags()
{
    const Prepared p =
        prepareSpeakRequest(QStringLiteral("Hello [laugh] there"), kModelSapi, 1.4);
    QCOMPARE(p.modelId, QString(kModelSapi));
    QCOMPARE(p.text, QStringLiteral("Hello there"));
    QCOMPARE(p.localSpeed, 1.4);

    const Prepared unknown =
        prepareSpeakRequest(QStringLiteral("Hello"), QStringLiteral("nope"), 1.4);
    QCOMPARE(unknown.modelId, QString(kModelSapi));
    QVERIFY(!isElevenModel(unknown.modelId));
}

void ElevenRequestTest::nonFiniteSpeedDefaults()
{
    const Prepared p = prepareSpeakRequest(QStringLiteral("Hi"), kModelTurbo,
                                           std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(p.localSpeed, 1.0);
}

void ElevenRequestTest::dialogueMessages()
{
    const QString url = dialogueStreamUrl();
    QVERIFY(url.startsWith(
        QStringLiteral("wss://api.elevenlabs.io/v1/text-to-dialogue/stream-input?")));
    QVERIFY(url.contains(QStringLiteral("model_id=eleven_v4_turbo")));
    QVERIFY(url.contains(QStringLiteral("output_format=pcm_24000")));
    QCOMPARE(QString(kVoicesUrl), QStringLiteral("https://api.elevenlabs.io/v1/voices"));
    QCOMPARE(kSpeakTimeoutMs, 25000);
    QCOMPARE(kVoicesTimeoutMs, 15000);

    const QJsonObject reg =
        QJsonDocument::fromJson(registerVoicesMessage(QStringLiteral("abc\"x"))).object();
    QCOMPARE(reg.value(QStringLiteral("voices")).toArray().at(0).toString(),
             QStringLiteral("abc\"x"));
    QVERIFY(!reg.contains(QStringLiteral("xi_api_key")));

    const QJsonObject line =
        QJsonDocument::fromJson(speakInputsMessage(QStringLiteral("Hi [laugh]"), QStringLiteral("v1")))
            .object()
            .value(QStringLiteral("inputs"))
            .toArray()
            .at(0)
            .toObject();
    QCOMPARE(line.value(QStringLiteral("text")).toString(), QStringLiteral("Hi [laugh]"));
    QCOMPARE(line.value(QStringLiteral("voice_id")).toString(), QStringLiteral("v1"));
    QVERIFY(line.value(QStringLiteral("new_turn")).toBool());
    QVERIFY(QJsonDocument::fromJson(closeSocketMessage()).object().value(QStringLiteral("close_socket")).toBool());
    QVERIFY(QJsonDocument::fromJson(keepAliveMessage()).object().value(QStringLiteral("keep_alive")).toBool());
}

void ElevenRequestTest::voicesCacheRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("voices-cache.json"));
    const QByteArray body =
        QByteArrayLiteral("{\"voices\":[{\"voice_id\":\"abc\",\"name\":\"Ada\"}]}");
    const QDateTime now =
        QDateTime::fromString(QStringLiteral("2026-09-03T12:00:00Z"), Qt::ISODate);
    QVERIFY(ElevenClient::writeVoicesCacheAt(path, body, now));
    QVERIFY(!ElevenClient::writeVoicesCacheAt(path, QByteArrayLiteral("{\"ok\":true}"), now));
    const QJsonObject cache = ElevenClient::readVoicesCacheAt(path);
    QCOMPARE(cache.value(QStringLiteral("fetchedAt")).toString(), now.toUTC().toString(Qt::ISODate));
    const QJsonArray voices = cache.value(QStringLiteral("voices")).toArray();
    QCOMPARE(voices.size(), 1);
    QCOMPARE(voices.at(0).toObject().value(QStringLiteral("voice_id")).toString(),
             QStringLiteral("abc"));
}

void ElevenRequestTest::voicesCacheTtl()
{
    using gazer::ElevenClient;
    QJsonObject cache;
    const QDateTime now =
        QDateTime::fromString(QStringLiteral("2026-09-03T12:00:00Z"), Qt::ISODate);
    cache.insert(QStringLiteral("fetchedAt"), now.toUTC().toString(Qt::ISODate));
    cache.insert(QStringLiteral("voices"), QJsonArray());
    QVERIFY(ElevenClient::voicesCacheFresh(cache, now.addSecs(3600)));
    QVERIFY(!ElevenClient::voicesCacheFresh(cache, now.addSecs(25 * 3600)));
    QVERIFY(!ElevenClient::voicesCacheFresh(QJsonObject{}, now));
}

QObject* createElevenRequestTest()
{
    return new ElevenRequestTest;
}

#include "ElevenRequestTest.moc"
