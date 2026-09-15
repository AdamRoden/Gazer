#include "core/GazePoint.h"
#include "layout/DwellPhase.h"
#include "layout/DwellStateMachine.h"
#include "layout/InvalidGazeGrace.h"

#include <QSignalSpy>
#include <QtTest>

using namespace gazer;

class DwellStateMachineTest final : public QObject {
    Q_OBJECT

private slots:
    void leadingZeroFiresAfterScanGrace();
    void positiveStepStillRequiresHold();
    void invalidGraceDoesNotAdvanceFirstStep();
    void emptyHitDoesNotAdvanceFirstStep();
    void invalidGraceDoesNotShortenScanGrace();
    void invalidGraceFreezesProgress();
    void secondInvalidHoldDoesNotAdvance();
    void invalidGraceExpiryRestartsDwell();
    void zeroInvalidGraceExpiresImmediately();
    void dwellPhaseArmAdvanceCommitWrap();
    void rescanAfterStepHoldsProgressUntilNextStep();
    void rescanAfterStepLookAwayDoesNotFillNextStep();
    void startHoldBlocksUntilElapsed();
    void startHoldLatchesFirstSample();
    void startHoldZeroNeverBlocks();
    void startHoldResetClearsArm();
};

namespace {

GazePoint sample(qint64 t, bool valid = true)
{
    GazePoint p;
    p.x = 10;
    p.y = 10;
    p.timestampMs = t;
    p.valid = valid;
    return p;
}

} // namespace

void DwellStateMachineTest::leadingZeroFiresAfterScanGrace()
{
    DwellStateMachine sm;
    sm.setScanGraceMs(100);
    sm.setDwellSequence({0, 640});
    QSignalSpy fired(&sm, &DwellStateMachine::itemActivated);

    sm.onGazeSample(sample(0), QStringLiteral("a"));
    QCOMPARE(fired.size(), 0);

    sm.onGazeSample(sample(99), QStringLiteral("a"));
    QCOMPARE(fired.size(), 0);

    sm.onGazeSample(sample(100), QStringLiteral("a"));
    QCOMPARE(fired.size(), 1);

    sm.onGazeSample(sample(739), QStringLiteral("a"));
    QCOMPARE(fired.size(), 1);

    sm.onGazeSample(sample(740), QStringLiteral("a"));
    QCOMPARE(fired.size(), 2);
}

void DwellStateMachineTest::positiveStepStillRequiresHold()
{
    DwellStateMachine sm;
    sm.setScanGraceMs(0);
    sm.setDwellSequence({800});
    QSignalSpy fired(&sm, &DwellStateMachine::itemActivated);

    sm.onGazeSample(sample(0), QStringLiteral("a"));
    QCOMPARE(fired.size(), 0);
    sm.onGazeSample(sample(799), QStringLiteral("a"));
    QCOMPARE(fired.size(), 0);
    sm.onGazeSample(sample(800), QStringLiteral("a"));
    QCOMPARE(fired.size(), 1);
}

void DwellStateMachineTest::invalidGraceDoesNotAdvanceFirstStep()
{
    DwellStateMachine sm;
    sm.setScanGraceMs(0);
    sm.setInvalidGraceMs(180);
    sm.setDwellSequence({800});
    QSignalSpy fired(&sm, &DwellStateMachine::itemActivated);

    sm.onGazeSample(sample(0), QStringLiteral("a"));
    sm.onGazeSample(sample(400), QStringLiteral("a"));
    sm.onGazeSample(sample(400, false), QString());
    sm.onGazeSample(sample(500, false), QString());
    sm.onGazeSample(sample(500), QStringLiteral("a"));
    sm.onGazeSample(sample(799), QStringLiteral("a"));
    QCOMPARE(fired.size(), 0);

    sm.onGazeSample(sample(900), QStringLiteral("a"));
    QCOMPARE(fired.size(), 1);
}

void DwellStateMachineTest::emptyHitDoesNotAdvanceFirstStep()
{
    DwellStateMachine sm;
    sm.setScanGraceMs(0);
    sm.setInvalidGraceMs(180);
    sm.setDwellSequence({800});
    QSignalSpy fired(&sm, &DwellStateMachine::itemActivated);

    sm.onGazeSample(sample(0), QStringLiteral("a"));
    sm.onGazeSample(sample(400), QStringLiteral("a"));
    QCOMPARE(sm.progress(), 0.5);

    sm.onGazeSample(sample(400), QString());
    sm.onGazeSample(sample(520), QString());
    QCOMPARE(sm.progress(), 0.5);
    QCOMPARE(sm.hoveredItemId(), QStringLiteral("a"));

    sm.onGazeSample(sample(520), QStringLiteral("a"));
    QCOMPARE(sm.progress(), 0.5);
    sm.onGazeSample(sample(799), QStringLiteral("a"));
    QCOMPARE(fired.size(), 0);

    sm.onGazeSample(sample(920), QStringLiteral("a"));
    QCOMPARE(fired.size(), 1);
}

void DwellStateMachineTest::invalidGraceDoesNotShortenScanGrace()
{
    DwellStateMachine sm;
    sm.setScanGraceMs(200);
    sm.setInvalidGraceMs(180);
    sm.setDwellSequence({10});
    QSignalSpy fired(&sm, &DwellStateMachine::itemActivated);

    sm.onGazeSample(sample(0), QStringLiteral("a"));
    sm.onGazeSample(sample(50), QStringLiteral("a"));
    sm.onGazeSample(sample(50), QString());
    sm.onGazeSample(sample(150), QString());
    sm.onGazeSample(sample(150), QStringLiteral("a"));
    QCOMPARE(fired.size(), 0);
    QCOMPARE(sm.isScanGraceComplete(), false);

    sm.onGazeSample(sample(299), QStringLiteral("a"));
    QCOMPARE(fired.size(), 0);
    QCOMPARE(sm.isScanGraceComplete(), false);

    sm.onGazeSample(sample(300), QStringLiteral("a"));
    QCOMPARE(sm.isScanGraceComplete(), true);
    QCOMPARE(fired.size(), 0);

    sm.onGazeSample(sample(310), QStringLiteral("a"));
    QCOMPARE(fired.size(), 1);
}

void DwellStateMachineTest::invalidGraceFreezesProgress()
{
    DwellStateMachine sm;
    sm.setScanGraceMs(0);
    sm.setInvalidGraceMs(180);
    sm.setDwellSequence({800});

    sm.onGazeSample(sample(0), QStringLiteral("a"));
    sm.onGazeSample(sample(400), QStringLiteral("a"));
    QCOMPARE(sm.progress(), 0.5);

    sm.onGazeSample(sample(400, false), QString());
    sm.onGazeSample(sample(500, false), QString());
    QCOMPARE(sm.progress(), 0.5);
    QCOMPARE(sm.hoveredItemId(), QStringLiteral("a"));

    sm.onGazeSample(sample(500), QStringLiteral("a"));
    QCOMPARE(sm.progress(), 0.5);
}

void DwellStateMachineTest::secondInvalidHoldDoesNotAdvance()
{
    DwellStateMachine sm;
    sm.setScanGraceMs(0);
    sm.setInvalidGraceMs(180);
    sm.setDwellSequence({800});
    QSignalSpy fired(&sm, &DwellStateMachine::itemActivated);

    sm.onGazeSample(sample(0), QStringLiteral("a"));
    sm.onGazeSample(sample(300), QStringLiteral("a"));
    sm.onGazeSample(sample(300, false), QString());
    sm.onGazeSample(sample(400, false), QString());
    sm.onGazeSample(sample(400), QStringLiteral("a"));
    sm.onGazeSample(sample(500), QStringLiteral("a"));
    sm.onGazeSample(sample(500, false), QString());
    sm.onGazeSample(sample(600, false), QString());
    sm.onGazeSample(sample(600), QStringLiteral("a"));
    sm.onGazeSample(sample(999), QStringLiteral("a"));
    QCOMPARE(fired.size(), 0);

    sm.onGazeSample(sample(1000), QStringLiteral("a"));
    QCOMPARE(fired.size(), 1);
}

void DwellStateMachineTest::invalidGraceExpiryRestartsDwell()
{
    DwellStateMachine sm;
    sm.setScanGraceMs(0);
    sm.setInvalidGraceMs(180);
    sm.setDwellSequence({800});
    QSignalSpy fired(&sm, &DwellStateMachine::itemActivated);
    QSignalSpy hover(&sm, &DwellStateMachine::hoverChanged);

    sm.onGazeSample(sample(0), QStringLiteral("a"));
    sm.onGazeSample(sample(200), QStringLiteral("a"));
    sm.onGazeSample(sample(200, false), QString());
    sm.onGazeSample(sample(379, false), QString());
    QCOMPARE(sm.hoveredItemId(), QStringLiteral("a"));
    QCOMPARE(fired.size(), 0);

    sm.onGazeSample(sample(380, false), QString());
    QCOMPARE(sm.hoveredItemId(), QString());
    QVERIFY(hover.size() >= 2);
    QCOMPARE(hover.last().at(0).toString(), QString());

    sm.onGazeSample(sample(380), QStringLiteral("a"));
    QCOMPARE(sm.hoveredItemId(), QStringLiteral("a"));
    QCOMPARE(sm.progress(), 0.0);
    sm.onGazeSample(sample(1179), QStringLiteral("a"));
    QCOMPARE(fired.size(), 0);

    sm.onGazeSample(sample(1180), QStringLiteral("a"));
    QCOMPARE(fired.size(), 1);
}

void DwellStateMachineTest::zeroInvalidGraceExpiresImmediately()
{
    DwellStateMachine sm;
    sm.setScanGraceMs(0);
    sm.setInvalidGraceMs(0);
    sm.setDwellSequence({800});

    sm.onGazeSample(sample(0), QStringLiteral("a"));
    sm.onGazeSample(sample(200), QStringLiteral("a"));
    QCOMPARE(sm.hoveredItemId(), QStringLiteral("a"));
    sm.onGazeSample(sample(200, false), QString());
    QCOMPARE(sm.hoveredItemId(), QString());
}

void DwellStateMachineTest::dwellPhaseArmAdvanceCommitWrap()
{
    DwellPhaseBank b;
    QVERIFY(!b.current(QStringLiteral("c")).has_value());
    b.onActivated(QStringLiteral("c"), 3);
    QCOMPARE(b.current(QStringLiteral("c")).value_or(-1), 0);
    b.onActivated(QStringLiteral("c"), 3);
    QCOMPARE(b.current(QStringLiteral("c")).value_or(-1), 1);
    b.onActivated(QStringLiteral("c"), 3);
    QCOMPARE(b.current(QStringLiteral("c")).value_or(-1), 2);
    b.onActivated(QStringLiteral("c"), 3);
    QCOMPARE(b.current(QStringLiteral("c")).value_or(-1), 0);
    QCOMPARE(b.takeCommit(QStringLiteral("c")).value_or(-1), 0);
    QVERIFY(!b.takeCommit(QStringLiteral("c")).has_value());
}

void DwellStateMachineTest::rescanAfterStepHoldsProgressUntilNextStep()
{
    DwellStateMachine sm;
    sm.setScanGraceMs(100);
    sm.setRescanAfterStep(true);
    sm.setDwellSequence({400, 400});
    QSignalSpy fired(&sm, &DwellStateMachine::itemActivated);

    sm.onGazeSample(sample(0), QStringLiteral("a"));
    sm.onGazeSample(sample(100), QStringLiteral("a"));
    sm.onGazeSample(sample(500), QStringLiteral("a"));
    QCOMPARE(fired.size(), 1);
    QCOMPARE(sm.progress(), 1.0);
    QCOMPARE(sm.isScanGraceComplete(), false);

    sm.onGazeSample(sample(599), QStringLiteral("a"));
    QCOMPARE(fired.size(), 1);
    QCOMPARE(sm.progress(), 1.0);

    sm.onGazeSample(sample(600), QStringLiteral("a"));
    QCOMPARE(fired.size(), 1);
    QCOMPARE(sm.progress(), 0.0);
    QCOMPARE(sm.isScanGraceComplete(), true);

    sm.onGazeSample(sample(999), QStringLiteral("a"));
    QCOMPARE(fired.size(), 1);
    sm.onGazeSample(sample(1000), QStringLiteral("a"));
    QCOMPARE(fired.size(), 2);
}

void DwellStateMachineTest::rescanAfterStepLookAwayDoesNotFillNextStep()
{
    DwellStateMachine sm;
    sm.setScanGraceMs(100);
    sm.setInvalidGraceMs(180);
    sm.setRescanAfterStep(true);
    sm.setDwellSequence({400, 400});
    QSignalSpy fired(&sm, &DwellStateMachine::itemActivated);
    QSignalSpy hover(&sm, &DwellStateMachine::hoverChanged);

    sm.onGazeSample(sample(0), QStringLiteral("a"));
    sm.onGazeSample(sample(100), QStringLiteral("a"));
    sm.onGazeSample(sample(500), QStringLiteral("a"));
    QCOMPARE(fired.size(), 1);
    QCOMPARE(sm.progress(), 1.0);

    sm.onGazeSample(sample(500, false), QString());
    sm.onGazeSample(sample(600, false), QString());
    QCOMPARE(sm.progress(), 1.0);
    QCOMPARE(sm.hoveredItemId(), QStringLiteral("a"));
    QCOMPARE(fired.size(), 1);

    sm.onGazeSample(sample(680, false), QString());
    QCOMPARE(sm.hoveredItemId(), QString());
    QCOMPARE(fired.size(), 1);
    QVERIFY(hover.size() >= 2);
    QCOMPARE(hover.last().at(0).toString(), QString());
}

void DwellStateMachineTest::startHoldBlocksUntilElapsed()
{
    StartHold hold;
    hold.arm(500);
    QVERIFY(hold.blocking(1000));
    QVERIFY(hold.blocking(1499));
    QVERIFY(!hold.blocking(1500));
    QVERIFY(!hold.blocking(1600));
}

void DwellStateMachineTest::startHoldLatchesFirstSample()
{
    StartHold hold;
    hold.arm(500);
    QVERIFY(hold.blocking(2000));
    QVERIFY(hold.blocking(2499));
    QVERIFY(!hold.blocking(2500));
}

void DwellStateMachineTest::startHoldZeroNeverBlocks()
{
    StartHold hold;
    hold.arm(0);
    QVERIFY(!hold.blocking(0));
    QVERIFY(!hold.blocking(500));
}

void DwellStateMachineTest::startHoldResetClearsArm()
{
    StartHold hold;
    hold.arm(500);
    QVERIFY(hold.blocking(0));
    hold.reset();
    QVERIFY(!hold.blocking(0));
    hold.arm(500);
    QVERIFY(hold.blocking(100));
}

QTEST_MAIN(DwellStateMachineTest)
#include "DwellStateMachineTest.moc"
