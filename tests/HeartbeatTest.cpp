#include "utils/Heartbeat.h"

#include <QtTest>

using namespace gazer;

class HeartbeatTest final : public QObject {
    Q_OBJECT

private slots:
    void crashLoopCountsWindow();
    void hostGuardRoundtrip();
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
#else
    QSKIP("Windows-only");
#endif
}

QObject* createHeartbeatTest()
{
    return new HeartbeatTest;
}

#include "HeartbeatTest.moc"
