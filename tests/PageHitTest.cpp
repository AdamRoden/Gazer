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
    void mainChipProgressOverlapsTaskbar();
    void hitMainChipOffScreen();
    void engagedZoneIncludesProgress();
    void hitDrawerCell();
    void visibleWhenHidesMainChip();
    void sleepKeepsContentWhenSuspended();
    void edgeChipHidesUntilProgress();
    void qwertyClosedGridHidden();
    void showHidesGridCellAndZone();
    void edgeChipGazeHitsOnScreenChrome();
    void liveEditorGridHasOpaqueChrome();
    void overlappingBoardOccludesLowerPage();
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

void PageHitTest::mainChipProgressOverlapsTaskbar()
{
    PageDocument doc;
    QString err;
    const QString path =
        QStringLiteral(GAZER_SOURCE_DIR) + QStringLiteral("/resources/layouts/main.xml");
    QVERIFY2(PageLoader::loadFromFile(path, doc, &err), qPrintable(err));
    const PageZone* chip = doc.findZone(QStringLiteral("mainChip"));
    const PageZone* sleep = doc.findZone(QStringLiteral("sleep"));
    QVERIFY(chip);
    QVERIFY(sleep);
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = QRectF(0, 0, 1920, 1040);
    const QSet<QString> hiddenGrids{QStringLiteral("drawer"), QStringLiteral("quit")};
    const QVector<PageTarget> t = PageHit::collect(doc, frame, hiddenGrids, {});
    const PageTarget* vis = targetById(t, QStringLiteral("mainChip"));
    const PageTarget* sleepT = targetById(t, QStringLiteral("sleep"));
    QVERIFY(vis);
    QVERIFY(sleepT);
    QCOMPARE(vis->geom.progressZone.bottom(), frame.screen.bottom());
    QCOMPARE(sleepT->geom.progressZone.bottom(), frame.screen.bottom());
    QVERIFY(vis->geom.progressZone.bottom() > frame.desktop.bottom() + 1.0);
    QVERIFY(vis->geom.progressZone.intersects(
        QRectF(QPointF(frame.desktop.left(), frame.desktop.bottom()), frame.screen.bottomRight())));
}

void PageHitTest::hitMainChipOffScreen()
{
    PageDocument doc;
    QString err;
    const QString path =
        QStringLiteral(GAZER_SOURCE_DIR) + QStringLiteral("/resources/layouts/main.xml");
    QVERIFY2(PageLoader::loadFromFile(path, doc, &err), qPrintable(err));
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    const QSet<QString> hiddenGrids{QStringLiteral("drawer"), QStringLiteral("quit")};
    const QVector<PageTarget> t = PageHit::collect(doc, frame, hiddenGrids, {});
    const PageTarget* chip = targetById(t, QStringLiteral("mainChip"));
    QVERIFY(chip);
    const PageTarget* hit = PageHit::at(t, chip->geom.dwellZone.center());
    QVERIFY(hit);
    QCOMPARE(hit->kind, PageTarget::Kind::Zone);
    QCOMPARE(hit->id, QStringLiteral("mainChip"));
    QVERIFY(hit->geom.progressZone.intersects(frame.screen));
    QVERIFY(!frame.screen.contains(hit->geom.dwellZone.center()));
}

void PageHitTest::engagedZoneIncludesProgress()
{
    PageDocument doc;
    QString err;
    const QString path =
        QStringLiteral(GAZER_SOURCE_DIR) + QStringLiteral("/resources/layouts/main.xml");
    QVERIFY2(PageLoader::loadFromFile(path, doc, &err), qPrintable(err));
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    const QSet<QString> hiddenGrids{QStringLiteral("drawer"), QStringLiteral("quit")};
    const QVector<PageTarget> t = PageHit::collect(doc, frame, hiddenGrids, {});
    const PageTarget* chip = targetById(t, QStringLiteral("mainChip"));
    QVERIFY(chip);
    QVERIFY(PageHit::at(t, chip->geom.dwellZone.center()) == chip);
    const QPointF onStrip = chip->geom.progressZone.center();
    QVERIFY(!chip->geom.dwellZone.contains(onStrip));
    QVERIFY(PageHit::at(t, onStrip) == nullptr);
    const PageTarget* held = PageHit::at(t, onStrip, 1.0, chip->id);
    QVERIFY(held);
    QCOMPARE(held->id, chip->id);
    const QPointF gap(onStrip.x(), (chip->geom.visual.bottom() + chip->geom.dwellZone.top()) / 2.0);
    QVERIFY(!chip->geom.dwellZone.contains(gap));
    QVERIFY(!chip->geom.visual.contains(gap));
    QVERIFY(PageHit::at(t, gap) == nullptr);
    QVERIFY(PageHit::at(t, gap, 1.0, chip->id) == chip);
}

void PageHitTest::hitDrawerCell()
{
    PageDocument doc;
    QString err;
    const QString path =
        QStringLiteral(GAZER_SOURCE_DIR) + QStringLiteral("/resources/layouts/main.xml");
    QVERIFY2(PageLoader::loadFromFile(path, doc, &err), qPrintable(err));
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    const QSet<QString> hiddenGrids{QStringLiteral("quit")};
    const QVector<PageTarget> t = PageHit::collect(doc, frame, hiddenGrids, {});
    const PageGrid* drawer = doc.findGrid(QStringLiteral("drawer"));
    QVERIFY(drawer);
    const QRectF bounds = PageHit::gridBounds(*drawer, frame);
    const QRectF cell0 = PageHit::cellRect(*drawer, bounds, 0, 0, 1, 1);
    QVERIFY(!cell0.isEmpty());
    const PageTarget* hit = PageHit::at(t, cell0.center());
    QVERIFY(hit);
    QCOMPARE(hit->kind, PageTarget::Kind::Cell);
    QCOMPARE(hit->gridId, QStringLiteral("drawer"));
    QCOMPARE(hit->id, QStringLiteral("open_keyboard"));
    QVERIFY(hit->shell);
    QVERIFY(t.last().shell);
}

void PageHitTest::visibleWhenHidesMainChip()
{
    PageDocument doc;
    QString err;
    const QString path =
        QStringLiteral(GAZER_SOURCE_DIR) + QStringLiteral("/resources/layouts/main.xml");
    QVERIFY2(PageLoader::loadFromFile(path, doc, &err), qPrintable(err));
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    QVariantMap expanded;
    expanded.insert(QStringLiteral("expanded"), true);
    const QSet<QString> hiddenGrids{QStringLiteral("drawer"), QStringLiteral("quit")};
    const QVector<PageTarget> t = PageHit::collect(doc, frame, hiddenGrids, expanded, false);
    QVERIFY(targetById(t, QStringLiteral("mainChip")) == nullptr);
    const PageTarget* sleep = targetById(t, QStringLiteral("sleep"));
    QVERIFY(sleep);
    QVERIFY(PageHit::at(t, sleep->geom.dwellZone.center()) == sleep);
}

void PageHitTest::sleepKeepsContentWhenSuspended()
{
    PageDocument doc;
    QString err;
    const QString path =
        QStringLiteral(GAZER_SOURCE_DIR) + QStringLiteral("/resources/layouts/main.xml");
    QVERIFY2(PageLoader::loadFromFile(path, doc, &err), qPrintable(err));
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    const QSet<QString> hiddenGrids{QStringLiteral("drawer"), QStringLiteral("quit")};
    const QVector<PageTarget> t = PageHit::collect(doc, frame, hiddenGrids, {}, true);
    const PageTarget* sleep = nullptr;
    const PageTarget* main = nullptr;
    for (const PageTarget& x : t) {
        if (x.id == QLatin1String("sleep")) {
            sleep = &x;
        } else if (x.id == QLatin1String("mainChip")) {
            main = &x;
        }
    }
    QVERIFY(sleep);
    QVERIFY(sleep->suspendExempt);
    QVERIFY(sleep->interactive);
    QVERIFY(!sleep->label.isEmpty() || !sleep->icon.isEmpty());
    QVERIFY(main);
    QVERIFY(main->interactive);
    QVERIFY(main->suspendExempt);
}

void PageHitTest::edgeChipHidesUntilProgress()
{
    PageDocument doc;
    QString err;
    const QString path =
        QStringLiteral(GAZER_SOURCE_DIR) + QStringLiteral("/resources/layouts/main.xml");
    QVERIFY2(PageLoader::loadFromFile(path, doc, &err), qPrintable(err));
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    const QVector<PageTarget> t =
        PageHit::collect(doc, frame, {QStringLiteral("drawer"), QStringLiteral("quit")});
    const PageTarget* sleep = nullptr;
    for (const PageTarget& x : t) {
        if (x.id == QLatin1String("sleep")) {
            sleep = &x;
        }
    }
    QVERIFY(sleep);
    QVERIFY(sleep->geom.hidesUntilProgress());

    PageDocument kb;
    const QString kbPath =
        QStringLiteral(GAZER_SOURCE_DIR) + QStringLiteral("/resources/layouts/uw_qwerty.xml");
    QVERIFY2(PageLoader::loadFromFile(kbPath, kb, &err), qPrintable(err));
    const QVector<PageTarget> keys = PageHit::collect(kb, frame);
    const PageTarget* kbSleep = nullptr;
    for (const PageTarget& x : keys) {
        if (x.id == QLatin1String("sleep")) {
            kbSleep = &x;
        }
    }
    QVERIFY(kbSleep);
    QVERIFY(!kbSleep->geom.hidesUntilProgress());
}

void PageHitTest::qwertyClosedGridHidden()
{
    PageDocument kb;
    QString err;
    const QString kbPath =
        QStringLiteral(GAZER_SOURCE_DIR) + QStringLiteral("/resources/layouts/uw_qwerty.xml");
    QVERIFY2(PageLoader::loadFromFile(kbPath, kb, &err), qPrintable(err));
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    QVERIFY(!kb.findGrid(QStringLiteral("vert2"))->show);
    const QVector<PageTarget> keys = PageHit::collect(kb, frame);
    QVERIFY(targetById(keys, QStringLiteral("k_q")));
    QVERIFY(targetById(keys, QStringLiteral("sleep")));
    QVERIFY(!targetById(keys, QStringLiteral("max")));
    const QVector<PageTarget> all = PageHit::collect(kb, frame, {}, {}, false, nullptr, true);
    QVERIFY(targetById(all, QStringLiteral("max")));
}

void PageHitTest::showHidesGridCellAndZone()
{
    const QByteArray xml = R"xml(
<Page id="p">
  <Grid id="shown" size="200,200">
    <Cell id="a" label="A"/>
    <Cell id="b" label="B" show="false"/>
  </Grid>
  <Grid id="hidden" size="200,200" show="false">
    <Cell id="c" label="C"/>
  </Grid>
  <Zone id="zshow" size="80,40"/>
  <Zone id="zhide" size="80,40" show="false"/>
</Page>
)xml";
    PageDocument doc;
    QString err;
    QVERIFY2(PageLoader::loadFromXml(xml, doc, &err), qPrintable(err));
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    QVector<PageGridPaint> grids;
    const QVector<PageTarget> t = PageHit::collect(doc, frame, {}, {}, false, &grids);
    QVERIFY(targetById(t, QStringLiteral("a")));
    QVERIFY(!targetById(t, QStringLiteral("b")));
    QVERIFY(!targetById(t, QStringLiteral("c")));
    QVERIFY(targetById(t, QStringLiteral("zshow")));
    QVERIFY(!targetById(t, QStringLiteral("zhide")));
    bool sawShown = false;
    bool sawHidden = false;
    for (const PageGridPaint& g : grids) {
        if (g.gridId == QLatin1String("shown")) {
            sawShown = true;
        }
        if (g.gridId == QLatin1String("hidden")) {
            sawHidden = true;
        }
    }
    QVERIFY(sawShown);
    QVERIFY(!sawHidden);
    const QVector<PageTarget> all = PageHit::collect(doc, frame, {}, {}, false, nullptr, true);
    QVERIFY(targetById(all, QStringLiteral("b")));
    QVERIFY(targetById(all, QStringLiteral("c")));
    QVERIFY(targetById(all, QStringLiteral("zhide")));
}

void PageHitTest::edgeChipGazeHitsOnScreenChrome()
{
    PageDocument doc;
    QString err;
    const QString path =
        QStringLiteral(GAZER_SOURCE_DIR) + QStringLiteral("/resources/layouts/main.xml");
    QVERIFY2(PageLoader::loadFromFile(path, doc, &err), qPrintable(err));
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    const QSet<QString> hiddenGrids{QStringLiteral("drawer"), QStringLiteral("quit")};
    const QVector<PageTarget> t = PageHit::collect(doc, frame, hiddenGrids, {});
    const PageTarget* chip = targetById(t, QStringLiteral("mainChip"));
    QVERIFY(chip);
    QVERIFY(chip->suspendExempt);
    QVERIFY(chip->geom.hidesUntilProgress());
    const QRectF scan = PageHit::gazeHitRect(*chip);
    QVERIFY(scan.contains(chip->geom.dwellZone.center()));
    QVERIFY(!scan.contains(chip->geom.progressZone.center()));
    QVERIFY(PageHit::at(t, chip->geom.progressZone.center()) == nullptr);
    const QRectF acc = PageHit::gazeHitRect(*chip, chip->id);
    QVERIFY(acc.contains(chip->geom.progressZone.center()));
    QVERIFY(PageHit::at(t, chip->geom.progressZone.center(), 1.0, chip->id) == chip);
}

void PageHitTest::liveEditorGridHasOpaqueChrome()
{
    PageDocument doc;
    doc.id = QStringLiteral("settings_color_live");
    PageGrid g;
    g.id = QStringLiteral("board");
    g.desktopMode = true;
    g.anchor = PageAnchor::Center;
    g.size.x = PageDim::pixels(1400);
    g.size.y = PageDim::pixels(980);
    g.style.background = QColor(10, 10, 11, 255);
    g.style.radius = PageBox::all(8.0);
    g.style.thickness = PageBox::all(1.0);
    doc.grids.push_back(g);
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    QVector<PageGridPaint> grids;
    (void)PageHit::collect(doc, frame, {}, {}, false, &grids);
    QCOMPARE(grids.size(), 1);
    QVERIFY(grids[0].chrome.background.has_value());
    QCOMPARE(grids[0].chrome.background->alpha(), 255);
    QCOMPARE(grids[0].visual.width(), 1400.0);
    QCOMPARE(grids[0].visual.height(), 980.0);
}

void PageHitTest::overlappingBoardOccludesLowerPage()
{
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;

    PageDocument settings;
    settings.id = QStringLiteral("main_settings_theme");
    PageGrid sg;
    sg.id = QStringLiteral("board");
    sg.desktopMode = true;
    sg.anchor = PageAnchor::Top;
    sg.size.x = PageDim::pixels(1600);
    sg.size.y = PageDim::pixels(1080);
    sg.rows = 1;
    sg.columns = 1;
    sg.style.background = QColor(20, 20, 40, 255);
    PageCell sc;
    sc.id = QStringLiteral("bg_name");
    sc.label = QStringLiteral("Background");
    sg.cells.push_back(sc);
    settings.grids.push_back(sg);

    PageDocument editor;
    editor.id = QStringLiteral("settings_color_live");
    PageGrid eg;
    eg.id = QStringLiteral("board");
    eg.desktopMode = true;
    eg.anchor = PageAnchor::Center;
    eg.size.x = PageDim::pixels(400);
    eg.size.y = PageDim::pixels(400);
    eg.rows = 1;
    eg.columns = 1;
    eg.style.background = QColor(10, 10, 11, 255);
    editor.grids.push_back(eg);

    QVector<PageGridPaint> grids;
    QVector<PageTarget> targets;
    QVector<PageGridPaint> g1;
    QVector<PageTarget> t1 = PageHit::collect(settings, frame, {}, {}, false, &g1);
    for (PageGridPaint& gp : g1) {
        gp.pageId = settings.id;
        grids.push_back(gp);
    }
    for (PageTarget& t : t1) {
        t.pageId = settings.id;
        t.id = settings.id + QLatin1Char('/') + t.id;
        targets.push_back(t);
    }
    QVector<PageGridPaint> g2;
    QVector<PageTarget> t2 = PageHit::collect(editor, frame, {}, {}, false, &g2);
    for (PageGridPaint& gp : g2) {
        gp.pageId = editor.id;
        grids.push_back(gp);
    }
    for (PageTarget& t : t2) {
        t.pageId = editor.id;
        targets.push_back(t);
    }

    QCOMPARE(grids.size(), 2);
    const QPointF inEditor = grids[1].visual.center();
    QVERIFY(grids[0].visual.contains(inEditor));
    QCOMPARE(PageHit::coveringPageId(grids, inEditor, 1.0, targets), editor.id);
    QVERIFY(PageHit::at(targets, inEditor, 1.0, {}, grids) == nullptr);
    const PageTarget* lower = PageHit::at(targets, inEditor);
    QVERIFY(lower);
    QCOMPARE(lower->pageId, settings.id);
}

QObject* createPageHitTest()
{
    return new PageHitTest;
}

#include "PageHitTest.moc"
