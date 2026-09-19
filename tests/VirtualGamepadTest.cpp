#include "input/VirtualGamepad.h"

#include <QtTest>

using namespace gazer;

class VirtualGamepadTest final : public QObject {
    Q_OBJECT

private slots:
    void lookupButtons();
    void dryRunPressRelease();
    void dryRunAxes();
    void unknownAxisFails();
    void missingDllFails();
};

void VirtualGamepadTest::lookupButtons()
{
    std::uint16_t bit = 0;
    QVERIFY(VirtualGamepad::lookupButton(QStringLiteral("A"), &bit));
    QCOMPARE(bit, std::uint16_t(0x1000));
    QVERIFY(VirtualGamepad::lookupButton(QStringLiteral("left-shoulder"), &bit));
    QCOMPARE(bit, std::uint16_t(0x0100));
    QVERIFY(VirtualGamepad::lookupButton(QStringLiteral("dpadUp"), &bit));
    QCOMPARE(bit, std::uint16_t(0x0001));
    QString err;
    QVERIFY(!VirtualGamepad::lookupButton(QStringLiteral("nope"), &bit, &err));
    QVERIFY(err.contains(QStringLiteral("Unknown")));
}

void VirtualGamepadTest::dryRunPressRelease()
{
    VirtualGamepad pad;
    pad.setDryRun(true);
    QString err;
    QVERIFY2(pad.pressButton(QStringLiteral("a"), &err), qPrintable(err));
    QVERIFY(pad.isConnected());
    QCOMPARE(pad.backendName(), QStringLiteral("dry"));
    QCOMPARE(pad.report().buttons, std::uint16_t(0x1000));
    QVERIFY2(pad.pressButton(QStringLiteral("b"), &err), qPrintable(err));
    QCOMPARE(pad.report().buttons, std::uint16_t(0x1000 | 0x2000));
    QVERIFY2(pad.releaseButton(QStringLiteral("a"), &err), qPrintable(err));
    QCOMPARE(pad.report().buttons, std::uint16_t(0x2000));
}

void VirtualGamepadTest::dryRunAxes()
{
    VirtualGamepad pad;
    pad.setDryRun(true);
    QString err;
    QVERIFY2(pad.setAxis(QStringLiteral("lx"), 1.0, &err), qPrintable(err));
    QCOMPARE(pad.report().leftX, std::int16_t(32767));
    QVERIFY2(pad.setAxis(QStringLiteral("ly"), -1.0, &err), qPrintable(err));
    QCOMPARE(pad.report().leftY, std::int16_t(-32768));
    QVERIFY2(pad.setAxis(QStringLiteral("lt"), 1.0, &err), qPrintable(err));
    QCOMPARE(pad.report().leftTrigger, std::uint8_t(255));
    QVERIFY2(pad.setAxis(QStringLiteral("rt"), 0.5, &err), qPrintable(err));
    QCOMPARE(pad.report().rightTrigger, std::uint8_t(128));
}

void VirtualGamepadTest::unknownAxisFails()
{
    VirtualGamepad pad;
    pad.setDryRun(true);
    QString err;
    QVERIFY(!pad.setAxis(QStringLiteral("z"), 1.0, &err));
    QVERIFY(err.contains(QStringLiteral("axis")));
}

void VirtualGamepadTest::missingDllFails()
{
    VirtualGamepad pad;
    pad.setOverrideDll(QString());
    QString err;
    QVERIFY(!pad.ensureConnected(&err));
    QVERIFY(err.contains(QStringLiteral("ViGEmClient.dll not found")));
    QVERIFY(!pad.isConnected());
}

QObject* createVirtualGamepadTest()
{
    return new VirtualGamepadTest;
}

#include "VirtualGamepadTest.moc"
