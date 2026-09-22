#include "app/ActionChannel.h"
#include "app/InboundActions.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QTest>
#include <QUuid>
#include <atomic>
#include <thread>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

using namespace gazer;

class ActionChannelTest final : public QObject {
    Q_OBJECT

private slots:
    void ackRoundtrip();
    void pingDoesNotDispatch();
    void hungPeerTimesOut();
};

namespace {
QString uniquePipe()
{
    return QStringLiteral("GazerTest-%1").arg(QUuid::createUuid().toString(QUuid::Id128));
}
} // namespace

void ActionChannelTest::ackRoundtrip()
{
    qputenv("GAZER_ACTION_PIPE", uniquePipe().toUtf8());
    ActionChannel server;
    QVERIFY(server.listen());
    QString got;
    server.setDispatch([&](const QString& p) { got = p; });
    QString err;
    std::atomic<int> rc{int(PeerResult::ConnectFailed)};
    std::thread t([&] {
        rc.store(int(ActionChannel::sendToPeer(QStringLiteral("command=toggleLookToScroll"), &err)));
    });
    QTRY_COMPARE(got, QStringLiteral("command=toggleLookToScroll"));
    t.join();
    QCOMPARE(rc.load(), int(PeerResult::Ok));
}

void ActionChannelTest::pingDoesNotDispatch()
{
    qputenv("GAZER_ACTION_PIPE", uniquePipe().toUtf8());
    ActionChannel server;
    QVERIFY(server.listen());
    int n = 0;
    server.setDispatch([&](const QString&) { ++n; });
    QString err;
    std::atomic<int> rc{int(PeerResult::ConnectFailed)};
    std::thread t([&] { rc.store(int(ActionChannel::sendToPeer(QStringLiteral("ping"), &err))); });
    QTRY_COMPARE(rc.load(), int(PeerResult::Ok));
    t.join();
    QCOMPARE(n, 0);
    QVERIFY(isInboundPing(QStringLiteral(" ping ")));
}

void ActionChannelTest::hungPeerTimesOut()
{
    qputenv("GAZER_ACTION_PIPE", uniquePipe().toUtf8());
    QLocalServer hung;
    hung.setSocketOptions(QLocalServer::UserAccessOption);
    QObject::connect(&hung, &QLocalServer::newConnection, &hung, [&]() {
        QLocalSocket* s = hung.nextPendingConnection();
        if (s) {
            s->setParent(&hung);
        }
    });
    QVERIFY(hung.listen(ActionChannel::pipeName()));
    QString err;
    quint32 pid = 0;
    std::atomic<int> rc{int(PeerResult::ConnectFailed)};
    std::thread t([&] {
        rc.store(int(ActionChannel::sendToPeer(QStringLiteral("raise"), &err, &pid)));
    });
    QTRY_COMPARE(rc.load(), int(PeerResult::Timeout));
    t.join();
    QCOMPARE(rc.load(), int(PeerResult::Timeout));
    QVERIFY(!err.isEmpty());
#ifdef Q_OS_WIN
    QCOMPARE(pid, quint32(GetCurrentProcessId()));
#endif
}

QObject* createActionChannelTest()
{
    return new ActionChannelTest;
}

#include "ActionChannelTest.moc"
