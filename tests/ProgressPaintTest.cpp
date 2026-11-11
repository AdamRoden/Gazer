#include "ui/ProgressPaint.h"
#include "ui/Theme.h"

#include <QtTest>

#include <cmath>

using namespace gazer;

class ProgressPaintTest final : public QObject {
    Q_OBJECT

private slots:
    void radialRingCell();
    void radialRingMid();
    void radialRingPointer();
    void strokeMatchesPieAlpha();
    void fillFollowsRoundedOuterPath();
    void progressShiftsTowardForeground();
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

void ProgressPaintTest::progressShiftsTowardForeground()
{
    const QColor canvas(0x0A, 0x0A, 0x0A);
    const QColor progress(0xFF, 0x47, 0x3D, 0x99);
    const QColor lightInk(0xE3, 0xE3, 0xE3);
    const QColor darkInk(0x1A, 0x1A, 0x1A);
    const double from = ThemeColors::relativeLuminance(progress);

    const QColor darkFill(0x0A, 0x0A, 0x0A);
    const QColor lightFill(216, 216, 216);
    const QColor quitYes(0x9B, 0x22, 0x26);
    const QColor blue(0x2B, 0x7F, 0xFF);
    QVERIFY(ThemeColors::hslDistance(progress, darkFill) >= ThemeColors::kProgressHslDistance);
    QVERIFY(ThemeColors::hslDistance(progress, lightFill) >= ThemeColors::kProgressHslDistance);
    QVERIFY(ThemeColors::hslDistance(progress, quitYes) >= ThemeColors::kProgressHslDistance);
    QVERIFY(ThemeColors::contrastRatio(progress, blue) < 2.0);
    QVERIFY(ThemeColors::hslDistance(progress, blue) >= ThemeColors::kProgressHslDistance);
    QCOMPARE(ThemeColors::progressOnFill(darkFill, progress, lightInk, canvas), progress);
    QCOMPARE(ThemeColors::progressOnFill(lightFill, progress, darkInk, canvas), progress);
    QCOMPARE(ThemeColors::progressOnFill(quitYes, progress, lightInk, canvas), progress);
    QCOMPARE(ThemeColors::progressOnFill(blue, progress, lightInk, canvas), progress);

    auto shiftedLum = [&](const QColor& fill, const QColor& ink) {
        const double distance = ThemeColors::hslDistance(progress, fill);
        const double closeness = 1.0 - distance / ThemeColors::kProgressHslDistance;
        const double toward = ThemeColors::relativeLuminance(ink);
        return from + ThemeColors::kProgressLuminanceShift * closeness * (toward - from);
    };

    const QColor closeDark(0xC1, 0x00, 0x07);
    QVERIFY(ThemeColors::hslDistance(progress, closeDark) < ThemeColors::kProgressHslDistance);
    const QColor lighter = ThemeColors::progressOnFill(closeDark, progress, lightInk, canvas);
    QCOMPARE(lighter.alpha(), progress.alpha());
    const int lightHue = qAbs(lighter.hslHue() - progress.hslHue());
    QVERIFY(lightHue <= 8 || lightHue >= 352);
    const double lightGot = ThemeColors::relativeLuminance(lighter);
    QVERIFY(std::abs(lightGot - shiftedLum(closeDark, lightInk)) < 0.03);
    QVERIFY(lightGot > from);

    ProgressVisuals vis;
    vis.progressColor = progress;
    vis.fillColor = progress;
    const ProgressVisuals drawn = vis.onFill(closeDark, lightInk, canvas);
    QCOMPARE(drawn.progressColor, lighter);
    QCOMPARE(drawn.fillColor, lighter);

    const QColor closeLight(0xFF, 0x64, 0x67);
    QVERIFY(ThemeColors::hslDistance(progress, closeLight)
            < ThemeColors::hslDistance(progress, closeDark));
    const QColor darker = ThemeColors::progressOnFill(closeLight, progress, darkInk, canvas);
    QCOMPARE(darker.alpha(), progress.alpha());
    const int darkHue = qAbs(darker.hslHue() - progress.hslHue());
    QVERIFY(darkHue <= 8 || darkHue >= 352);
    const double darkGot = ThemeColors::relativeLuminance(darker);
    QVERIFY(std::abs(darkGot - shiftedLum(closeLight, darkInk)) < 0.03);
    QVERIFY(darkGot < from);

    QVERIFY(!ThemeColors::progressOnFill(quitYes, QColor(), lightInk, canvas).isValid());
}

QObject* createProgressPaintTest()
{
    return new ProgressPaintTest;
}

#include "ProgressPaintTest.moc"
