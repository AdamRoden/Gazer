#include "assist/AudioGain.h"

#include <QtTest>
#include <limits>

using namespace gazer;

class AudioGainTest final : public QObject {
    Q_OBJECT

private slots:
    void clampNonFiniteAndRange();
    void needsDecodeAboveOne();
    void scaleInt16NoOpAtUnity();
    void scaleInt16Doubles();
    void scaleInt16Clamps();
    void scaleInt16IgnoresOddTrailingByte();
};

void AudioGainTest::clampNonFiniteAndRange()
{
    QCOMPARE(AudioGain::clamp(1.0), 1.0);
    QCOMPARE(AudioGain::clamp(2.5), 2.5);
    QCOMPARE(AudioGain::clamp(0.2), 1.0);
    QCOMPARE(AudioGain::clamp(9.0), 5.0);
    QCOMPARE(AudioGain::clamp(std::numeric_limits<double>::quiet_NaN()), 1.0);
}

void AudioGainTest::needsDecodeAboveOne()
{
    QVERIFY(!AudioGain::needsDecode(1.0));
    QVERIFY(!AudioGain::needsDecode(0.5));
    QVERIFY(AudioGain::needsDecode(1.5));
}

void AudioGainTest::scaleInt16NoOpAtUnity()
{
    qint16 samples[] = {1000, -2000, 32767};
    AudioGain::scaleInt16(reinterpret_cast<char*>(samples), int(sizeof(samples)), 1.0);
    QCOMPARE(samples[0], qint16(1000));
    QCOMPARE(samples[1], qint16(-2000));
    QCOMPARE(samples[2], qint16(32767));
}

void AudioGainTest::scaleInt16Doubles()
{
    qint16 samples[] = {1000, -2000, 0};
    AudioGain::scaleInt16(reinterpret_cast<char*>(samples), int(sizeof(samples)), 2.0);
    QCOMPARE(samples[0], qint16(2000));
    QCOMPARE(samples[1], qint16(-4000));
    QCOMPARE(samples[2], qint16(0));
}

void AudioGainTest::scaleInt16Clamps()
{
    qint16 samples[] = {20000, -20000};
    AudioGain::scaleInt16(reinterpret_cast<char*>(samples), int(sizeof(samples)), 5.0);
    QCOMPARE(samples[0], qint16(32767));
    QCOMPARE(samples[1], qint16(-32768));
}

void AudioGainTest::scaleInt16IgnoresOddTrailingByte()
{
    char bytes[5] = {char(0x10), char(0x00), char(0x20), char(0x00), char(0x7f)};
    AudioGain::scaleInt16(bytes, 5, 2.0);
    const auto* samples = reinterpret_cast<qint16*>(bytes);
    QCOMPARE(samples[0], qint16(0x20));
    QCOMPARE(samples[1], qint16(0x40));
    QCOMPARE(bytes[4], char(0x7f));
}

QObject* createAudioGainTest()
{
    return new AudioGainTest;
}

#include "AudioGainTest.moc"
