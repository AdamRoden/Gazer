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
    t.dwell.scanGrace = 0;
    return t;
}

std::optional<VolumeBoard::Gaze> look(const QVector<PageTarget>& targets,
                                      const QVector<PageGridPaint>& grids, const QPointF& gaze,
                                      qint64 nowMs, int fallbackGraceMs, VolumeBoard::Arm* arm)
{
    return VolumeBoard::at(targets, grids, 1.0, gaze, nowMs, fallbackGraceMs, arm);
}

} // namespace

class VolumeBoardTest final : public QObject {
    Q_OBJECT

private slots:
    void stampWritesSlidersOnly();
    void sliderFollowsGaze();
    void scanGraceArmsThenFollows();
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

void VolumeBoardTest::sliderFollowsGaze()
{
    const QRectF bar(200, 0, 100, 40);
    const QPointF gaze(250, 20);
    const QVector<PageTarget> targets{box(QStringLiteral("key"), QRectF(0, 0, 100, 40), true),
                                      volumeSlider(QStringLiteral("vol"), bar)};
    VolumeBoard::Arm arm;

    QVERIFY(PageHit::at(targets, gaze) == nullptr);
    const std::optional<VolumeBoard::Gaze> on = look(targets, {}, gaze, 0, 100, &arm);
    QVERIFY(on && on->percent);
    QCOMPARE(*on->percent, qRound(SliderTrack::volumeFractionAtX(bar, gaze.x()) * 100.0));
    QVERIFY(!look(targets, {}, QPointF(50, 20), 10, 100, &arm));
}

void VolumeBoardTest::scanGraceArmsThenFollows()
{
    const QRectF bar(0, 0, 100, 40);
    const QPointF gaze(80, 20);
    PageTarget vol = volumeSlider(QStringLiteral("vol"), bar);
    vol.dwell.scanGrace = 2000;
    const int level = qRound(SliderTrack::volumeFractionAtX(bar, gaze.x()) * 100.0);
    VolumeBoard::Arm arm;

    const std::optional<VolumeBoard::Gaze> start = look({vol}, {}, gaze, 0, 100, &arm);
    QVERIFY(start && !start->percent);
    QCOMPARE(start->arm, 0.0);

    const std::optional<VolumeBoard::Gaze> mid = look({vol}, {}, gaze, 1000, 100, &arm);
    QVERIFY(mid && !mid->percent);
    QVERIFY(mid->arm > 0.45);
    QVERIFY(mid->arm < 0.55);

    const std::optional<VolumeBoard::Gaze> armed = look({vol}, {}, gaze, 2000, 100, &arm);
    QVERIFY(armed && armed->percent);
    QCOMPARE(*armed->percent, level);
    QCOMPARE(armed->arm, 1.0);

    QVERIFY(!look({vol}, {}, QPointF(-10, -10), 2500, 100, &arm));
    const std::optional<VolumeBoard::Gaze> again = look({vol}, {}, gaze, 2500, 100, &arm);
    QVERIFY(again && !again->percent);

    PageTarget bare = volumeSlider(QStringLiteral("vol"), bar);
    bare.dwell.scanGrace.reset();
    VolumeBoard::Arm fallback;
    const std::optional<VolumeBoard::Gaze> waiting = look({bare}, {}, gaze, 0, 500, &fallback);
    QVERIFY(waiting && !waiting->percent);
    const std::optional<VolumeBoard::Gaze> fell = look({bare}, {}, gaze, 500, 500, &fallback);
    QVERIFY(fell && fell->percent);
    QCOMPARE(*fell->percent, level);
}

void VolumeBoardTest::frontKeyBlocksTheSlider()
{
    const QRectF bar(0, 0, 100, 40);
    PageTarget key = box(QStringLiteral("key"), bar, true);
    const QVector<PageTarget> targets{volumeSlider(QStringLiteral("vol"), bar), key};
    const QPointF gaze(50, 20);

    QCOMPARE(PageHit::at(targets, gaze)->id, QStringLiteral("key"));
    VolumeBoard::Arm arm;
    QVERIFY(!look(targets, {}, gaze, 0, 0, &arm));
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
    VolumeBoard::Arm blocked;
    QVERIFY(!look(targets, grids, QPointF(80, 20), 0, 0, &blocked));
    VolumeBoard::Arm open;
    const std::optional<VolumeBoard::Gaze> shown = look(targets, {older}, QPointF(80, 20), 0, 0, &open);
    QVERIFY(shown && shown->percent);
}

QObject* createVolumeBoardTest()
{
    return new VolumeBoardTest;
}

#include "VolumeBoardTest.moc"
