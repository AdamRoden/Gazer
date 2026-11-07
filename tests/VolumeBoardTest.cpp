#include "assist/GazeTrack.h"
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

/// A miss leaves the clock so the shell can release that scrub id. This helper
/// clears it afterwards, which is what the shell does once the release returns.
std::optional<VolumeBoard::Gaze> look(const QVector<PageTarget>& targets,
                                      const QVector<PageGridPaint>& grids, const QPointF& gaze,
                                      qint64 nowMs, GazeTrack::Lock* arm)
{
    const std::optional<VolumeBoard::Gaze> hit =
        VolumeBoard::at(targets, grids, 1.0, gaze, nowMs, arm);
    if (!hit && arm) {
        arm->clear();
    }
    return hit;
}

} // namespace

class VolumeBoardTest final : public QObject {
    Q_OBJECT

private slots:
    void stampWritesSlidersOnly();
    void sliderFollowsGaze();
    void scanGraceArmsThenFollows();
    void dragUsesSliderRate();
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
    GazeTrack::Lock arm;

    QVERIFY(PageHit::at(targets, gaze) == nullptr);
    const std::optional<VolumeBoard::Gaze> on = look(targets, {}, gaze, 0, &arm);
    QVERIFY(on && on->ready);
    QCOMPARE(on->gazePx, gaze.x());
    const SliderTrack::ValueAxis axis = SliderTrack::volumeValueAxis(bar);
    QCOMPARE(on->track0, axis.from);
    QCOMPARE(on->track1, axis.to);
    QVERIFY(!look(targets, {}, QPointF(50, 20), 10, &arm));
}

void VolumeBoardTest::scanGraceArmsThenFollows()
{
    const QRectF bar(0, 0, 100, 40);
    const QPointF gaze(80, 20);
    PageTarget vol = volumeSlider(QStringLiteral("vol"), bar);
    vol.dwell.scanGrace = 2000;
    GazeTrack::Lock arm;

    const std::optional<VolumeBoard::Gaze> start = look({vol}, {}, gaze, 0, &arm);
    QVERIFY(start && !start->ready);
    QCOMPARE(start->arm, 0.0);

    const std::optional<VolumeBoard::Gaze> mid = look({vol}, {}, gaze, 1000, &arm);
    QVERIFY(mid && !mid->ready);
    QVERIFY(mid->arm > 0.45);
    QVERIFY(mid->arm < 0.55);

    const std::optional<VolumeBoard::Gaze> armed = look({vol}, {}, gaze, 2000, &arm);
    QVERIFY(armed && armed->ready);
    QCOMPARE(armed->arm, 1.0);

    QVERIFY(!look({vol}, {}, QPointF(-10, -10), 2500, &arm));
    const std::optional<VolumeBoard::Gaze> again = look({vol}, {}, gaze, 2500, &arm);
    QVERIFY(again && !again->ready);
    QCOMPARE(again->arm, 0.0);

    PageTarget bare = volumeSlider(QStringLiteral("vol"), bar);
    bare.dwell.scanGrace.reset();
    GazeTrack::Lock fallback;
    const std::optional<VolumeBoard::Gaze> waiting = look({bare}, {}, gaze, 0, &fallback);
    QVERIFY(waiting && !waiting->ready);
    QCOMPARE(waiting->arm, 0.0);
    const std::optional<VolumeBoard::Gaze> halfway =
        look({bare}, {}, gaze, kTrackLockMs / 2, &fallback);
    QVERIFY(halfway && !halfway->ready);
    QVERIFY(halfway->arm > 0.45);
    QVERIFY(halfway->arm < 0.55);
    const std::optional<VolumeBoard::Gaze> fell = look({bare}, {}, gaze, kTrackLockMs, &fallback);
    QVERIFY(fell && fell->ready);
    QCOMPARE(fell->arm, 1.0);
}

/// Thumb position after @p frames of 16 ms. The caller feeds the last position back in,
/// the same way the shell quantizes a slider between samples.
double dragThumb(double thumb, double gaze, int frames)
{
    GazeTrack::Drag drag;
    double pos = thumb;
    for (int i = 0; i < frames; ++i) {
        pos = GazeTrack::moveThumb(drag, gaze, pos, 0.0, 400.0, qint64(i) * 16);
    }
    return pos;
}

void VolumeBoardTest::dragUsesSliderRate()
{
    // Inside the 40 px rest band around the thumb, it stays put.
    QCOMPARE(dragThumb(100.0, 140.0, 30), 100.0);

    // A look on either end is full speed from anywhere along the track, and it arrives.
    const double fromLow = dragThumb(100.0, 400.0, 8);
    const double fromHigh = dragThumb(300.0, 400.0, 8);
    QVERIFY(fromLow - 100.0 > 25.0);
    QVERIFY(fromHigh - 300.0 > 25.0);
    QVERIFY(qAbs((fromLow - 100.0) - (fromHigh - 300.0)) < 1.0);
    QVERIFY(dragThumb(390.0, 400.0, 8) > 399.0);
    QVERIFY(dragThumb(100.0, 400.0, 80) > 399.0);

    // A look partway from the thumb toward the far end is slower than a look on that end.
    const double partway = dragThumb(0.0, 200.0, 8);
    const double atEnd = dragThumb(0.0, 400.0, 8);
    QVERIFY(partway > 5.0);
    QVERIFY(partway < atEnd * 0.6);

    const double down = dragThumb(400.0, 0.0, 8);
    QVERIFY(400.0 - down > 25.0);
}

void VolumeBoardTest::frontKeyBlocksTheSlider()
{
    const QRectF bar(0, 0, 100, 40);
    PageTarget key = box(QStringLiteral("key"), bar, true);
    const QVector<PageTarget> targets{volumeSlider(QStringLiteral("vol"), bar), key};
    const QPointF gaze(50, 20);

    QCOMPARE(PageHit::at(targets, gaze)->id, QStringLiteral("key"));
    GazeTrack::Lock arm;
    QVERIFY(!look(targets, {}, gaze, 0, &arm));
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
    GazeTrack::Lock blocked;
    QVERIFY(!look(targets, grids, QPointF(80, 20), 0, &blocked));
    GazeTrack::Lock open;
    const std::optional<VolumeBoard::Gaze> shown = look(targets, {older}, QPointF(80, 20), 0, &open);
    QVERIFY(shown && shown->ready);
}

QObject* createVolumeBoardTest()
{
    return new VolumeBoardTest;
}

#include "VolumeBoardTest.moc"
