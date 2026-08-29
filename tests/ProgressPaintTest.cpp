#include "ui/ProgressPaint.h"

#include <QtTest>

using namespace gazer;

class ProgressPaintTest final : public QObject {
    Q_OBJECT

private slots:
    void radialRingCell();
    void radialRingMid();
    void radialRingPointer();
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

QObject* createProgressPaintTest()
{
    return new ProgressPaintTest;
}

#include "ProgressPaintTest.moc"
