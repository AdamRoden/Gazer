#include "assist/ElevenClient.h"
#include "assist/ElevenRequest.h"

#include <QDateTime>
#include <QJsonArray>
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
    void emptyBodyAfterStrip();
    void splitSpeed_data();
    void splitSpeed();
    void splitSpeedAliasUsesFlash();
    void tagsForceV3KeepText();
    void flashStripsTags();
    void sapiUnknownHasNoElevenModel();
    void prepareJsonKeysFlash();
    void prepareJsonKeysV3();
    void nonFiniteSpeedAndPitchDefault();
    void speakUrlEncodesVoiceId();
    void voicesCacheRoundTrip();
    void voicesCacheTtl();
};

void ElevenRequestTest::normalizeAliasesAndUnknown()
{
    QCOMPARE(normalizeModelId(QStringLiteral("eleven_v3")), QString(kModelV3));
    QCOMPARE(normalizeModelId(QStringLiteral("eleven_flash_v2_5")), QString(kModelFlash));
    QCOMPARE(normalizeModelId(QStringLiteral("eleven_flash_v2")), QString(kModelFlash));
    QCOMPARE(normalizeModelId(QStringLiteral("eleven_multilingual_v2")), QString(kModelFlash));
    QCOMPARE(normalizeModelId(QStringLiteral("sapi")), QString(kModelSapi));
    QCOMPARE(normalizeModelId(QString()), QString(kModelSapi));
    QCOMPARE(normalizeModelId(QStringLiteral("browser_tts")), QString(kModelSapi));
    QCOMPARE(normalizeModelId(QStringLiteral("piper_tts")), QString(kModelSapi));
    QCOMPARE(normalizeModelId(QStringLiteral("nope")), QString(kModelSapi));
}

void ElevenRequestTest::isElevenAfterNormalize()
{
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

void ElevenRequestTest::emptyBodyAfterStrip()
{
    QVERIFY(!hasNonTagSpeechContent(QStringLiteral("[laugh][cry]")));
    const Prepared p = prepareSpeakRequest(QStringLiteral("[laugh][cry]"), kModelFlash, 1.0, 1.0);
    QCOMPARE(p.modelId, QString(kModelV3)); // tags force v3
    QCOMPARE(p.text, QStringLiteral("[laugh][cry]"));
    QCOMPARE(p.body.value(QStringLiteral("text")).toString(), QStringLiteral("[laugh][cry]"));
}

void ElevenRequestTest::splitSpeed_data()
{
    QTest::addColumn<double>("desired");
    QTest::addColumn<QString>("model");
    QTest::addColumn<bool>("hasApi");
    QTest::addColumn<double>("api");
    QTest::addColumn<double>("local");

    auto flashRow = [](const char* name, double d, double api) {
        QTest::newRow(name) << d << QString(kModelFlash) << true << api << (d / api);
    };
    auto v3Row = [](const char* name, double d) {
        QTest::newRow(name) << d << QString(kModelV3) << false << 0.0 << d;
    };

    flashRow("flash_0.25", 0.25, 0.7);
    flashRow("flash_0.5", 0.5, 0.7);
    flashRow("flash_0.7", 0.7, 0.7);
    flashRow("flash_1.0", 1.0, 1.0);
    flashRow("flash_1.2", 1.2, 1.2);
    flashRow("flash_1.4", 1.4, 1.2);
    flashRow("flash_2.0", 2.0, 1.2);
    flashRow("flash_4.0", 4.0, 1.2);
    v3Row("v3_0.25", 0.25);
    v3Row("v3_0.5", 0.5);
    v3Row("v3_0.7", 0.7);
    v3Row("v3_1.0", 1.0);
    v3Row("v3_1.2", 1.2);
    v3Row("v3_1.4", 1.4);
    v3Row("v3_2.0", 2.0);
    v3Row("v3_4.0", 4.0);

    QTest::newRow("flash_below_min") << 0.1 << QString(kModelFlash) << true << 0.7
                                     << (0.25 / 0.7);
    QTest::newRow("flash_above_max") << 8.0 << QString(kModelFlash) << true << 1.2
                                     << (4.0 / 1.2);
    QTest::newRow("v3_below_min") << 0.1 << QString(kModelV3) << false << 0.0 << 0.25;
    QTest::newRow("v3_above_max") << 8.0 << QString(kModelV3) << false << 0.0 << 4.0;
}

void ElevenRequestTest::splitSpeed()
{
    QFETCH(double, desired);
    QFETCH(QString, model);
    QFETCH(bool, hasApi);
    QFETCH(double, api);
    QFETCH(double, local);

    const SpeedSplit s = ElevenRequest::splitSpeed(desired, model);
    QCOMPARE(s.apiSpeed.has_value(), hasApi);
    if (hasApi) {
        QCOMPARE(*s.apiSpeed, api);
    }
    QCOMPARE(s.localSpeed, local);
}

void ElevenRequestTest::splitSpeedAliasUsesFlash()
{
    const SpeedSplit s = ElevenRequest::splitSpeed(1.4, QStringLiteral("eleven_flash_v2"));
    QVERIFY(s.apiSpeed.has_value());
    QCOMPARE(*s.apiSpeed, 1.2);
    QCOMPARE(s.localSpeed, 1.4 / 1.2);
}

void ElevenRequestTest::tagsForceV3KeepText()
{
    const Prepared p = prepareSpeakRequest(QStringLiteral("Hello [laugh] there"), kModelFlash,
                                           1.4, 1.0);
    QCOMPARE(p.modelId, QString(kModelV3));
    QCOMPARE(p.text, QStringLiteral("Hello [laugh] there"));
    QCOMPARE(p.body.value(QStringLiteral("model_id")).toString(), QString(kModelV3));
    QCOMPARE(p.body.value(QStringLiteral("text")).toString(), p.text);
    QVERIFY(!p.body.contains(QStringLiteral("voice_settings")));
    QVERIFY(!p.body.contains(QStringLiteral("fx")));
    QCOMPARE(p.localSpeed, 1.4);

    const Prepared fromSapi = prepareSpeakRequest(QStringLiteral("Hi [cry]"), kModelSapi, 1.0, 1.0);
    QCOMPARE(fromSapi.modelId, QString(kModelV3));
}

void ElevenRequestTest::flashStripsTags()
{
    // Tags always force v3, so Flash never sees [tags] in prepareSpeakRequest.
    // The Flash strip path is untagged text (identity) plus stripInlineTags itself.
    QCOMPARE(stripInlineTags(QStringLiteral("Hello [laugh] there")), QStringLiteral("Hello there"));
    const Prepared clean =
        prepareSpeakRequest(QStringLiteral("Hello there"), kModelFlash, 1.0, 1.0);
    QCOMPARE(clean.modelId, QString(kModelFlash));
    QCOMPARE(clean.text, QStringLiteral("Hello there"));
    QVERIFY(!phraseHasInlineTags(clean.text));
}

void ElevenRequestTest::sapiUnknownHasNoElevenModel()
{
    const Prepared p = prepareSpeakRequest(QStringLiteral("Hello"), QStringLiteral("nope"), 1.4,
                                           1.0);
    QCOMPARE(p.modelId, QString(kModelSapi));
    QVERIFY(!isElevenModel(p.modelId));
    QCOMPARE(p.body.value(QStringLiteral("model_id")).toString(), QString(kModelSapi));
    QVERIFY(p.body.contains(QStringLiteral("voice_settings")));
    QCOMPARE(p.body.value(QStringLiteral("voice_settings")).toObject().value(QStringLiteral("speed")).toDouble(),
             1.2);
    QCOMPARE(p.localSpeed, 1.4 / 1.2);
}

void ElevenRequestTest::prepareJsonKeysFlash()
{
    const Prepared p = prepareSpeakRequest(QStringLiteral("Hello there"), kModelFlash, 1.4, 0.8);
    QCOMPARE(p.body.keys().size(), 3);
    QVERIFY(p.body.contains(QStringLiteral("text")));
    QVERIFY(p.body.contains(QStringLiteral("model_id")));
    QVERIFY(p.body.contains(QStringLiteral("voice_settings")));
    QCOMPARE(p.body.value(QStringLiteral("model_id")).toString(), QString(kModelFlash));
    QCOMPARE(p.body.value(QStringLiteral("text")).toString(), QStringLiteral("Hello there"));
    const QJsonObject vs = p.body.value(QStringLiteral("voice_settings")).toObject();
    QCOMPARE(vs.value(QStringLiteral("speed")).toDouble(), 1.2);
    QCOMPARE(p.localSpeed, 1.4 / 1.2);
    QCOMPARE(p.pitch, 0.8);
}

void ElevenRequestTest::prepareJsonKeysV3()
{
    const Prepared p = prepareSpeakRequest(QStringLiteral("Hello [laugh] there"), kModelV3, 1.4,
                                           1.5);
    QCOMPARE(p.body.keys().size(), 2);
    QVERIFY(p.body.contains(QStringLiteral("text")));
    QVERIFY(p.body.contains(QStringLiteral("model_id")));
    QVERIFY(!p.body.contains(QStringLiteral("voice_settings")));
    QCOMPARE(p.pitch, 1.5);
}

void ElevenRequestTest::nonFiniteSpeedAndPitchDefault()
{
    const Prepared p = prepareSpeakRequest(QStringLiteral("Hi"), kModelFlash,
                                           std::numeric_limits<double>::quiet_NaN(),
                                           std::numeric_limits<double>::infinity());
    QCOMPARE(p.localSpeed, 1.0);
    QCOMPARE(p.pitch, 1.0);
    QCOMPARE(p.body.value(QStringLiteral("voice_settings"))
                 .toObject()
                 .value(QStringLiteral("speed"))
                 .toDouble(),
             1.0);
}

void ElevenRequestTest::speakUrlEncodesVoiceId()
{
    QCOMPARE(speakUrl(QStringLiteral("abc123")),
             QStringLiteral("https://api.elevenlabs.io/v1/text-to-speech/abc123"));
    QCOMPARE(QString(kVoicesUrl), QStringLiteral("https://api.elevenlabs.io/v1/voices"));
    QCOMPARE(kSpeakTimeoutMs, 25000);
    QCOMPARE(kVoicesTimeoutMs, 15000);
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
