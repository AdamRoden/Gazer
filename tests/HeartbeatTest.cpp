#include "utils/Heartbeat.h"

#include <QtTest>

using namespace gazer;

class HeartbeatTest final : public QObject {
    Q_OBJECT

private slots:
    void crashLoopCountsWindow();
    void hostGuardRoundtrip();
    void cleanShutdownSurvivesPulse();
    void publishGazeRoundtrip();
};

void HeartbeatTest::crashLoopCountsWindow()
{
    QVERIFY(!crashLoopTripped({}, 100000));
    QVERIFY(!crashLoopTripped({1000, 2000}, 3000));
    QVERIFY(crashLoopTripped({1000, 2000, 3000}, 3000));
    QVERIFY(!crashLoopTripped({1, 2, 3}, 200000));
}

void HeartbeatTest::hostGuardRoundtrip()
{
#ifdef Q_OS_WIN
    qputenv("GAZER_HEARTBEAT_MAP", "Local\\GazerHeartbeatTest");
    Heartbeat host;
    QVERIFY(host.openAsHost());
    host.pulseGui();
    host.setExclusiveOccluded(true);
    Heartbeat guard;
    QVERIFY(guard.openAsGuard());
    const HeartbeatSnapshot s = guard.read();
    QVERIFY(s.valid);
    QVERIFY(s.hostPid != 0);
    QVERIFY(s.flags & HeartbeatFlag::ExclusiveOccluded);
    QVERIFY(s.flags & HeartbeatFlag::HostReady);
    QVERIFY(!Heartbeat::guiStale(s, 5000));
    host.setCleanShutdown();
    QVERIFY(guard.read().flags & HeartbeatFlag::CleanShutdown);
    host.pulseGui();
    const HeartbeatSnapshot after = guard.read();
    QVERIFY(after.flags & HeartbeatFlag::CleanShutdown);
    QVERIFY(!(after.flags & HeartbeatFlag::HostReady));
#else
    QSKIP("Windows-only");
#endif
}

void HeartbeatTest::cleanShutdownSurvivesPulse()
{
#ifdef Q_OS_WIN
    qputenv("GAZER_HEARTBEAT_MAP", "Local\\GazerHeartbeatTestClean");
    Heartbeat host;
    QVERIFY(host.openAsHost());
    host.pulseGui();
    const qint64 tick = host.read().guiTickMs;
    host.setCleanShutdown();
    QTest::qWait(20);
    host.pulseGui();
    const HeartbeatSnapshot s = host.read();
    QVERIFY(s.flags & HeartbeatFlag::CleanShutdown);
    QVERIFY(!(s.flags & HeartbeatFlag::HostReady));
    QCOMPARE(s.guiTickMs, tick);
#else
    QSKIP("Windows-only");
#endif
}

void HeartbeatTest::publishGazeRoundtrip()
{
#ifdef Q_OS_WIN
    qputenv("GAZER_HEARTBEAT_MAP", "Local\\GazerHeartbeatTestGaze");
    Heartbeat host;
    QVERIFY(host.openAsHost());
    Heartbeat::publishGaze(12, -4, true);
    const HeartbeatSnapshot s = host.read();
    QVERIFY(s.gazeValid);
    QCOMPARE(s.gazeX, 12);
    QCOMPARE(s.gazeY, -4);
    QVERIFY(Heartbeat::gazeFresh(s, 1000));
    Heartbeat::publishGaze(0, 0, false);
    QVERIFY(!host.read().gazeValid);
    QVERIFY(!Heartbeat::gazeFresh(host.read(), 1000));
#else
    QSKIP("Windows-only");
#endif
}

QObject* createHeartbeatTest()
{
    return new HeartbeatTest;
}

#include "HeartbeatTest.moc"
