#include "assist/MouseLeftDrag.h"

#include <QtTest>

using namespace gazer;

namespace {

const DragFanButton* button(const DragFan& fan, DragChoice choice)
{
    for (const DragFanButton& b : fan.buttons) {
        if (b.choice == choice) {
            return &b;
        }
    }
    return nullptr;
}

double dist(QPointF a, QPointF b)
{
    return std::hypot(a.x() - b.x(), a.y() - b.y());
}

} // namespace

class MouseLeftDragTest final : public QObject {
    Q_OBJECT

private slots:
    void lowerPointFansAbove();
    void upperPointFansTowardCenter();
    void leftPointFansTowardCenter();
    void nearCenterFansAbove();
    void cornerButtonsStayApart();
    void hitPicksButtonNotAnchor();
};

void MouseLeftDragTest::lowerPointFansAbove()
{
    const QRectF screen(0, 0, 1920, 1080);
    const QPointF anchor(960, 980);
    const DragFan fan = layoutDragFan(anchor, screen);
    const DragFanButton* move = button(fan, DragChoice::Move);
    const DragFanButton* release = button(fan, DragChoice::Release);
    const DragFanButton* cancel = button(fan, DragChoice::Cancel);
    QVERIFY(move && release && cancel);
    QVERIFY(fan.facing.y() < -0.8);
    QVERIFY(move->center.y() < anchor.y() - 40.0);
    QVERIFY(release->center.y() < anchor.y());
    QVERIFY(cancel->center.y() < anchor.y());
    QVERIFY(release->center.x() < move->center.x());
    QVERIFY(cancel->center.x() > move->center.x());
    QVERIFY(dist(anchor, move->center) > 140.0);
    QVERIFY(dist(move->center, release->center) > fan.buttonRadius * 2.0);
    QVERIFY(dist(move->center, cancel->center) > fan.buttonRadius * 2.0);
    QVERIFY(dragFanFits(fan, screen));
}

void MouseLeftDragTest::upperPointFansTowardCenter()
{
    const QRectF screen(0, 0, 1920, 1080);
    const QPointF anchor(960, 80);
    const DragFan fan = layoutDragFan(anchor, screen);
    const DragFanButton* move = button(fan, DragChoice::Move);
    QVERIFY(move);
    QVERIFY(fan.facing.y() > 0.8);
    QVERIFY(move->center.y() > anchor.y() + 40.0);
    QVERIFY(dragFanFits(fan, screen));
}

void MouseLeftDragTest::leftPointFansTowardCenter()
{
    const QRectF screen(0, 0, 1920, 1080);
    const QPointF anchor(40, 540);
    const DragFan fan = layoutDragFan(anchor, screen);
    const DragFanButton* move = button(fan, DragChoice::Move);
    QVERIFY(move);
    QVERIFY(fan.facing.x() > 0.8);
    QVERIFY(move->center.x() > anchor.x() + 40.0);
    QVERIFY(dragFanFits(fan, screen));
}

void MouseLeftDragTest::nearCenterFansAbove()
{
    const QRectF screen(0, 0, 1920, 1080);
    const DragFan fan = layoutDragFan(screen.center(), screen);
    const DragFanButton* move = button(fan, DragChoice::Move);
    QVERIFY(move);
    QCOMPARE(fan.facing, QPointF(0, -1));
    QVERIFY(move->center.y() < screen.center().y());
}

void MouseLeftDragTest::cornerButtonsStayApart()
{
    const QRectF screen(0, 0, 1920, 1080);
    const DragFan fan = layoutDragFan(QPointF(2, 2), screen);
    QVERIFY(dragFanFits(fan, screen));
    for (int i = 0; i < 3; ++i) {
        for (int j = i + 1; j < 3; ++j) {
            QVERIFY(dist(fan.buttons[i].center, fan.buttons[j].center) + 1.0
                    >= fan.buttonRadius * 2.0);
        }
        QVERIFY(dist(fan.anchor, fan.buttons[i].center) + 1.0 >= fan.buttonRadius);
    }
}

void MouseLeftDragTest::hitPicksButtonNotAnchor()
{
    const QRectF screen(0, 0, 1920, 1080);
    const QPointF anchor(960, 980);
    const DragFan fan = layoutDragFan(anchor, screen);
    const DragFanButton* release = button(fan, DragChoice::Release);
    QVERIFY(release);
    int releaseIndex = -1;
    for (int i = 0; i < 3; ++i) {
        if (fan.buttons[i].choice == DragChoice::Release) {
            releaseIndex = i;
        }
    }
    QCOMPARE(hitDragFan(fan, release->center), releaseIndex);
    QCOMPARE(hitDragFan(fan, anchor), -1);
    QCOMPARE(hitDragFan(fan, release->center + QPointF(0, -(fan.hitRadius + 4.0))), -1);
}

QObject* createMouseLeftDragTest()
{
    return new MouseLeftDragTest;
}

#include "MouseLeftDragTest.moc"
