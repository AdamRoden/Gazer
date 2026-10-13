#include "assist/AudioOutput.h"

#include <QtTest>

using namespace gazer;

class AudioOutputTest final : public QObject {
    Q_OBJECT

private slots:
    void deviceKeyRoundTrip();
    void deviceKeyRejectsGarbage();
};

void AudioOutputTest::deviceKeyRoundTrip()
{
    const QByteArray id("ep.{1234}/a+b");
    const QString key = AudioOutput::deviceKey(id);
    QVERIFY(!key.isEmpty());
    QCOMPARE(AudioOutput::idFromKey(key), id);
    QVERIFY(AudioOutput::deviceKey(QByteArray()).isEmpty());
    QVERIFY(AudioOutput::idFromKey(QString()).isEmpty());
}

void AudioOutputTest::deviceKeyRejectsGarbage()
{
    QVERIFY(AudioOutput::idFromKey(QStringLiteral("@@@")).isEmpty());
    QVERIFY(AudioOutput::idFromKey(QStringLiteral("not-a-key")).isEmpty());
}

QObject* createAudioOutputTest()
{
    return new AudioOutputTest;
}

#include "AudioOutputTest.moc"
