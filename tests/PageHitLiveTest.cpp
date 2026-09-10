#include "layout/PageDetector.h"
#include "layout/PageDim.h"
#include "layout/PageEdit.h"
#include "layout/PageHit.h"
#include "layout/PageLoader.h"

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

class PageHitLiveTest final : public QObject {
    Q_OBJECT

private slots:
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
    void coveringGridCornerOccludesLowerPage();
    void remapPageActionTargetsIncludesPhases();
};

void PageHitLiveTest::mainChipProgressOverlapsTaskbar()
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
    const QVector<PageTarget> t = PageHit::collect(doc, frame);
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

void PageHitLiveTest::hitMainChipOffScreen()
{
    PageDocument doc;
    QString err;
    const QString path =
        QStringLiteral(GAZER_SOURCE_DIR) + QStringLiteral("/resources/layouts/main.xml");
    QVERIFY2(PageLoader::loadFromFile(path, doc, &err), qPrintable(err));
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    const QVector<PageTarget> t = PageHit::collect(doc, frame);
    const PageTarget* chip = targetById(t, QStringLiteral("mainChip"));
    QVERIFY(chip);
    const PageTarget* hit = PageHit::at(t, chip->geom.dwellZone.center());
    QVERIFY(hit);
    QCOMPARE(hit->kind, PageTarget::Kind::Zone);
    QCOMPARE(hit->id, QStringLiteral("mainChip"));
    QVERIFY(hit->geom.progressZone.intersects(frame.screen));
    QVERIFY(!frame.screen.contains(hit->geom.dwellZone.center()));
}

void PageHitLiveTest::engagedZoneIncludesProgress()
{
    PageDocument doc;
    QString err;
    const QString path =
        QStringLiteral(GAZER_SOURCE_DIR) + QStringLiteral("/resources/layouts/main.xml");
    QVERIFY2(PageLoader::loadFromFile(path, doc, &err), qPrintable(err));
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    const QVector<PageTarget> t = PageHit::collect(doc, frame);
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

void PageHitLiveTest::hitDrawerCell()
{
    PageDocument doc;
    QString err;
    const QString path =
        QStringLiteral(GAZER_SOURCE_DIR) + QStringLiteral("/resources/layouts/main.xml");
    QVERIFY2(PageLoader::loadFromFile(path, doc, &err), qPrintable(err));
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    PageGrid* drawerGrid = doc.findGrid(QStringLiteral("drawer"));
    QVERIFY(drawerGrid);
    doc.showLayers = {1, 2};
    const QVector<PageTarget> t = PageHit::collect(doc, frame);
    const PageGrid* drawer = drawerGrid;
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

void PageHitLiveTest::visibleWhenHidesMainChip()
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
    const QVector<PageTarget> t = PageHit::collect(doc, frame, expanded);
    QVERIFY(targetById(t, QStringLiteral("mainChip")) == nullptr);
    const PageTarget* sleep = targetById(t, QStringLiteral("sleep"));
    QVERIFY(sleep);
    QVERIFY(PageHit::at(t, sleep->geom.dwellZone.center()) == sleep);
}

void PageHitLiveTest::sleepKeepsContentWhenSuspended()
{
    PageDocument doc;
    QString err;
    const QString path =
        QStringLiteral(GAZER_SOURCE_DIR) + QStringLiteral("/resources/layouts/main.xml");
    QVERIFY2(PageLoader::loadFromFile(path, doc, &err), qPrintable(err));
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    const QVector<PageTarget> t = PageHit::collect(doc, frame, {}, true);
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

void PageHitLiveTest::edgeChipHidesUntilProgress()
{
    PageDocument doc;
    QString err;
    const QString path =
        QStringLiteral(GAZER_SOURCE_DIR) + QStringLiteral("/resources/layouts/main.xml");
    QVERIFY2(PageLoader::loadFromFile(path, doc, &err), qPrintable(err));
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    const QVector<PageTarget> t = PageHit::collect(doc, frame);
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

void PageHitLiveTest::qwertyClosedGridHidden()
{
    PageDocument kb;
    QString err;
    const QString kbPath =
        QStringLiteral(GAZER_SOURCE_DIR) + QStringLiteral("/resources/layouts/uw_qwerty.xml");
    QVERIFY2(PageLoader::loadFromFile(kbPath, kb, &err), qPrintable(err));
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    QCOMPARE(kb.findGrid(QStringLiteral("vert2"))->layers, QVector<int>({2}));
    const QVector<PageTarget> keys = PageHit::collect(kb, frame);
    QVERIFY(targetById(keys, QStringLiteral("k_q")));
    QVERIFY(targetById(keys, QStringLiteral("sleep")));
    QVERIFY(!targetById(keys, QStringLiteral("max")));
    const QVector<PageTarget> all =
        PageHit::collect(kb, frame, {}, false, nullptr, false, PageEdit::usedLayers(kb));
    QVERIFY(targetById(all, QStringLiteral("max")));
}

void PageHitLiveTest::showHidesGridCellAndZone()
{
    const QByteArray xml = R"xml(
<Page id="p">
  <Grid id="shown" size="200,200">
    <Cell id="a" label="A"/>
  </Grid>
  <Grid id="hidden" size="200,200" layers="2">
    <Cell id="c" label="C"/>
  </Grid>
  <Zone id="zshow" size="80,40"/>
  <Zone id="zhide" size="80,40" layers="2"/>
</Page>
)xml";
    PageDocument doc;
    QString err;
    QVERIFY2(PageLoader::loadFromXml(xml, doc, &err), qPrintable(err));
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    QVector<PageGridPaint> grids;
    const QVector<PageTarget> t = PageHit::collect(doc, frame, {}, false, &grids);
    QVERIFY(targetById(t, QStringLiteral("a")));
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
    const QVector<PageTarget> all =
        PageHit::collect(doc, frame, {}, false, nullptr, false, PageEdit::usedLayers(doc));
    QVERIFY(targetById(all, QStringLiteral("c")));
    QVERIFY(targetById(all, QStringLiteral("zhide")));
}

void PageHitLiveTest::edgeChipGazeHitsOnScreenChrome()
{
    PageDocument doc;
    QString err;
    const QString path =
        QStringLiteral(GAZER_SOURCE_DIR) + QStringLiteral("/resources/layouts/main.xml");
    QVERIFY2(PageLoader::loadFromFile(path, doc, &err), qPrintable(err));
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    const QVector<PageTarget> t = PageHit::collect(doc, frame);
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

void PageHitLiveTest::liveEditorGridHasOpaqueChrome()
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
    (void)PageHit::collect(doc, frame, {}, false, &grids);
    QCOMPARE(grids.size(), 1);
    QVERIFY(grids[0].chrome.background.isSet());
    QCOMPARE(grids[0].chrome.background.parsed().alpha(), 255);
    QCOMPARE(grids[0].visual.width(), 1400.0);
    QCOMPARE(grids[0].visual.height(), 980.0);
}

void PageHitLiveTest::overlappingBoardOccludesLowerPage()
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
    QVector<PageTarget> t1 = PageHit::collect(settings, frame, {}, false, &g1);
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
    QVector<PageTarget> t2 = PageHit::collect(editor, frame, {}, false, &g2);
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

void PageHitLiveTest::coveringGridCornerOccludesLowerPage()
{
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;

    PageDocument back;
    back.id = QStringLiteral("back");
    PageGrid bg;
    bg.id = QStringLiteral("board");
    bg.desktopMode = true;
    bg.anchor = PageAnchor::TopLeft;
    bg.offset.x = PageDim::pixels(0);
    bg.offset.y = PageDim::pixels(0);
    bg.size.x = PageDim::pixels(400);
    bg.size.y = PageDim::pixels(400);
    bg.rows = 1;
    bg.columns = 1;
    PageCell bc;
    bc.id = QStringLiteral("under");
    bg.cells.push_back(bc);
    back.grids.push_back(bg);

    PageDocument front;
    front.id = QStringLiteral("front");
    PageGrid fg;
    fg.id = QStringLiteral("board");
    fg.desktopMode = true;
    fg.anchor = PageAnchor::TopLeft;
    fg.offset.x = PageDim::pixels(0);
    fg.offset.y = PageDim::pixels(0);
    fg.size.x = PageDim::pixels(200);
    fg.size.y = PageDim::pixels(200);
    fg.style.radius = PageBox::all(80.0);
    fg.style.background = QColor(10, 10, 11, 255);
    front.grids.push_back(fg);

    QVector<PageGridPaint> grids;
    QVector<PageTarget> targets;
    QVector<PageGridPaint> g1;
    QVector<PageTarget> t1 = PageHit::collect(back, frame, {}, false, &g1);
    for (PageGridPaint& gp : g1) {
        gp.pageId = back.id;
        grids.push_back(gp);
    }
    for (PageTarget& t : t1) {
        t.pageId = back.id;
        targets.push_back(t);
    }
    QVector<PageGridPaint> g2;
    QVector<PageTarget> t2 = PageHit::collect(front, frame, {}, false, &g2);
    for (PageGridPaint& gp : g2) {
        gp.pageId = front.id;
        grids.push_back(gp);
    }
    for (PageTarget& t : t2) {
        t.pageId = front.id;
        targets.push_back(t);
    }

    const QPointF corner = grids[1].visual.topLeft() + QPointF(2.0, 2.0);
    QVERIFY(grids[1].visual.contains(corner));
    QVERIFY(!PageHit::shapeContains(grids[1].visual, grids[1].chrome, corner));
    QCOMPARE(PageHit::coveringPageId(grids, corner, 1.0, targets), front.id);
    QVERIFY(PageHit::at(targets, corner, 1.0, {}, grids) == nullptr);
}

void PageHitLiveTest::remapPageActionTargetsIncludesPhases()
{
    PageDocument doc;
    doc.id = QStringLiteral("p");
    PageGrid g;
    g.id = QStringLiteral("g");
    PageCell c;
    c.id = QStringLiteral("c");
    PageAction a;
    a.type = PageActionType::Nav;
    a.verb = PageVerb::Open;
    a.targetScope = PageNavScope::Id;
    a.targetId = QStringLiteral("old");
    PagePhase ph;
    ph.actions.push_back(a);
    c.phases.push_back(ph);
    g.cells.push_back(c);
    doc.grids.push_back(g);

    QHash<QString, QString> idMap;
    idMap.insert(QStringLiteral("old"), QStringLiteral("new"));
    PageEdit::remapPageActionTargets(doc, idMap);
    QCOMPARE(doc.grids[0].cells[0].phases[0].actions[0].targetId, QStringLiteral("new"));
}


QObject* createPageHitLiveTest()
{
    return new PageHitLiveTest;
}

#include "PageHitLiveTest.moc"
