#include "assist/ActionLoopService.h"
#include "layout/PageTypes.h"

#include <QSignalSpy>
#include <QtTest>

using namespace gazer;

class ActionLoopServiceTest final : public QObject {
    Q_OBJECT

private slots:
    void startStop();
};

void ActionLoopServiceTest::startStop()
{
    ActionLoopService loops;
    int fires = 0;
    loops.setDispatchFn([&](const QVector<PageAction>&, const QString&) { ++fires; });
    PageAction a;
    a.type = PageActionType::Command;
    a.command = QStringLiteral("mouseLeftClick");
    QVERIFY(loops.start(QStringLiteral("p"), QStringLiteral("c"), {a}, {}));
    QVERIFY(loops.isActive(QStringLiteral("p"), QStringLiteral("c")));
    QTRY_VERIFY(fires >= 1);
    loops.stopAll();
    QVERIFY(!loops.anyActive());
}

QObject* createActionLoopServiceTest()
{
    return new ActionLoopServiceTest;
}

#include "ActionLoopServiceTest.moc"
