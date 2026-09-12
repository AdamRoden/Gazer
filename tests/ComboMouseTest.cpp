#include "assist/ComboMouseHit.h"

#include <QtTest>

using namespace gazer;

class ComboMouseTest final : public QObject {
    Q_OBJECT

private slots:
    void deadzoneAndDriftAndOutside();
    void slicesClockwiseFromTop();
    void packingEdgesAndCorners();
    void comboRegionOrders();
    void halfLeftKeepsFiveSlices();
    void halfLeftOffArcMisses();
    void offScreenHoleAndRingStillHit();
    void overlayRectStaysOnScreen();
    void yellowRingSnapsEightWays();
    void radiiStayAsGiven();
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

void ComboMouseTest::packingEdgesAndCorners()
{
    const QRectF screen(0, 0, 1920, 1080);
    const double pie = 200;
    auto pack = [&](QPointF o) { return ComboMouseHit::makeComboLayout(o, screen, 60, 120, pie); };

    const auto full = pack(QPointF(960, 540));
    QCOMPARE(full.arcStartDeg, 0.0);
    QCOMPARE(full.arcSpanDeg, 360.0);
    QCOMPARE(full.pieOuter, pie);
    QCOMPARE(full.wedgeCount, ComboMouseHit::kSliceCount);

    const auto left = pack(QPointF(0, 540));
    QCOMPARE(left.arcStartDeg, 0.0);
    QCOMPARE(left.arcSpanDeg, 180.0);
    QCOMPARE(left.pieOuter, pie);

    const auto right = pack(QPointF(1919, 540));
    QCOMPARE(right.arcStartDeg, 180.0);
    QCOMPARE(right.arcSpanDeg, 180.0);
    QCOMPARE(right.pieOuter, pie);

    const auto top = pack(QPointF(960, 0));
    QCOMPARE(top.arcStartDeg, 90.0);
    QCOMPARE(top.arcSpanDeg, 180.0);

    const auto bottom = pack(QPointF(960, 1079));
    QCOMPARE(bottom.arcStartDeg, 270.0);
    QCOMPARE(bottom.arcSpanDeg, 180.0);

    const auto tl = pack(QPointF(0, 0));
    QCOMPARE(tl.arcStartDeg, 90.0);
    QCOMPARE(tl.arcSpanDeg, 90.0);
    QCOMPARE(tl.pieOuter, pie * ComboMouseHit::kCornerOuterScale);

    const auto tr = pack(QPointF(1919, 0));
    QCOMPARE(tr.arcStartDeg, 180.0);
    QCOMPARE(tr.arcSpanDeg, 90.0);
    QCOMPARE(tr.pieOuter, pie * ComboMouseHit::kCornerOuterScale);

    const auto br = pack(QPointF(1919, 1079));
    QCOMPARE(br.arcStartDeg, 270.0);
    QCOMPARE(br.arcSpanDeg, 90.0);
    QCOMPARE(br.pieOuter, pie * ComboMouseHit::kCornerOuterScale);

    const auto bl = pack(QPointF(0, 1079));
    QCOMPARE(bl.arcStartDeg, 0.0);
    QCOMPARE(bl.arcSpanDeg, 90.0);
    QCOMPARE(bl.pieOuter, pie * ComboMouseHit::kCornerOuterScale);
}

void ComboMouseTest::comboRegionOrders()
{
    const QRectF screen(0, 0, 1920, 1080);
    const double dead = 60;
    const double ring = 120;
    const double pie = 200;
    using S = ComboMouseHit::Slice;
    struct Case {
        QPointF o;
        double fillStart;
        bool clockwise;
        double span;
        double outer;
        S order[ComboMouseHit::kSliceCount];
    };
    const Case cases[] = {
        {{960, 540}, 0.0, true, 360.0, pie, {S::Drag, S::Move, S::Cancel, S::Left, S::Right}},
        {{0, 540}, 0.0, true, 180.0, pie, {S::Left, S::Right, S::Drag, S::Move, S::Cancel}},
        {{1919, 540}, 0.0, false, 180.0, pie, {S::Left, S::Right, S::Drag, S::Move, S::Cancel}},
        {{960, 0}, 270.0, false, 180.0, pie, {S::Left, S::Right, S::Drag, S::Move, S::Cancel}},
        {{960, 1079}, 270.0, true, 180.0, pie, {S::Left, S::Right, S::Drag, S::Move, S::Cancel}},
        {{0, 0}, 180.0, false, 90.0, pie * 2.0, {S::Left, S::Right, S::Drag, S::Move, S::Cancel}},
        {{1919, 0}, 270.0, false, 90.0, pie * 2.0, {S::Left, S::Right, S::Drag, S::Move, S::Cancel}},
        {{1919, 1079}, 270.0, true, 90.0, pie * 2.0, {S::Left, S::Right, S::Drag, S::Move, S::Cancel}},
        {{0, 1079}, 0.0, true, 90.0, pie * 2.0, {S::Left, S::Right, S::Drag, S::Move, S::Cancel}},
    };
    for (const auto& c : cases) {
        const auto L = ComboMouseHit::makeComboLayout(c.o, screen, dead, ring, pie);
        QCOMPARE(L.arcSpanDeg, c.span);
        QCOMPARE(L.pieOuter, c.outer);
        QCOMPARE(L.wedgeCount, ComboMouseHit::kSliceCount);
        const double sliceDeg = c.span / double(ComboMouseHit::kSliceCount);
        const double r = (L.ringOuter + L.pieOuter) * 0.5;
        for (int i = 0; i < ComboMouseHit::kSliceCount; ++i) {
            const double cw = c.clockwise ? c.fillStart + (double(i) + 0.5) * sliceDeg
                                          : c.fillStart - (double(i) + 0.5) * sliceDeg;
            const auto h = ComboMouseHit::hit(ComboMouseHit::pointOnRay(c.o, cw, r), c.o, L);
            QCOMPARE(h.band, ComboMouseHit::Band::Slice);
            QCOMPARE(h.slice, c.order[i]);
            QCOMPARE(L.wedges[i].id, c.order[i]);
            QCOMPARE(L.wedges[i].inner, ring);
            QCOMPARE(L.wedges[i].outer, c.outer);
        }
    }
}

void ComboMouseTest::halfLeftKeepsFiveSlices()
{
    const QRectF screen(0, 0, 1920, 1080);
    const QPointF o(0, 540);
    const auto L = ComboMouseHit::makeComboLayout(o, screen, 60, 120, 200);
    QCOMPARE(L.arcSpanDeg, 180.0);
    QCOMPARE(L.wedgeCount, ComboMouseHit::kSliceCount);
    QCOMPARE(L.pieOuter, 200.0);

    auto at = [&](double x, double y) { return ComboMouseHit::hit(QPointF(x, y), o, L); };

    const auto up = at(20, 540 - 180);
    QCOMPARE(up.band, ComboMouseHit::Band::Slice);
    QCOMPARE(up.slice, ComboMouseHit::Slice::Left);

    const auto right = at(180, 540);
    QCOMPARE(right.band, ComboMouseHit::Band::Slice);
    QCOMPARE(right.slice, ComboMouseHit::Slice::Drag);

    const auto down = at(20, 540 + 180);
    QCOMPARE(down.band, ComboMouseHit::Band::Slice);
    QCOMPARE(down.slice, ComboMouseHit::Slice::Cancel);

    QCOMPARE(at(o.x() + 10, o.y()).band, ComboMouseHit::Band::Deadzone);
    QCOMPARE(at(o.x() + 90, o.y()).band, ComboMouseHit::Band::Drift);
}

void ComboMouseTest::halfLeftOffArcMisses()
{
    const QRectF screen(0, 0, 1920, 1080);
    const QPointF o(0, 540);
    const auto L = ComboMouseHit::makeComboLayout(o, screen, 60, 120, 200);
    const auto miss = ComboMouseHit::hit(QPointF(-180, 540), o, L);
    QCOMPARE(miss.band, ComboMouseHit::Band::None);
}

void ComboMouseTest::offScreenHoleAndRingStillHit()
{
    const QRectF screen(0, 0, 1920, 1080);
    const QPointF o(0, 0);
    const auto L = ComboMouseHit::makeComboLayout(o, screen, 60, 120, 200);
    QCOMPARE(ComboMouseHit::hit(QPointF(-20, -20), o, L).band, ComboMouseHit::Band::Deadzone);
    QCOMPARE(ComboMouseHit::hit(QPointF(-80, 0), o, L).band, ComboMouseHit::Band::Drift);
    QCOMPARE(ComboMouseHit::hit(QPointF(80, 80), o, L).band, ComboMouseHit::Band::Drift);
    const double r = (L.ringOuter + L.pieOuter) * 0.5;
    const auto slice = ComboMouseHit::hit(ComboMouseHit::pointOnRay(o, 171.0, r), o, L);
    QCOMPARE(slice.band, ComboMouseHit::Band::Slice);
    QCOMPARE(slice.slice, ComboMouseHit::Slice::Left);
}

void ComboMouseTest::overlayRectStaysOnScreen()
{
    const QRectF screen(0, 0, 1920, 1080);
    const QPointF o(0, 0);
    const auto L = ComboMouseHit::makeComboLayout(o, screen, 60, 120, 200);
    const QRect wr = ComboMouseHit::overlayRect(o, L, screen);
    QVERIFY(screen.contains(QRectF(wr)));
    QVERIFY(wr.contains(0, 0));
}

void ComboMouseTest::radiiStayAsGiven()
{
    const QRectF screen(0, 0, 1920, 1080);
    const auto center = ComboMouseHit::makeComboLayout(QPointF(960, 540), screen, 50, 110, 210);
    QCOMPARE(center.deadzone, 50.0);
    QCOMPARE(center.ringOuter, 110.0);
    QCOMPARE(center.pieOuter, 210.0);

    const auto edge = ComboMouseHit::makeComboLayout(QPointF(0, 540), screen, 50, 110, 210);
    QCOMPARE(edge.deadzone, 50.0);
    QCOMPARE(edge.ringOuter, 110.0);
    QCOMPARE(edge.pieOuter, 210.0);

    const auto corner = ComboMouseHit::makeComboLayout(QPointF(0, 0), screen, 50, 110, 210);
    QCOMPARE(corner.deadzone, 50.0);
    QCOMPARE(corner.ringOuter, 110.0);
    QCOMPARE(corner.pieOuter, 420.0);
    const auto* right = ComboMouseHit::wedgeById(corner, ComboMouseHit::Slice::Right);
    const auto* move = ComboMouseHit::wedgeById(corner, ComboMouseHit::Slice::Move);
    QVERIFY(right && move);
    QCOMPARE(right->inner, 110.0);
    QCOMPARE(right->outer, 420.0);
    QCOMPARE(move->inner, 110.0);
    QCOMPARE(move->outer, 420.0);
}

void ComboMouseTest::yellowRingSnapsEightWays()
{
    QCOMPARE(ComboMouseHit::snap8(QPointF(10, 0)), QPoint(1, 0));
    QCOMPARE(ComboMouseHit::snap8(QPointF(10, 10)), QPoint(1, 1));
    QCOMPARE(ComboMouseHit::snap8(QPointF(0, 10)), QPoint(0, 1));
    QCOMPARE(ComboMouseHit::snap8(QPointF(-10, 10)), QPoint(-1, 1));
    QCOMPARE(ComboMouseHit::snap8(QPointF(-10, 0)), QPoint(-1, 0));
    QCOMPARE(ComboMouseHit::snap8(QPointF(-10, -10)), QPoint(-1, -1));
    QCOMPARE(ComboMouseHit::snap8(QPointF(0, -10)), QPoint(0, -1));
    QCOMPARE(ComboMouseHit::snap8(QPointF(10, -10)), QPoint(1, -1));
    QCOMPARE(ComboMouseHit::snap8(QPointF(0, 0)), QPoint(0, 0));
}

QObject* createComboMouseTest()
{
    return new ComboMouseTest;
}

#include "ComboMouseTest.moc"
