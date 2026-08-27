#include "assist/ComboMouseHit.h"

#include <QtTest>

using namespace gazer;

class ComboMouseTest final : public QObject {
    Q_OBJECT

private slots:
    void deadzoneAndDriftAndOutside();
    void slicesClockwiseFromTop();
    void packingEdgesAndCorners();
    void halfLeftKeepsFiveSlices();
    void halfLeftOffArcMisses();
    void cornerTlDoubleQuarterHits();
    void offScreenHoleAndRingStillHit();
    void overlayRectStaysOnScreen();
    void allCornersHaveFiveButtons();
    void yellowRingSnapsEightWays();
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
    auto pack = [&](QPointF o) { return ComboMouseHit::makeLayout(o, screen, 60, 120, pie); };

    const auto full = pack(QPointF(960, 540));
    QCOMPARE(full.arcStartDeg, 0.0);
    QCOMPARE(full.arcSpanDeg, 360.0);
    QCOMPARE(full.wedgeCount, ComboMouseHit::kSliceCount);

    const auto left = pack(QPointF(0, 540));
    QCOMPARE(left.arcStartDeg, 0.0);
    QCOMPARE(left.arcSpanDeg, 180.0);

    const auto right = pack(QPointF(1919, 540));
    QCOMPARE(right.arcStartDeg, 180.0);
    QCOMPARE(right.arcSpanDeg, 180.0);

    const auto top = pack(QPointF(960, 0));
    QCOMPARE(top.arcStartDeg, 90.0);
    QCOMPARE(top.arcSpanDeg, 180.0);

    const auto bottom = pack(QPointF(960, 1079));
    QCOMPARE(bottom.arcStartDeg, 270.0);
    QCOMPARE(bottom.arcSpanDeg, 180.0);

    const auto tl = pack(QPointF(0, 0));
    QCOMPARE(tl.arcStartDeg, 90.0);
    QCOMPARE(tl.arcSpanDeg, 90.0);

    const auto tr = pack(QPointF(1919, 0));
    QCOMPARE(tr.arcStartDeg, 180.0);
    QCOMPARE(tr.arcSpanDeg, 90.0);

    const auto br = pack(QPointF(1919, 1079));
    QCOMPARE(br.arcStartDeg, 270.0);
    QCOMPARE(br.arcSpanDeg, 90.0);

    const auto bl = pack(QPointF(0, 1079));
    QCOMPARE(bl.arcStartDeg, 0.0);
    QCOMPARE(bl.arcSpanDeg, 90.0);
}

void ComboMouseTest::halfLeftKeepsFiveSlices()
{
    const QRectF screen(0, 0, 1920, 1080);
    const QPointF o(0, 540);
    const auto L = ComboMouseHit::makeLayout(o, screen, 60, 120, 200);
    QCOMPARE(L.arcSpanDeg, 180.0);
    QCOMPARE(L.wedgeCount, ComboMouseHit::kSliceCount);
    QVERIFY(L.pieOuter > 200.0);

    auto at = [&](double x, double y) { return ComboMouseHit::hit(QPointF(x, y), o, L); };

    const auto up = at(20, 540 - 180);
    QCOMPARE(up.band, ComboMouseHit::Band::Slice);
    QCOMPARE(up.slice, ComboMouseHit::Slice::Right);

    const auto right = at(180, 540);
    QCOMPARE(right.band, ComboMouseHit::Band::Slice);
    QCOMPARE(right.slice, ComboMouseHit::Slice::Cancel);

    const auto down = at(20, 540 + 180);
    QCOMPARE(down.band, ComboMouseHit::Band::Slice);
    QCOMPARE(down.slice, ComboMouseHit::Slice::Left);

    QCOMPARE(at(o.x() + 10, o.y()).band, ComboMouseHit::Band::Deadzone);
    QCOMPARE(at(o.x() + 90, o.y()).band, ComboMouseHit::Band::Drift);
}

void ComboMouseTest::halfLeftOffArcMisses()
{
    const QRectF screen(0, 0, 1920, 1080);
    const QPointF o(0, 540);
    const auto L = ComboMouseHit::makeLayout(o, screen, 60, 120, 200);
    const auto miss = ComboMouseHit::hit(QPointF(-180, 540), o, L);
    QCOMPARE(miss.band, ComboMouseHit::Band::None);
}

void ComboMouseTest::cornerTlDoubleQuarterHits()
{
    const QRectF screen(0, 0, 1920, 1080);
    const QPointF o(0, 0);
    const auto L = ComboMouseHit::makeLayout(o, screen, 60, 120, 200);
    QCOMPARE(L.arcSpanDeg, ComboMouseHit::kCornerArcSpanDeg);
    QCOMPARE(L.wedgeCount, ComboMouseHit::kSliceCount);
    const auto* right = ComboMouseHit::wedgeById(L, ComboMouseHit::Slice::Right);
    const auto* move = ComboMouseHit::wedgeById(L, ComboMouseHit::Slice::Move);
    QVERIFY(right && move);
    QVERIFY(right->outer < move->outer);

    const double innerR = (right->inner + right->outer) * 0.5;
    const double outerR = (move->inner + move->outer) * 0.5;
    const double start = L.arcStartDeg;

    auto atDegR = [&](double cw, double r) {
        return ComboMouseHit::hit(ComboMouseHit::pointOnRay(o, cw, r), o, L);
    };

    QCOMPARE(atDegR(start + 22.5, innerR).slice, ComboMouseHit::Slice::Right);
    QCOMPARE(atDegR(start + 67.5, innerR).slice, ComboMouseHit::Slice::Left);
    QCOMPARE(atDegR(start + 15.0, outerR).slice, ComboMouseHit::Slice::Move);
    QCOMPARE(atDegR(start + 45.0, outerR).slice, ComboMouseHit::Slice::Cancel);
    QCOMPARE(atDegR(start + 75.0, outerR).slice, ComboMouseHit::Slice::Drag);
    QCOMPARE(atDegR(start + 22.5, innerR).band, ComboMouseHit::Band::Slice);

    QCOMPARE(ComboMouseHit::hit(o, o, L).band, ComboMouseHit::Band::Deadzone);
    QCOMPARE(ComboMouseHit::hit(QPointF(80, 80), o, L).band, ComboMouseHit::Band::Drift);
}

void ComboMouseTest::offScreenHoleAndRingStillHit()
{
    const QRectF screen(0, 0, 1920, 1080);
    const QPointF o(0, 0);
    const auto L = ComboMouseHit::makeLayout(o, screen, 60, 120, 200);
    QCOMPARE(ComboMouseHit::hit(QPointF(-20, -20), o, L).band, ComboMouseHit::Band::Deadzone);
    QCOMPARE(ComboMouseHit::hit(QPointF(-80, 0), o, L).band, ComboMouseHit::Band::Drift);
    const auto* right = ComboMouseHit::wedgeById(L, ComboMouseHit::Slice::Right);
    QVERIFY(right);
    const double innerR = (right->inner + right->outer) * 0.5;
    const auto slice =
        ComboMouseHit::hit(ComboMouseHit::pointOnRay(o, L.arcStartDeg + 22.5, innerR), o, L);
    QCOMPARE(slice.band, ComboMouseHit::Band::Slice);
    QCOMPARE(slice.slice, ComboMouseHit::Slice::Right);
}

void ComboMouseTest::overlayRectStaysOnScreen()
{
    const QRectF screen(0, 0, 1920, 1080);
    const QPointF o(0, 0);
    const auto L = ComboMouseHit::makeLayout(o, screen, 60, 120, 200);
    const QRect wr = ComboMouseHit::overlayRect(o, L, screen);
    QVERIFY(screen.contains(QRectF(wr)));
    QVERIFY(wr.contains(0, 0));
}

void ComboMouseTest::allCornersHaveFiveButtons()
{
    const QRectF screen(0, 0, 1920, 1080);
    const QPointF origins[] = {{0, 0}, {1919, 0}, {1919, 1079}, {0, 1079}};
    const double starts[] = {90.0, 180.0, 270.0, 0.0};
    for (int c = 0; c < 4; ++c) {
        const QPointF o = origins[c];
        const auto L = ComboMouseHit::makeLayout(o, screen, 60, 120, 200);
        QCOMPARE(L.arcStartDeg, starts[c]);
        QCOMPARE(L.arcSpanDeg, 90.0);
        QCOMPARE(L.wedgeCount, ComboMouseHit::kSliceCount);
        const auto* right = ComboMouseHit::wedgeById(L, ComboMouseHit::Slice::Right);
        const auto* move = ComboMouseHit::wedgeById(L, ComboMouseHit::Slice::Move);
        QVERIFY(right && move);
        const double innerR = (right->inner + right->outer) * 0.5;
        const double outerR = (move->inner + move->outer) * 0.5;
        const double start = L.arcStartDeg;
        const struct {
            double cw;
            double r;
            ComboMouseHit::Slice slice;
        } samples[] = {{start + 22.5, innerR, ComboMouseHit::Slice::Right},
                       {start + 67.5, innerR, ComboMouseHit::Slice::Left},
                       {start + 15.0, outerR, ComboMouseHit::Slice::Move},
                       {start + 45.0, outerR, ComboMouseHit::Slice::Cancel},
                       {start + 75.0, outerR, ComboMouseHit::Slice::Drag}};
        for (const auto& s : samples) {
            const auto h = ComboMouseHit::hit(ComboMouseHit::pointOnRay(o, s.cw, s.r), o, L);
            QCOMPARE(h.band, ComboMouseHit::Band::Slice);
            QCOMPARE(h.slice, s.slice);
        }
    }
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
