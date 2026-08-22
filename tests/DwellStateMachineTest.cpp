#include "core/GazePoint.h"
#include "layout/DwellStateMachine.h"

#include <QSignalSpy>
#include <QtTest>

using namespace gazer;

class DwellStateMachineTest final : public QObject {
    Q_OBJECT

private slots:
    void leadingZeroFiresAfterScanGrace();
    void positiveStepStillRequiresHold();
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

QTEST_MAIN(DwellStateMachineTest)
#include "DwellStateMachineTest.moc"
