#include "layout/PageDetector.h"
#include "layout/PageDim.h"
#include "layout/PageHit.h"
#include "layout/PageLoader.h"
#include "layout/RoundBox.h"

#include <QSet>
#include <QVariantMap>
#include <QtTest>

using namespace gazer;

namespace {

const PageTarget* targetById(const QVector<PageTarget>& targets, const QString& id)
{
    for (const PageTarget& t : targets) {
        if (t.id == id) {
            return &t;
        }
    }
    return nullptr;
}

} // namespace

class PageHitTest final : public QObject {
    Q_OBJECT

private slots:
    void roundedBoxFitsSemicircle();
    void roundedBoxHitIgnoresSquareCorners();
    void cellDetectorClipsProgress();
    void zoneDetectorUnrestrictedDwell();
    void zoneProgressCoercedWhenOffScreen();
    void collectEmitsGridChrome();
    void screenExpressionSizes16by9();
    void cellsInheritPageNotGrid();
    void cellRectSpan();
    void cellIndexAtMatchesCellRect();
    void cellRectRowWeights();
    void drawerMapIsIdentityAtFullScale();
    void drawerMapShrinksAboutBottom();
    void drawerScaleMustNotMoveOtherChrome();
    void frostedBoundsUsesRestPose();
    void reservedBoundsKeepsHiddenGrids();
    void desktopModeUsesDesktop();
    void masterZonesBeatOpenGrids();
    void masterGridCoversOpenTargets();
    void frontPageGridOccludesBackPage();
    void visibleCellRemainderHits();
    void engagedHullDoesNotPierceCover();
};

void PageHitTest::roundedBoxFitsSemicircle()
{
    const QRectF r(0, 0, 200, 100);
    const PageBox fit = fitCornerRadii(r, PageBox::of(999, 999, 0, 0));
    QCOMPARE(fit.at(0), 100.0);
    QCOMPARE(fit.at(1), 100.0);
    QCOMPARE(fit.at(2), 0.0);
    QCOMPARE(fit.at(3), 0.0);
    const PageBox pill = fitCornerRadii(r, PageBox::all(999));
    QCOMPARE(pill.at(0), 50.0);
    QCOMPARE(pill.at(1), 50.0);
    QCOMPARE(pill.at(2), 50.0);
    QCOMPARE(pill.at(3), 50.0);
}

void PageHitTest::roundedBoxHitIgnoresSquareCorners()
{
    const QRectF r(0, 0, 200, 100);
    const PageBox dome = PageBox::of(100, 100, 0, 0);
    QVERIFY(roundedBoxContains(r, dome, QPointF(100, 20)));
    QVERIFY(!roundedBoxContains(r, dome, QPointF(2, 2)));
    QVERIFY(roundedBoxContains(r, dome, QPointF(10, 90)));
    PageTarget t;
    t.interactive = true;
    t.chrome.radius = dome;
    t.geom.dwellZone = r;
    t.geom.progressZone = r;
    t.geom.visual = r;
    const QVector<PageTarget> targets{t};
    QVERIFY(PageHit::at(targets, QPointF(2, 2)) != nullptr);
    QVERIFY(PageHit::atProgress(targets, QPointF(2, 2)) == nullptr);
    QVERIFY(PageHit::atProgress(targets, QPointF(100, 20)) != nullptr);
}

void PageHitTest::cellDetectorClipsProgress()
{
    const QRectF screen(0, 0, 1920, 1080);
    const QRectF cell(100, 100, 80, 80);
    const PageDetectorGeom g = PageDetector::cell(cell, screen);
    QCOMPARE(g.dwellZone, cell);
    QCOMPARE(g.visual, cell);
    QCOMPARE(g.progressZone, cell);
    QVERIFY(!g.progressCoerced);
    QCOMPARE(g.contentOnScreen(), cell);

    const QRectF hanging(1900, 100, 80, 80);
    const PageDetectorGeom h = PageDetector::cell(hanging, screen);
    QCOMPARE(h.dwellZone, hanging);
    QCOMPARE(h.progressZone, QRectF(1900, 100, 20, 80));
    QVERIFY(!h.progressCoerced);
}

void PageHitTest::zoneDetectorUnrestrictedDwell()
{
    const QRectF screen(0, 0, 1920, 1080);
    const QRectF bounds = screen;
    PageZone z;
    z.anchor = PageAnchor::Bottom;
    z.offset.x = PageDim::pixels(0);
    z.offset.y = PageDim::pixels(0);
    z.size.x = PageDim::pixels(300);
    z.size.y = PageDim::pixels(150);
    z.dwellOffset.x = PageDim::pixels(0);
    z.dwellOffset.y = PageDim::pixels(240);
    z.dwellSize.x = PageDim::pixels(300);
    z.dwellSize.y = PageDim::pixels(200);
    const PageDetectorGeom g = PageDetector::zoneFromDef(z, bounds, screen);
    QVERIFY(g.progressZone.intersects(screen));
    QVERIFY(screen.contains(g.progressZone));
    QVERIFY(!screen.contains(g.dwellZone.center()));
    QVERIFY(g.dwellZone.bottom() > screen.bottom());
    QVERIFY(g.dwellZone.contains(QPointF(960, 1080 + 200)));
    // Dwell Bottom-anchor is offset from the progress box's bottom-center, not
    // from the progress top-left.
    QCOMPARE(g.visual.top(), 930.0);
    QCOMPARE(g.visual.bottom(), 1080.0);
    QCOMPARE(g.dwellZone.left(), 810.0);
    QCOMPARE(g.dwellZone.top(), 1120.0);
    QCOMPARE(g.dwellZone.width(), 300.0);
    QCOMPARE(g.dwellZone.height(), 200.0);
}

void PageHitTest::zoneProgressCoercedWhenOffScreen()
{
    const QRectF screen(0, 0, 1920, 1080);
    const QRectF visual(810, 1200, 300, 150);
    const QRectF dwell(810, 1300, 300, 200);
    const PageDetectorGeom g = PageDetector::zone(visual, dwell, screen);
    QVERIFY(!g.progressCoerced);
    QCOMPARE(g.progressZone, visual);
    QCOMPARE(g.dwellZone, dwell);
    QVERIFY(!screen.contains(g.visual.center()));
}

void PageHitTest::collectEmitsGridChrome()
{
    PageDocument doc;
    QString err;
    const QString path = QStringLiteral(GAZER_SOURCE_DIR)
                         + QStringLiteral("/resources/layouts/example_mouse.xml");
    QVERIFY2(PageLoader::loadFromFile(path, doc, &err), qPrintable(err));
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    QVector<PageGridPaint> grids;
    const QVector<PageTarget> t = PageHit::collect(doc, frame, {}, {}, false, &grids);
    QCOMPARE(grids.size(), 1);
    QVERIFY(!grids[0].visual.isEmpty());
    QVERIFY(!grids[0].chrome.background.has_value());
    QVERIFY(!t.isEmpty());
    const PageTarget* cell = PageHit::at(t, grids[0].visual.center());
    QVERIFY(cell);
    QCOMPARE(cell->kind, PageTarget::Kind::Cell);
}

void PageHitTest::screenExpressionSizes16by9()
{
    PageDocument doc;
    QString err;
    const QByteArray xml = R"xml(
<Page id="p">
  <Grid id="g" desktopMode="false" anchor="Center" size="A_ScreenHeight/9*16, A_ScreenHeight">
    <Cell id="c" label="X"/>
  </Grid>
</Page>
)xml";
    QVERIFY2(PageLoader::loadFromXml(xml, doc, &err), qPrintable(err));
    PageFrame frame;
    frame.screen = QRectF(0, 0, 2560, 1080);
    frame.desktop = QRectF(0, 0, 3840, 1080);
    QVector<PageGridPaint> grids;
    const QVector<PageTarget> t = PageHit::collect(doc, frame, {}, {}, false, &grids);
    QCOMPARE(grids.size(), 1);
    QCOMPARE(grids[0].visual.width(), 1920.0);
    QCOMPARE(grids[0].visual.height(), 1080.0);
    QCOMPARE(grids[0].visual.left(), 320.0);
    QVERIFY(!t.isEmpty());
}

void PageHitTest::cellsInheritPageNotGrid()
{
    PageDocument doc;
    QString err;
    const QByteArray xml = R"xml(
<Page id="p" background="#FF0000" radius="4">
  <Grid id="g" background="#00FF00" radius="20" size="200,100">
    <Cell id="c" label="X"/>
  </Grid>
</Page>
)xml";
    QVERIFY2(PageLoader::loadFromXml(xml, doc, &err), qPrintable(err));
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    QVector<PageGridPaint> grids;
    const QVector<PageTarget> t = PageHit::collect(doc, frame, {}, {}, false, &grids);
    QCOMPARE(grids.size(), 1);
    QCOMPARE(grids[0].chrome.background->rgb(), QColor(QStringLiteral("#00FF00")).rgb());
    QCOMPARE(grids[0].chrome.radius ? grids[0].chrome.radius->first() : -1.0, 20.0);
    QVERIFY(!t.isEmpty());
    QCOMPARE(t[0].chrome.background->rgb(), QColor(QStringLiteral("#FF0000")).rgb());
    QCOMPARE(t[0].chrome.radius ? t[0].chrome.radius->first() : -1.0, 4.0);
}

void PageHitTest::cellRectSpan()
{
    PageGrid g;
    g.rows = 2;
    g.columns = 2;
    g.gapPx = 0;
    g.marginPx = 0;
    const QRectF a = PageHit::cellRect(g, QRectF(0, 0, 200, 200), 0, 0, 1, 2);
    QCOMPARE(a, QRectF(0, 0, 200, 100));
}

void PageHitTest::cellIndexAtMatchesCellRect()
{
    PageGrid g;
    g.rows = 2;
    g.columns = 4;
    g.gapPx = 8;
    g.marginPx = 12;
    const QRectF board(100, 50, 400, 200);
    const QRectF cell = PageHit::cellRect(g, board, 1, 2, 1, 1);
    const QPoint idx = PageHit::cellIndexAt(g, board, cell.center());
    QCOMPARE(idx.x(), 2);
    QCOMPARE(idx.y(), 1);
    QVERIFY(PageHit::cellIndexAt(g, board, QPointF(0, 0)) == QPoint(-1, -1));
}

void PageHitTest::cellRectRowWeights()
{
    PageGrid g;
    g.rows = 3;
    g.columns = 1;
    g.gapPx = 10;
    g.marginPx = 0;
    g.rowWeights = {1.0, 2.0, 2.0};
    const QRectF board(0, 0, 100, 110);
    const QRectF header = PageHit::cellRect(g, board, 0, 0, 1, 1);
    const QRectF body = PageHit::cellRect(g, board, 1, 0, 1, 1);
    QCOMPARE(header.height(), 18.0);
    QCOMPARE(body.height(), 36.0);
    QCOMPARE(PageHit::cellIndexAt(g, board, header.center()).y(), 0);
    QCOMPARE(PageHit::cellIndexAt(g, board, body.center()).y(), 1);
}

void PageHitTest::drawerMapIsIdentityAtFullScale()
{
    PageTarget t;
    t.drawerMotion = true;
    t.geom.visual = QRectF(0, 900, 200, 100);
    t.geom.dwellZone = t.geom.visual;
    t.geom.progressZone = t.geom.visual;
    const QVector<PageTarget> targets{t};
    const QTransform xf = PageHit::drawerTransform(targets, 1.0);
    QCOMPARE(PageHit::mapDrawer(t, t.geom.dwellZone, xf, 1.0), t.geom.dwellZone);
}

void PageHitTest::drawerMapShrinksAboutBottom()
{
    PageTarget t;
    t.interactive = true;
    t.drawerMotion = true;
    t.geom.visual = QRectF(0, 900, 200, 100);
    t.geom.dwellZone = t.geom.visual;
    t.geom.progressZone = t.geom.visual;
    const QVector<PageTarget> targets{t};
    const QTransform xf = PageHit::drawerTransform(targets, 0.5);
    const QRectF mapped = PageHit::mapDrawer(t, t.geom.dwellZone, xf, 0.5);
    QVERIFY(mapped.height() < t.geom.dwellZone.height());
    QVERIFY(qAbs(mapped.bottom() - t.geom.dwellZone.bottom()) < 0.01);
    QVERIFY(PageHit::at(targets, t.geom.dwellZone.topLeft(), 0.5) == nullptr);
    QVERIFY(PageHit::at(targets, mapped.center(), 0.5) != nullptr);
}

void PageHitTest::drawerScaleMustNotMoveOtherChrome()
{
    PageGridPaint drawer;
    drawer.visual = QRectF(360, 930, 1200, 150);
    drawer.drawerMotion = true;
    PageGridPaint strip;
    strip.visual = QRectF(2000, 0, 100, 754);
    strip.drawerMotion = false;
    const QVector<PageGridPaint> grids{drawer, strip};
    const QRectF full = PageHit::paintBounds({}, grids, 1.0);
    const QRectF scaled = PageHit::paintBounds({}, grids, 0.5);
    QCOMPARE(full, PageHit::hostBounds({}, grids));
    QCOMPARE(full.left(), 360.0);
    QCOMPARE(full.right(), 2100.0);
    QCOMPARE(scaled.right(), 2100.0);
    QVERIFY(scaled.left() > full.left());
    QCOMPARE(PageHit::mapDrawer(false, strip.visual, PageHit::drawerTransform({}, 0.5, grids), 0.5),
             strip.visual);
}

void PageHitTest::frostedBoundsUsesRestPose()
{
    PageGridPaint drawer;
    drawer.visual = QRectF(360, 930, 1200, 150);
    drawer.drawerMotion = true;
    drawer.chrome.blur = 25.0;
    PageGridPaint strip;
    strip.visual = QRectF(2000, 0, 100, 754);
    strip.drawerMotion = false;
    strip.chrome.blur = 15.0;
    PageTarget cell;
    cell.drawerMotion = true;
    cell.chrome.blur = 25.0;
    cell.geom.visual = QRectF(370, 940, 80, 130);
    cell.geom.dwellZone = cell.geom.visual;
    cell.geom.progressZone = cell.geom.visual;
    PageGridPaint opaque;
    opaque.visual = QRectF(0, 0, 80, 80);
    const QVector<PageGridPaint> grids{drawer, strip, opaque};
    const QVector<PageTarget> targets{cell};
    const QRectF frost = PageHit::frostedBounds(targets, grids);
    QCOMPARE(frost, drawer.visual.united(strip.visual));
    QVERIFY(frost.contains(cell.geom.visual));
    QCOMPARE(PageHit::frostedBounds({}, {opaque}), QRectF());
    QVERIFY(PageHit::paintBounds({cell}, {drawer}, 0.5).left() > drawer.visual.left());
}

void PageHitTest::reservedBoundsKeepsHiddenGrids()
{
    PageDocument doc;
    doc.id = QStringLiteral("p");
    PageGrid strip;
    strip.id = QStringLiteral("vert1");
    strip.anchor = PageAnchor::TopLeft;
    strip.offset.x = PageDim::pixels(2000);
    strip.offset.y = PageDim::pixels(0);
    strip.size.x = PageDim::pixels(100);
    strip.size.y = PageDim::pixels(754);
    PageGrid board;
    board.id = QStringLiteral("board");
    board.anchor = PageAnchor::TopLeft;
    board.offset.x = PageDim::pixels(500);
    board.offset.y = PageDim::pixels(754);
    board.size.x = PageDim::pixels(1600);
    board.size.y = PageDim::pixels(280);
    board.show = false;
    doc.grids.push_back(strip);
    doc.grids.push_back(board);
    PageFrame frame;
    frame.screen = QRectF(0, 0, 2560, 1080);
    frame.desktop = frame.screen;
    QVector<PageGridPaint> visible;
    (void)PageHit::collect(doc, frame, {}, {}, false, &visible);
    const QRectF shown = PageHit::hostBounds({}, visible);
    QCOMPARE(shown.left(), 2000.0);
    QCOMPARE(shown.right(), 2100.0);
    const QRectF reserved = PageHit::reservedBounds(doc, frame);
    QCOMPARE(reserved.left(), 500.0);
    QCOMPARE(reserved.right(), 2100.0);
    QVERIFY(reserved.contains(shown));
}

void PageHitTest::desktopModeUsesDesktop()
{
    PageGrid g;
    g.id = QStringLiteral("drawer");
    g.desktopMode = true;
    g.anchor = PageAnchor::Bottom;
    g.size.x = PageDim::pixels(400);
    g.size.y = PageDim::pixels(80);
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = QRectF(0, 0, 1920, 1040);
    const QRectF r = PageHit::gridBounds(g, frame);
    QVERIFY(r.bottom() <= frame.desktop.bottom() + 0.51);
    QVERIFY(r.bottom() < frame.screen.bottom() - 1.0);

    PageDocument settings;
    QString err;
    QVERIFY2(PageLoader::loadFromXml(
                 QByteArray(R"xml(
<Page id="p">
  <Grid id="g" desktopMode="true" anchor="Top" size="A_ScreenHeight/9*16, A_ScreenHeight"/>
</Page>
)xml"),
                 settings, &err),
             qPrintable(err));
    const QRectF board = PageHit::gridBounds(settings.grids[0], frame);
    QCOMPARE(board.height(), 1040.0);
    QCOMPARE(board.width(), 1040.0 * 16.0 / 9.0);
    QCOMPARE(board.top(), frame.desktop.top());
    QVERIFY(board.bottom() <= frame.desktop.bottom() + 0.51);
}

namespace {

PageTarget hitCell(bool master, PageTarget::Kind kind, const QString& id, const QString& pageId)
{
    PageTarget t;
    t.kind = kind;
    t.interactive = true;
    t.master = master;
    t.pageId = pageId;
    t.id = id;
    t.geom.dwellZone = QRectF(0, 0, 100, 100);
    t.geom.progressZone = t.geom.dwellZone;
    t.geom.visual = t.geom.dwellZone;
    return t;
}

PageGridPaint hitGrid(bool master, const QString& pageId)
{
    PageGridPaint g;
    g.pageId = pageId;
    g.master = master;
    g.visual = QRectF(0, 0, 100, 100);
    return g;
}

} // namespace

void PageHitTest::masterZonesBeatOpenGrids()
{
    const PageTarget openGrid = hitCell(false, PageTarget::Kind::Cell, QStringLiteral("key"),
                                        QStringLiteral("kb"));
    const PageTarget openZone = hitCell(false, PageTarget::Kind::Zone, QStringLiteral("openZone"),
                                        QStringLiteral("kb"));
    const PageTarget masterGrid = hitCell(true, PageTarget::Kind::Cell, QStringLiteral("drawer"),
                                          QStringLiteral("main"));
    const PageTarget masterZone = hitCell(true, PageTarget::Kind::Zone, QStringLiteral("chip"),
                                          QStringLiteral("main"));

    // One open page then master: cells, zones per page (master in front).
    const QVector<PageTarget> backToFront{openGrid, openZone, masterGrid, masterZone};
    QCOMPARE(PageHit::at(backToFront, QPointF(50, 50))->id, QStringLiteral("chip"));

    const QVector<PageTarget> withoutMasterZone{openGrid, openZone, masterGrid};
    QCOMPARE(PageHit::at(withoutMasterZone, QPointF(50, 50))->id, QStringLiteral("drawer"));

    const QVector<PageTarget> openOnly{openGrid, openZone};
    QCOMPARE(PageHit::at(openOnly, QPointF(50, 50))->id, QStringLiteral("openZone"));
}

void PageHitTest::masterGridCoversOpenTargets()
{
    const PageGridPaint openPaint = hitGrid(false, QStringLiteral("kb"));
    const PageGridPaint masterPaint = hitGrid(true, QStringLiteral("main"));
    const QVector<PageGridPaint> grids{openPaint, masterPaint};

    const PageTarget openCell = hitCell(false, PageTarget::Kind::Cell, QStringLiteral("key"),
                                        QStringLiteral("kb"));
    const PageTarget openZone = hitCell(false, PageTarget::Kind::Zone, QStringLiteral("openZone"),
                                        QStringLiteral("kb"));
    const PageTarget masterCell = hitCell(true, PageTarget::Kind::Cell, QStringLiteral("drawer"),
                                          QStringLiteral("main"));
    const PageTarget masterZone = hitCell(true, PageTarget::Kind::Zone, QStringLiteral("chip"),
                                          QStringLiteral("main"));

    QCOMPARE(PageHit::coveringPageId(grids, QPointF(50, 50)), QStringLiteral("main"));
    const QVector<PageTarget> all{openCell, openZone, masterCell, masterZone};
    QCOMPARE(PageHit::at(all, QPointF(50, 50), 1.0, {}, grids)->id, QStringLiteral("chip"));

    const QVector<PageTarget> noMasterZone{openCell, openZone, masterCell};
    QCOMPARE(PageHit::at(noMasterZone, QPointF(50, 50), 1.0, {}, grids)->id,
             QStringLiteral("drawer"));

    const QVector<PageTarget> openOnly{openCell, openZone};
    QVERIFY(PageHit::at(openOnly, QPointF(50, 50), 1.0, {}, grids) == nullptr);
}

void PageHitTest::frontPageGridOccludesBackPage()
{
    const PageGridPaint older = hitGrid(false, QStringLiteral("kb"));
    const PageGridPaint newer = hitGrid(false, QStringLiteral("settings"));
    const QVector<PageGridPaint> grids{older, newer};

    const PageTarget olderCell = hitCell(false, PageTarget::Kind::Cell, QStringLiteral("key"),
                                         QStringLiteral("kb"));
    const PageTarget olderZone = hitCell(false, PageTarget::Kind::Zone, QStringLiteral("more"),
                                         QStringLiteral("kb"));
    const PageTarget newerCell = hitCell(false, PageTarget::Kind::Cell, QStringLiteral("row"),
                                         QStringLiteral("settings"));

    QCOMPARE(PageHit::coveringPageId(grids, QPointF(50, 50)), QStringLiteral("settings"));

    // Per-page layers: kb (cells, zones) then settings. Newer grid buries older cells and zones.
    const QVector<PageTarget> layer{olderCell, olderZone, newerCell};
    QCOMPARE(PageHit::at(layer, QPointF(50, 50), 1.0, {}, grids)->id, QStringLiteral("row"));
}

void PageHitTest::visibleCellRemainderHits()
{
    PageGridPaint older = hitGrid(false, QStringLiteral("kb"));
    older.visual = QRectF(0, 0, 100, 100);
    PageGridPaint newer = hitGrid(false, QStringLiteral("settings"));
    newer.visual = QRectF(50, 0, 100, 100);
    const QVector<PageGridPaint> grids{older, newer};

    PageTarget olderCell = hitCell(false, PageTarget::Kind::Cell, QStringLiteral("key"),
                                   QStringLiteral("kb"));
    olderCell.geom.dwellZone = QRectF(0, 0, 100, 100);
    olderCell.geom.progressZone = olderCell.geom.dwellZone;
    olderCell.geom.visual = olderCell.geom.dwellZone;
    PageTarget newerCell = hitCell(false, PageTarget::Kind::Cell, QStringLiteral("row"),
                                   QStringLiteral("settings"));
    newerCell.geom.dwellZone = QRectF(50, 0, 100, 100);
    newerCell.geom.progressZone = newerCell.geom.dwellZone;
    newerCell.geom.visual = newerCell.geom.dwellZone;

    const QVector<PageTarget> layer{olderCell, newerCell};
    QCOMPARE(PageHit::at(layer, QPointF(25, 50), 1.0, {}, grids)->id, QStringLiteral("key"));
    QCOMPARE(PageHit::at(layer, QPointF(75, 50), 1.0, {}, grids)->id, QStringLiteral("row"));
}

void PageHitTest::engagedHullDoesNotPierceCover()
{
    PageGridPaint older = hitGrid(false, QStringLiteral("kb"));
    older.visual = QRectF(0, 0, 80, 80);
    PageGridPaint newer = hitGrid(false, QStringLiteral("settings"));
    newer.visual = QRectF(40, 0, 80, 80);
    const QVector<PageGridPaint> grids{older, newer};

    PageTarget olderZone = hitCell(false, PageTarget::Kind::Zone, QStringLiteral("more"),
                                   QStringLiteral("kb"));
    olderZone.geom.dwellZone = QRectF(0, 0, 80, 80);
    olderZone.geom.progressZone = QRectF(0, 0, 120, 80);
    olderZone.geom.visual = olderZone.geom.progressZone;
    PageTarget newerCell = hitCell(false, PageTarget::Kind::Cell, QStringLiteral("row"),
                                   QStringLiteral("settings"));
    newerCell.geom.dwellZone = QRectF(40, 0, 80, 80);
    newerCell.geom.progressZone = newerCell.geom.dwellZone;
    newerCell.geom.visual = newerCell.geom.dwellZone;

    const QVector<PageTarget> layer{olderZone, newerCell};
    const QString engaged = sessionKey(olderZone);
    QVERIFY(PageHit::gazeHitPolygon(olderZone, engaged).containsPoint(QPointF(100, 40),
                                                                      Qt::WindingFill));
    QCOMPARE(PageHit::at(layer, QPointF(100, 40), 1.0, engaged, grids)->id, QStringLiteral("row"));
    QCOMPARE(PageHit::at(layer, QPointF(20, 40), 1.0, engaged, grids)->id, QStringLiteral("more"));
}

QObject* createPageHitTest()
{
    return new PageHitTest;
}

#include "PageHitTest.moc"
