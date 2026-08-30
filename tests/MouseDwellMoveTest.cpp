#include "assist/MouseDwellMove.h"

#include <QtTest>

using namespace gazer;

class MouseDwellMoveTest final : public QObject {
    Q_OBJECT

private slots:
    void selectBudgetAddsPhaseDwell();
    void selectBudgetOffWhenTimeoutZero();
};

void MouseDwellMoveTest::selectBudgetAddsPhaseDwell()
{
    QCOMPARE(MouseDwellMove::selectDeadlineBudgetMs(5000, 800), 5800);
    QCOMPARE(MouseDwellMove::selectDeadlineBudgetMs(5000, 400), 5400);
}

void MouseDwellMoveTest::selectBudgetOffWhenTimeoutZero()
{
    QCOMPARE(MouseDwellMove::selectDeadlineBudgetMs(0, 800), 0);
    QCOMPARE(MouseDwellMove::selectDeadlineBudgetMs(-1, 800), 0);
}

QObject* createMouseDwellMoveTest()
{
    return new MouseDwellMoveTest;
}

#include "MouseDwellMoveTest.moc"
