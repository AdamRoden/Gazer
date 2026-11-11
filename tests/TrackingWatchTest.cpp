#include "assist/HeadPoseMapper.h"
#include "core/FakeTracker.h"
#include "core/TobiiPumpHealth.h"
#include "core/TrackingWatch.h"
#include "mapping/HeadPoseCurve.h"

#include <QtTest>

using namespace gazer;

namespace {

GazePoint gazeAt(qint64 t, bool valid, double x = 10.0)
{
    GazePoint p;
    p.timestampMs = t;
    p.valid = valid;
    p.x = x;
    p.y = 20.0;
    return p;
}

HeadPose headAt(qint64 t, double yaw)
{
    HeadPose pose;
    pose.timestampMs = t;
    pose.yaw = yaw;
    pose.yawValid = true;
    pose.pitchValid = true;
    pose.rollValid = true;
    pose.rotationValid = true;
    return pose;
}

HeadPoseMap commandMap()
{
    HeadPoseMap m;
    m.id = QStringLiteral("c");
    m.enabled = true;
    m.dest = HeadPoseDest::Command;
    m.command = QStringLiteral("mouseLeftClick");
    m.commandAt = 10.0;
    m.hysteresis = 2.0;
    m.points = {{0.0, 0.0}, {1.0, 1.0}};
    m.source = HeadPoseAxis::Yaw;
    return m;
}

} // namespace

class TrackingWatchTest final : public QObject {
    Q_OBJECT

private slots:
    void blinkDoesNotNeutralize();
    void defaultsWaitTenSeconds();
    void sustainedInvalidAfterLiveGazeNeutralizes();
    void noCallbacksAfterSampleNeutralizes();
    void startupSilenceIsLongerThanAStall();
    void disconnectIsImmediate();
    void disconnectStaysBlockedUntilGazeReturns();
    void headLossIsIndependentOfGaze();
    void headOnlyLossClearsWhenValidHeadReturns();
    void repeatedInvalidHeadDoesNotStayFresh();
    void sustainedInvalidHeadDoesNotBlockBoards();
    void validHeadDoesNotClearGazeHold();
    void failedReconnectEdgeIsPublished();
    void resumeLatchSkipsTheAcknowledgingSample();
    void fakeTrackerGazeReturnDoesNotActivate();
    void pumpNoCallbacksThenDisconnectAndReconnect();
    void pumpHeadLossWhileGazeContinues();
    void mapperRecoveryDoesNotFireCommand();
};

void TrackingWatchTest::blinkDoesNotNeutralize()
{
    TrackingWatch watch;
    watch.setBlinkGraceMs(200);
    watch.noteStreamRunning(0);
    QVERIFY(!watch.onGaze(gazeAt(0, true), 0).enteredGazeLoss);
    QVERIFY(!watch.onGaze(gazeAt(80, false), 80).enteredGazeLoss);
    QVERIFY(!watch.onGaze(gazeAt(200, false), 200).enteredGazeLoss);
    QVERIFY(!watch.poll(400).enteredGazeLoss);
    QVERIFY(watch.allowsActivation());
    QVERIFY(!watch.blocksBoardGaze());
}

void TrackingWatchTest::defaultsWaitTenSeconds()
{
    QCOMPARE(TrackingWatch::kDefaultStreamStaleMs, 10000);
    QCOMPARE(TrackingWatch::kDefaultSustainedInvalidMs, 10000);
    QCOMPARE(TrackingWatch::kDefaultStartupSilenceMs, 10000);

    TrackingWatch watch;
    watch.noteStreamRunning(0);
    QVERIFY(!watch.onGaze(gazeAt(0, true), 0).enteredGazeLoss);
    QVERIFY(!watch.onGaze(gazeAt(50, false), 50).enteredGazeLoss);
    QVERIFY(!watch.onGaze(gazeAt(50 + 9999, false), 50 + 9999).enteredGazeLoss);
    const TrackingWatch::Transition lost = watch.onGaze(gazeAt(50 + 10000, false), 50 + 10000);
    QVERIFY(lost.enteredGazeLoss);
    QCOMPARE(watch.statusText(), QStringLiteral("No gaze detected"));
    const TrackingWatch::Transition back = watch.onGaze(gazeAt(50 + 10001, true), 50 + 10001);
    QVERIFY(back.resumed);
    QVERIFY(watch.allowsActivation());

    TrackingWatch quiet;
    quiet.noteStreamRunning(0);
    QVERIFY(!quiet.onGaze(gazeAt(0, true), 0).enteredGazeLoss);
    QVERIFY(!quiet.poll(9999).enteredGazeLoss);
    QVERIFY(quiet.poll(10000).enteredGazeLoss);
}

void TrackingWatchTest::sustainedInvalidAfterLiveGazeNeutralizes()
{
    TrackingWatch watch;
    watch.setBlinkGraceMs(200);
    watch.setSustainedInvalidMs(500);
    watch.noteStreamRunning(0);
    QVERIFY(!watch.onGaze(gazeAt(0, true), 0).enteredGazeLoss);
    QVERIFY(!watch.onGaze(gazeAt(50, false), 50).enteredGazeLoss);
    const TrackingWatch::Transition lost = watch.onGaze(gazeAt(50 + 500, false), 50 + 500);
    QVERIFY(lost.enteredGazeLoss);
    QVERIFY(watch.blocksBoardGaze());
    QVERIFY(!watch.allowsActivation());
    QCOMPARE(watch.gazeLossReason(), TrackingWatch::GazeLossReason::SustainedInvalid);
    QVERIFY(!watch.onGaze(gazeAt(900, false), 900).enteredGazeLoss);
}

void TrackingWatchTest::noCallbacksAfterSampleNeutralizes()
{
    TrackingWatch watch;
    watch.setStreamStaleMs(800);
    watch.noteStreamRunning(0);
    QVERIFY(!watch.onGaze(gazeAt(50, true), 50).enteredGazeLoss);
    QVERIFY(!watch.poll(50 + 799).enteredGazeLoss);
    const TrackingWatch::Transition lost = watch.poll(50 + 800);
    QVERIFY(lost.enteredGazeLoss);
    QVERIFY(lost.enteredHeadLoss);
    QCOMPARE(watch.gazeLossReason(), TrackingWatch::GazeLossReason::NoCallbacks);
    QVERIFY(!watch.poll(50 + 1600).enteredGazeLoss);
}

void TrackingWatchTest::startupSilenceIsLongerThanAStall()
{
    TrackingWatch watch;
    watch.setStreamStaleMs(800);
    watch.setStartupSilenceMs(3000);
    watch.noteStreamRunning(0);
    QVERIFY(!watch.poll(800).enteredGazeLoss);
    const TrackingWatch::Transition lost = watch.poll(3000);
    QVERIFY(lost.enteredGazeLoss);
    QCOMPARE(watch.gazeLossReason(), TrackingWatch::GazeLossReason::NoCallbacks);
}

void TrackingWatchTest::disconnectIsImmediate()
{
    TrackingWatch watch;
    watch.noteStreamRunning(0);
    const TrackingWatch::Transition lost = watch.onDisconnect(5);
    QVERIFY(lost.enteredGazeLoss);
    QVERIFY(!watch.allowsActivation());
    QCOMPARE(watch.gazeLossReason(), TrackingWatch::GazeLossReason::Disconnected);
}

void TrackingWatchTest::disconnectStaysBlockedUntilGazeReturns()
{
    TrackingWatch watch;
    watch.noteStreamRunning(0);
    QVERIFY(watch.onGaze(gazeAt(10, true), 10).enteredGazeLoss == false);
    QVERIFY(watch.onDisconnect(20).enteredGazeLoss);
    QCOMPARE(watch.statusText(), QStringLiteral("No gaze detected"));
    QVERIFY(!watch.allowsActivation());

    const TrackingWatch::Transition back = watch.onGaze(gazeAt(40, true), 40);
    QVERIFY(back.resumed);
    QVERIFY(!back.enteredGazeLoss);
    QVERIFY(watch.allowsActivation());
    QVERIFY(!watch.blocksBoardGaze());
    QVERIFY(watch.consumeResumeLatch());
    QVERIFY(!watch.consumeResumeLatch());
}

void TrackingWatchTest::headLossIsIndependentOfGaze()
{
    TrackingWatch watch;
    watch.setStreamStaleMs(800);
    watch.noteStreamRunning(0);
    QVERIFY(!watch.onGaze(gazeAt(0, true), 0).enteredGazeLoss);
    QVERIFY(!watch.onHead(headAt(0, 1.0), 0).enteredHeadLoss);
    QVERIFY(!watch.onGaze(gazeAt(800, true), 800).enteredGazeLoss);
    const TrackingWatch::Transition lost = watch.poll(800);
    QVERIFY(lost.enteredHeadLoss);
    QVERIFY(!lost.enteredGazeLoss);
    QVERIFY(!watch.blocksBoardGaze());
    QVERIFY(watch.headHeld());
    QVERIFY(!watch.allowsActivation());
}

void TrackingWatchTest::headOnlyLossClearsWhenValidHeadReturns()
{
    TrackingWatch watch;
    watch.setStreamStaleMs(800);
    watch.noteStreamRunning(0);
    QVERIFY(!watch.onGaze(gazeAt(0, true), 0).enteredGazeLoss);
    QVERIFY(!watch.onHead(headAt(0, 1.0), 0).enteredHeadLoss);
    QVERIFY(!watch.onGaze(gazeAt(800, true), 800).enteredGazeLoss);

    const TrackingWatch::Transition first = watch.poll(800);
    QVERIFY(first.enteredHeadLoss);
    QVERIFY(!first.enteredGazeLoss);
    QVERIFY(!watch.blocksBoardGaze());
    QVERIFY(watch.headHeld());
    QVERIFY(watch.lossHold());

    QVERIFY(!watch.onGaze(gazeAt(900, true), 900).enteredGazeLoss);
    const TrackingWatch::Transition back = watch.onHead(headAt(900, 2.0), 900);
    QVERIFY(!back.enteredHeadLoss);
    QVERIFY(!back.enteredGazeLoss);
    QVERIFY(!watch.headHeld());
    QVERIFY(!watch.lossHold());
    QVERIFY(!watch.blocksBoardGaze());
    QVERIFY(watch.allowsActivation());

    // Worker signal for a second stall. Must enter again so neutralize runs.
    const TrackingWatch::Transition second = watch.noteHeadStale();
    QVERIFY(second.enteredHeadLoss);
    QVERIFY(!second.enteredGazeLoss);
    QVERIFY(!watch.blocksBoardGaze());
    QVERIFY(watch.headHeld());
    QVERIFY(watch.lossHold());
}

void TrackingWatchTest::repeatedInvalidHeadDoesNotStayFresh()
{
    TrackingWatch watch;
    watch.setStreamStaleMs(800);
    watch.setSustainedInvalidMs(2000);
    watch.noteStreamRunning(0);
    QVERIFY(!watch.onGaze(gazeAt(0, true), 0).enteredGazeLoss);
    QVERIFY(!watch.onHead(headAt(0, 1.0), 0).enteredHeadLoss);

    HeadPose bad;
    bad.timestampMs = 100;
    QVERIFY(!watch.onHead(bad, 100).enteredHeadLoss);
    bad.timestampMs = 300;
    QVERIFY(!watch.onHead(bad, 300).enteredHeadLoss);
    QVERIFY(!watch.headHeld());
    QVERIFY(!watch.blocksBoardGaze());

    QVERIFY(!watch.onGaze(gazeAt(400, true), 400).enteredGazeLoss);
    QVERIFY(!watch.poll(400).enteredHeadLoss);

    bad.timestampMs = 700;
    QVERIFY(!watch.onHead(bad, 700).enteredHeadLoss);
    QVERIFY(!watch.onGaze(gazeAt(800, true), 800).enteredGazeLoss);
    const TrackingWatch::Transition lost = watch.poll(800);
    QVERIFY(lost.enteredHeadLoss);
    QVERIFY(!lost.enteredGazeLoss);
    QVERIFY(!watch.blocksBoardGaze());
    QVERIFY(watch.headHeld());
}

void TrackingWatchTest::sustainedInvalidHeadDoesNotBlockBoards()
{
    TrackingWatch watch;
    watch.setStreamStaleMs(5000);
    watch.setSustainedInvalidMs(500);
    watch.noteStreamRunning(0);
    QVERIFY(!watch.onGaze(gazeAt(0, true), 0).enteredGazeLoss);
    QVERIFY(!watch.onHead(headAt(0, 1.0), 0).enteredHeadLoss);

    HeadPose bad;
    bad.timestampMs = 10;
    QVERIFY(!watch.onHead(bad, 10).enteredHeadLoss);
    QVERIFY(!watch.blocksBoardGaze());

    bad.timestampMs = 510;
    const TrackingWatch::Transition lost = watch.onHead(bad, 510);
    QVERIFY(lost.enteredHeadLoss);
    QVERIFY(!lost.enteredGazeLoss);
    QVERIFY(!watch.blocksBoardGaze());
    QVERIFY(watch.headHeld());
}

void TrackingWatchTest::validHeadDoesNotClearGazeHold()
{
    TrackingWatch watch;
    watch.noteStreamRunning(0);
    QVERIFY(!watch.onGaze(gazeAt(0, true), 0).enteredGazeLoss);
    QVERIFY(watch.onDisconnect(10).enteredGazeLoss);
    QVERIFY(!watch.onHead(headAt(20, 3.0), 20).enteredHeadLoss);
    QVERIFY(watch.blocksBoardGaze());
    QVERIFY(watch.headHeld());
    QVERIFY(watch.lossHold());
    QVERIFY(!watch.allowsActivation());
}

void TrackingWatchTest::failedReconnectEdgeIsPublished()
{
    TobiiPumpHealth pump;
    pump.noteStreamStart(0);
    const TobiiPumpHealth::Edges fail =
        pump.onReconnectFinished(false, 5, QStringLiteral("no device"));
    QVERIFY(fail.failed);
    QVERIFY(fail.actionable());
    QVERIFY(!fail.reconnected);
    QVERIFY(!fail.gazeStale);
    QVERIFY(!fail.headStale);
    QVERIFY(!fail.disconnected);
    QCOMPARE(fail.reason, QStringLiteral("no device"));

    TobiiPumpHealth::Edges quiet;
    QVERIFY(!quiet.actionable());
}

void TrackingWatchTest::resumeLatchSkipsTheAcknowledgingSample()
{
    TrackingWatch watch;
    watch.noteStreamRunning(0);
    QVERIFY(!watch.onGaze(gazeAt(0, true), 0).enteredGazeLoss);
    QVERIFY(watch.onDisconnect(10).enteredGazeLoss);
    QVERIFY(watch.blocksBoardGaze());
    const TrackingWatch::Transition back = watch.onGaze(gazeAt(20, true), 20);
    QVERIFY(back.resumed);
    QVERIFY(!watch.blocksBoardGaze());
    QVERIFY(watch.consumeResumeLatch());
}

void TrackingWatchTest::fakeTrackerGazeReturnDoesNotActivate()
{
    FakeTracker tracker;
    QVERIFY(tracker.start());
    QCOMPARE(tracker.name(), QStringLiteral("Fake"));

    TrackingWatch watch;
    int boardFeeds = 0;
    const auto feed = [&](const GazePoint& point) {
        const bool blockedBefore = watch.blocksBoardGaze();
        (void)watch.onGaze(point, point.timestampMs);
        if (blockedBefore || watch.blocksBoardGaze() || watch.consumeResumeLatch()) {
            return;
        }
        ++boardFeeds;
    };
    connect(&tracker, &ITracker::gazeUpdated, &tracker, feed);
    connect(&tracker, &FakeTracker::deviceDisconnected, &tracker, [&]() {
        (void)watch.onDisconnect(15);
    });

    watch.noteStreamRunning(0);
    tracker.emitGaze(gazeAt(10, true));
    QCOMPARE(boardFeeds, 1);
    tracker.emitDisconnected();
    QVERIFY(watch.blocksBoardGaze());
    tracker.emitGaze(gazeAt(50, true, 400.0));
    QCOMPARE(boardFeeds, 1);
    QVERIFY(watch.allowsActivation());
    // The sample that clears the hold is skipped, and the resume latch skips
    // the next one.
    tracker.emitGaze(gazeAt(80, true, 400.0));
    QCOMPARE(boardFeeds, 1);
    tracker.emitGaze(gazeAt(100, true, 400.0));
    QCOMPARE(boardFeeds, 2);
}

void TrackingWatchTest::pumpNoCallbacksThenDisconnectAndReconnect()
{
    TobiiPumpHealth pump;
    pump.noteStreamStart(0);
    QVERIFY(!pump.onWaitTimeout(TrackingWatch::kDefaultStartupSilenceMs - 1).gazeStale);

    TobiiPumpHealth::Edges startup =
        pump.onWaitTimeout(TrackingWatch::kDefaultStartupSilenceMs);
    QVERIFY(startup.gazeStale);
    QVERIFY(!startup.headStale);
    QVERIFY(!pump.onWaitTimeout(TrackingWatch::kDefaultStartupSilenceMs + 100).gazeStale);

    TobiiPumpHealth live;
    live.noteStreamStart(0);
    live.onGazeCallback(10);
    QVERIFY(!live.onWaitTimeout(10 + TrackingWatch::kDefaultStreamStaleMs - 1).gazeStale);
    TobiiPumpHealth::Edges stalled = live.onWaitTimeout(10 + TrackingWatch::kDefaultStreamStaleMs);
    QVERIFY(stalled.gazeStale);

    TobiiPumpHealth drop;
    drop.noteStreamStart(0);
    TobiiPumpHealth::Edges disc = drop.onConnectionFailed(5);
    QVERIFY(disc.disconnected);
    QVERIFY(!disc.gazeStale);
    QVERIFY(!drop.onConnectionFailed(6).disconnected);

    TobiiPumpHealth::Edges back = drop.onReconnectFinished(true, 7, {});
    QVERIFY(back.reconnected);
    QVERIFY(!back.failed);
    QVERIFY(!back.gazeStale);
    QVERIFY(!drop.onReconnectFinished(true, 8, {}).reconnected);
    drop.onGazeCallback(9);
    QVERIFY(drop.onReconnectFinished(true, 12, {}).reconnected);

    TobiiPumpHealth::Edges fail = drop.onReconnectFinished(false, 13, QStringLiteral("no device"));
    QVERIFY(fail.failed);
    QVERIFY(fail.actionable());
    QCOMPARE(fail.reason, QStringLiteral("no device"));
}

void TrackingWatchTest::pumpHeadLossWhileGazeContinues()
{
    TobiiPumpHealth pump;
    pump.setHeadSubscribed(true);
    pump.noteStreamStart(0);
    pump.onGazeCallback(100);
    pump.onHeadCallback(100);
    pump.onGazeCallback(100 + TrackingWatch::kDefaultStreamStaleMs);
    const TobiiPumpHealth::Edges edges =
        pump.onAfterCallbacks(100 + TrackingWatch::kDefaultStreamStaleMs);
    QVERIFY(edges.headStale);
    QVERIFY(!edges.gazeStale);

    TobiiPumpHealth both;
    both.setHeadSubscribed(true);
    both.noteStreamStart(0);
    both.onGazeCallback(100);
    both.onHeadCallback(100);
    const TobiiPumpHealth::Edges dead = both.onWaitTimeout(100 + TrackingWatch::kDefaultStreamStaleMs);
    QVERIFY(dead.gazeStale);
    QVERIFY(!dead.headStale);
}

void TrackingWatchTest::mapperRecoveryDoesNotFireCommand()
{
    HeadPoseMapper mapper;
    QStringList fired;
    mapper.setRunCommand([&](const QString& name, QString*) {
        fired.push_back(name);
        return true;
    });
    mapper.setEnabled(true);
    mapper.setMaps({commandMap()});

    mapper.onPose(headAt(10, 0.0));
    QVERIFY(fired.isEmpty());

    mapper.beginStreamLoss();
    HeadPose junk = headAt(20, 0.0);
    junk.yawValid = false;
    junk.rotationValid = true;
    junk.positionValid = true;
    mapper.onPose(junk);
    mapper.onPose(headAt(30, 12.0));
    QVERIFY(fired.isEmpty());
    mapper.onPose(headAt(50, 12.0));
    QVERIFY(fired.isEmpty());

    mapper.onPose(headAt(70, 0.0));
    mapper.onPose(headAt(90, 12.0));
    QCOMPARE(fired.size(), 1);
    QCOMPARE(fired.first(), QStringLiteral("mouseLeftClick"));
}

QObject* createTrackingWatchTest()
{
    return new TrackingWatchTest;
}

#include "TrackingWatchTest.moc"
