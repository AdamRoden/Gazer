#include "assist/VolumeBoard.h"
#include "layout/PageHit.h"
#include "layout/PageTypes.h"
#include "ui/SliderTrack.h"

#include <QtTest>

using namespace gazer;

namespace {

PageTarget box(const QString& id, const QRectF& r, bool interactive)
{
    PageTarget t;
    t.id = id;
    t.pageId = QStringLiteral("page");
    t.interactive = interactive;
    t.geom.visual = r;
    t.geom.dwellZone = r;
    t.geom.progressZone = r;
    return t;
}

PageTarget volumeSlider(const QString& id, const QRectF& r)
{
    PageTarget t = box(id, r, false);
    t.role = QStringLiteral("slider");
    t.caption = QStringLiteral("volume");
    return t;
}

} // namespace

class VolumeBoardTest final : public QObject {
    Q_OBJECT

private slots:
    void stampWritesSlidersOnly();
    void passiveSliderFollowsGaze();
    void frontKeyBlocksTheSlider();
    void coveringPageBlocksTheSlider();
};

void VolumeBoardTest::stampWritesSlidersOnly()
{
    PageDocument doc;
    PageGrid grid;
    PageCell slider;
    slider.role = QStringLiteral("Slider");
    slider.caption = QStringLiteral("Volume");
    slider.label = QStringLiteral("50%");
    PageCell key;
    key.label = QStringLiteral("a");
    grid.cells = {slider, key};
    PageGrid nested;
    PageCell nestedSlider;
    nestedSlider.role = QStringLiteral("slider");
    nestedSlider.caption = QStringLiteral("volume");
    nestedSlider.label = QStringLiteral("1%");
    nested.cells = {nestedSlider};
    grid.subGrids = {nested};
    doc.grids = {grid};
    PageZone zone;
    zone.role = QStringLiteral("slider");
    zone.caption = QStringLiteral("volume");
    zone.label = QStringLiteral("0%");
    doc.zones = {zone};

    VolumeBoard::stamp(doc, 37);

    QCOMPARE(doc.grids[0].cells[0].label, QStringLiteral("37%"));
    QCOMPARE(doc.grids[0].cells[1].label, QStringLiteral("a"));
    QCOMPARE(doc.grids[0].subGrids[0].cells[0].label, QStringLiteral("37%"));
    QCOMPARE(doc.zones[0].label, QStringLiteral("37%"));
}

void VolumeBoardTest::passiveSliderFollowsGaze()
{
    const QRectF bar(200, 0, 100, 40);
    const QPointF gaze(250, 20);
    const QVector<PageTarget> targets{box(QStringLiteral("key"), QRectF(0, 0, 100, 40), true),
                                      volumeSlider(QStringLiteral("vol"), bar)};

    QVERIFY(PageHit::at(targets, gaze) == nullptr);
    const std::optional<int> pct = VolumeBoard::percentAt(targets, {}, 1.0, gaze);
    QVERIFY(pct.has_value());
    QCOMPARE(*pct, qRound(SliderTrack::volumeFractionAtX(bar, gaze.x()) * 100.0));
    QVERIFY(!VolumeBoard::percentAt(targets, {}, 1.0, QPointF(50, 20)).has_value());
}

void VolumeBoardTest::frontKeyBlocksTheSlider()
{
    const QRectF bar(0, 0, 100, 40);
    PageTarget key = box(QStringLiteral("key"), bar, true);
    const QVector<PageTarget> targets{volumeSlider(QStringLiteral("vol"), bar), key};
    const QPointF gaze(50, 20);

    QCOMPARE(PageHit::at(targets, gaze)->id, QStringLiteral("key"));
    QVERIFY(!VolumeBoard::percentAt(targets, {}, 1.0, gaze).has_value());
}

void VolumeBoardTest::coveringPageBlocksTheSlider()
{
    PageTarget back = volumeSlider(QStringLiteral("vol"), QRectF(0, 0, 100, 40));
    back.pageId = QStringLiteral("kb");
    PageGridPaint older;
    older.pageId = QStringLiteral("kb");
    older.visual = QRectF(0, 0, 100, 40);
    PageGridPaint newer;
    newer.pageId = QStringLiteral("settings");
    newer.visual = QRectF(0, 0, 100, 40);

    const QVector<PageTarget> targets{back};
    const QVector<PageGridPaint> grids{older, newer};
    QVERIFY(!VolumeBoard::percentAt(targets, grids, 1.0, QPointF(80, 20)).has_value());
    QVERIFY(VolumeBoard::percentAt(targets, {older}, 1.0, QPointF(80, 20)).has_value());
}

QObject* createVolumeBoardTest()
{
    return new VolumeBoardTest;
}

#include "VolumeBoardTest.moc"
