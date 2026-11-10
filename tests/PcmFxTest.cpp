#include "assist/PcmFx.h"

#include <QtTest>
#include <cmath>

using gazer::PcmFx::apply;
using gazer::PcmFx::isIdentity;

class PcmFxTest final : public QObject {
    Q_OBJECT

private slots:
    void identityLeavesPcm();
    void speedHalvesLength();
    void pitchRestoresLength();
};

QByteArray tone(int samples)
{
    QByteArray pcm(samples * 2, '\0');
    auto* dst = reinterpret_cast<qint16*>(pcm.data());
    for (int i = 0; i < samples; ++i) {
        dst[i] = qint16(std::sin(i * 0.07) * 8000.0);
    }
    return pcm;
}

void PcmFxTest::identityLeavesPcm()
{
    QVERIFY(isIdentity(1.0, 1.0));
    QVERIFY(!isIdentity(1.2, 1.0));
    QVERIFY(!isIdentity(1.0, 1.5));
    const QByteArray pcm = tone(4000);
    QCOMPARE(apply(pcm, 24000, 1.0, 1.0), pcm);
    QByteArray odd = pcm;
    odd.append('\0');
    QCOMPARE(apply(odd, 24000, 1.0, 1.0), pcm);
    QCOMPARE(apply(QByteArray(1, '\0'), 24000, 1.0, 1.0), QByteArray());
}

void PcmFxTest::speedHalvesLength()
{
    const QByteArray pcm = tone(4000);
    const QByteArray out = apply(pcm, 24000, 2.0, 1.0);
    QCOMPARE(out.size(), 4000);
}

void PcmFxTest::pitchRestoresLength()
{
    const QByteArray pcm = tone(4000);
    const QByteArray out = apply(pcm, 24000, 1.0, 2.0);
    QCOMPARE(out.size(), pcm.size());
}

QObject* createPcmFxTest()
{
    return new PcmFxTest;
}

#include "PcmFxTest.moc"
