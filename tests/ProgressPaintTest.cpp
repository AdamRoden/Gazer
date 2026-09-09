#include "ui/ProgressPaint.h"
#include "ui/Theme.h"

#include <QtTest>

using namespace gazer;

class ProgressPaintTest final : public QObject {
    Q_OBJECT

private slots:
    void radialRingCell();
    void radialRingMid();
    void radialRingPointer();
    void strokeMatchesPieAlpha();
    void fillFollowsRoundedOuterPath();
};

void ProgressPaintTest::radialRingCell()
{
    const RadialRing ring = radialRingGeom(QRectF(0, 0, 200, 200), ProgressShape::RoundedRect);
    QCOMPARE(ring.stroke, 10.0);
    QCOMPARE(ring.disk.width(), 180.0);
    QCOMPARE(ring.arc.width(), 170.0);
}

void ProgressPaintTest::radialRingMid()
{
    const RadialRing ring = radialRingGeom(QRectF(0, 0, 100, 100), ProgressShape::RoundedRect);
    QCOMPARE(ring.stroke, 10.0);
    QCOMPARE(ring.disk.width(), 88.0);
    QCOMPARE(ring.arc.width(), 78.0);
}

void ProgressPaintTest::radialRingPointer()
{
    const RadialRing ring = radialRingGeom(QRectF(0, 0, 64, 64), ProgressShape::Ellipse);
    QCOMPARE(ring.stroke, 6.4);
    QCOMPARE(ring.disk.width(), 64.0);
    QCOMPARE(ring.arc.width(), 57.6);
}

void ProgressPaintTest::strokeMatchesPieAlpha()
{
    const QColor pie(0xFF, 0x47, 0x3D, kProgressFillAlpha);
    QCOMPARE(progressStrokeColor(QColor(0xFF, 0x47, 0x3D), pie).alpha(), kProgressFillAlpha);
    const QColor track = radialTrackColor(pie);
    QCOMPARE(track.alpha(), qRound(kProgressFillAlpha * 80.0 / 255.0));
}

void ProgressPaintTest::fillFollowsRoundedOuterPath()
{
    const QRectF r(0, 0, 100, 100);
    const QPainterPath outer = roundedBoxPath(r, PageBox::all(40.0));
    const QPainterPath fill = progressFillPath(outer, r, 0.5, ProgressFillDir::Up);
    QVERIFY(fill.contains(QPointF(50, 90)));
    QVERIFY(!fill.contains(QPointF(2, 98)));
    QVERIFY(!fill.contains(QPointF(50, 10)));
    const QPainterPath center = progressFillPath(outer, r, 0.5, ProgressFillDir::Center);
    QVERIFY(center.contains(QPointF(50, 50)));
    QVERIFY(!center.contains(QPointF(2, 50)));
}

QObject* createProgressPaintTest()
{
    return new ProgressPaintTest;
}

#include "ProgressPaintTest.moc"
