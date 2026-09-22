#include "input/InjectGate.h"

#include <QtTest>

using namespace gazer;

class InjectGateTest final : public QObject {
    Q_OBJECT

private slots:
    void cleanup();
    void pauseRoundtrip();
};

void InjectGateTest::cleanup()
{
    InjectGate::setPaused(false);
}

void InjectGateTest::pauseRoundtrip()
{
    InjectGate::setPaused(false);
    QVERIFY(!InjectGate::paused());
    InjectGate::setPaused(true);
    QVERIFY(InjectGate::paused());
    InjectGate::setPaused(false);
    QVERIFY(!InjectGate::paused());
}

QObject* createInjectGateTest()
{
    return new InjectGateTest;
}

#include "InjectGateTest.moc"
