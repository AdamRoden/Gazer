#include "assist/AudioOutput.h"
#include "assist/SystemVolume.h"

#include <QtTest>

using namespace gazer;

class SystemVolumeTest final : public QObject {
    Q_OBJECT

private slots:
    void followsSpeechOutputKey();
};

void SystemVolumeTest::followsSpeechOutputKey()
{
#ifndef Q_OS_WIN
    QSKIP("WASAPI volume is Windows-only");
#else
    SystemVolume vol;
    if (!vol.setDeviceKey(QString()) || vol.boundEndpointId().isEmpty()) {
        QSKIP("No default playback device");
    }
    const QString def = vol.boundEndpointId();
    QVERIFY(vol.percent() >= SystemVolume::kMin);
    QVERIFY(vol.percent() <= SystemVolume::kMax);

    QVERIFY(vol.setDeviceKey(QStringLiteral("not-a-key")));
    QCOMPARE(vol.boundEndpointId(), def);
    const QString missing = AudioOutput::deviceKey(QByteArray("missing-endpoint"));
    QVERIFY(vol.setDeviceKey(missing));
    QCOMPARE(vol.boundEndpointId(), def);

    const QVector<AudioOutput::PlaybackDevice> devices = AudioOutput::playbackDevices();
    for (const AudioOutput::PlaybackDevice& dev : devices) {
        QVERIFY2(vol.setDeviceKey(dev.key), qPrintable(dev.name));
        QCOMPARE(vol.boundEndpointId(), QString::fromUtf8(AudioOutput::idFromKey(dev.key)));
        QVERIFY(vol.percent() >= SystemVolume::kMin);
        QVERIFY(vol.percent() <= SystemVolume::kMax);
    }

    QVERIFY(vol.setDeviceKey(QString()));
    QCOMPARE(vol.boundEndpointId(), def);
#endif
}

QObject* createSystemVolumeTest()
{
    return new SystemVolumeTest;
}

#include "SystemVolumeTest.moc"
