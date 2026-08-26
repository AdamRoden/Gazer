#include "assist/ComboMouseHit.h"

#include <QtTest>

using namespace gazer;

class ComboMouseTest final : public QObject {
    Q_OBJECT

private slots:
    void deadzoneAndDriftAndOutside();
    void slicesClockwiseFromTop();
};

void ComboMouseTest::deadzoneAndDriftAndOutside()
{
    const QPointF o(100, 100);
    const auto hole = ComboMouseHit::hit(QPointF(100, 100), o, 40, 70, 120);
    QCOMPARE(hole.band, ComboMouseHit::Band::Deadzone);

    const auto drift = ComboMouseHit::hit(QPointF(100, 50), o, 40, 70, 120);
    QCOMPARE(drift.band, ComboMouseHit::Band::Drift);

    const auto miss = ComboMouseHit::hit(QPointF(100, -50), o, 40, 70, 120);
    QCOMPARE(miss.band, ComboMouseHit::Band::None);
}

void ComboMouseTest::slicesClockwiseFromTop()
{
    const QPointF o(0, 0);
    const double dead = 40;
    const double ring = 70;
    const double pie = 120;
    auto at = [&](double x, double y) {
        return ComboMouseHit::hit(QPointF(x, y), o, dead, ring, pie);
    };

    const auto top = at(0, -100);
    QCOMPARE(top.band, ComboMouseHit::Band::Slice);
    QCOMPARE(top.slice, ComboMouseHit::Slice::Right);

    const auto right = at(100, 40);
    QCOMPARE(right.band, ComboMouseHit::Band::Slice);
    QCOMPARE(right.slice, ComboMouseHit::Slice::Move);

    const auto bottom = at(0, 100);
    QCOMPARE(bottom.band, ComboMouseHit::Band::Slice);
    QCOMPARE(bottom.slice, ComboMouseHit::Slice::Cancel);

    const auto left = at(-100, 0);
    QCOMPARE(left.band, ComboMouseHit::Band::Slice);
    QCOMPARE(left.slice, ComboMouseHit::Slice::Drag);

    const auto topLeft = at(-70, -70);
    QCOMPARE(topLeft.band, ComboMouseHit::Band::Slice);
    QCOMPARE(topLeft.slice, ComboMouseHit::Slice::Left);
}

QObject* createComboMouseTest()
{
    return new ComboMouseTest;
}

#include "ComboMouseTest.moc"
