#include "layout/PageDetector.h"
#include "layout/PageDim.h"
#include "layout/PageHit.h"
#include "layout/PageLoader.h"
#include "layout/PageResolve.h"

#include <QFile>
#include <QSet>
#include <QStringList>
#include <QtTest>

using namespace gazer;

class PageLoaderTest final : public QObject {
    Q_OBJECT

private slots:
    void dimPixelsVsProportion();
    void dimFraction();
    void placeRectBottom();
    void loadFixture();
    void inheritStyleAndDwell();
    void zoneDwellDefaultsAndOffset();
    void rejectMissingPageId();
    void rejectUnknownChild();
    void rejectNonPageRoot();
    void cellDetectorClipsProgress();
    void zoneDetectorUnrestrictedDwell();
    void loadMainPage();
    void hitMainChipOffScreen();
    void engagedZoneIncludesProgress();
    void hitDrawerCell();
    void cellRectSpan();
    void visibleWhenHidesMainChip();
    void drawerMapIsIdentityAtFullScale();
    void drawerMapShrinksAboutBottom();
    void zoneProgressCoercedWhenOffScreen();
    void loadQwertyXml();
    void loadConvertedBoards();
    void collectEmitsGridChrome();
    void settingsPagesAnchorTop();
    void settingsTabsEqualWidth();
    void sleepKeepsContentWhenSuspended();
    void edgeChipHidesUntilProgress();
    void keyboardMainOpensDrawer();
    void liveEditorGridHasOpaqueChrome();
    void overlappingBoardOccludesLowerPage();
};

void PageLoaderTest::dimPixelsVsProportion()
{
    QCOMPARE(PageDimParse::parse(QStringLiteral("150")).unit, PageDim::Unit::Pixels);
    QCOMPARE(PageDimParse::parse(QStringLiteral("150")).value, 150.0);
    QCOMPARE(PageDimParse::parse(QStringLiteral("0.5")).unit, PageDim::Unit::Proportion);
    QCOMPARE(PageDimParse::parse(QStringLiteral("0.5")).value, 0.5);
    QCOMPARE(PageDimParse::parse(QStringLiteral("1.0")).resolve(1920), 1920.0);
    QCOMPARE(PageDimParse::parse(QStringLiteral("1920")).resolve(100), 1920.0);
    QCOMPARE(PageDimParse::parse(QStringLiteral("2")).unit, PageDim::Unit::Pixels);
    QCOMPARE(PageDimParse::parse(QStringLiteral("2.0")).unit, PageDim::Unit::Proportion);
}

void PageLoaderTest::dimFraction()
{
    const PageDim d = PageDimParse::parse(QStringLiteral("-1/2"));
    QCOMPARE(d.unit, PageDim::Unit::Proportion);
    QCOMPARE(d.value, -0.5);
    const PageDimPair p = PageDimParse::parsePair(QStringLiteral("1/2,1/4"));
    QCOMPARE(p.x.resolve(200), 100.0);
    QCOMPARE(p.y.resolve(200), 50.0);
}

void PageLoaderTest::placeRectBottom()
{
    const QRectF bounds(0, 0, 1920, 1080);
    PageDimPair size;
    size.x = PageDim::pixels(300);
    size.y = PageDim::pixels(150);
    PageDimPair offset;
    offset.x = PageDim::pixels(0);
    offset.y = PageDim::pixels(0);
    const QRectF r = PageDimParse::placeRect(bounds, PageAnchor::Bottom, offset, size);
    QCOMPARE(r.width(), 300.0);
    QCOMPARE(r.height(), 150.0);
    QCOMPARE(r.left(), 810.0);
    QCOMPARE(r.top(), 930.0);
}

void PageLoaderTest::loadFixture()
{
    PageDocument doc;
    QString err;
    const QString fixture =
        QStringLiteral(GAZER_TEST_FIXTURES) + QStringLiteral("/example_page.xml");
    QVERIFY2(PageLoader::loadFromFile(fixture, doc, &err), qPrintable(err));
    QCOMPARE(doc.id, QStringLiteral("Example Page"));
    QCOMPARE(doc.grids.size(), 1);
    QCOMPARE(doc.zones.size(), 1);
    const PageGrid* g = doc.findGrid(QStringLiteral("Quick Settings"));
    QVERIFY(g);
    QCOMPARE(g->desktopMode, true);
    QCOMPARE(g->rows, 1);
    QCOMPARE(g->columns, 2);
    QCOMPARE(g->size.x.value, 1920.0);
    QCOMPARE(g->size.x.unit, PageDim::Unit::Pixels);
    QCOMPARE(g->subGrids.size(), 1);
    QCOMPARE(g->cells.size(), 1);
    QCOMPARE(g->subGrids[0].id, QStringLiteral("splitter"));
    QCOMPARE(g->subGrids[0].cells.size(), 2);
    QCOMPARE(g->subGrids[0].cells[0].actions.size(), 1);
    QCOMPARE(g->subGrids[0].cells[0].actions[0].type, PageActionType::Ahk);
    QVERIFY(g->subGrids[0].cells[0].actions[0].ahkSource.contains(QStringLiteral("LAlt")));
    QCOMPARE(g->subGrids[0].cells[1].actions[0].type, PageActionType::Command);
    QCOMPARE(g->subGrids[0].cells[1].actions[0].command, QStringLiteral("Sleep"));
    QCOMPARE(g->cells[0].actions.size(), 2);
    QCOMPARE(g->cells[0].actions[0].type, PageActionType::Page);
    QCOMPARE(g->cells[0].actions[0].verb, PageVerb::Open);
    QCOMPARE(g->cells[0].actions[0].targetKind, PageTargetKind::Page);
    QCOMPARE(g->cells[0].actions[0].targetId, QStringLiteral("uw_qwerty"));
    QCOMPARE(g->cells[0].actions[1].type, PageActionType::Send);
    QCOMPARE(g->cells[0].actions[1].sendKey, QStringLiteral("Enter"));
    QCOMPARE(g->cells[0].actions[1].sendEdge, QStringLiteral("Down"));
    QCOMPARE(g->cells[0].actions[1].sendDurationMs, 50);

    const PageZone* z = doc.findZone(QStringLiteral("edge"));
    QVERIFY(z);
    QCOMPARE(z->dwellExempt, true);
    QCOMPARE(z->dwellOffset.y.value, 240.0);
    QCOMPARE(z->dwellSize.x.value, 300.0);
    QCOMPARE(z->actions[0].targetKind, PageTargetKind::Grid);
    QCOMPARE(z->actions[0].targetId, QStringLiteral("Quick Settings"));

    QVERIFY(doc.styles.contains(QStringLiteral("stl")));
    QVERIFY(doc.dwells.contains(QStringLiteral("dwl")));
    QCOMPARE(doc.dwells.value(QStringLiteral("dwl")).activation->size(), 9);
    QCOMPARE(doc.dwells.value(QStringLiteral("dwl")).activation->at(0), 0);
}

void PageLoaderTest::inheritStyleAndDwell()
{
    const QByteArray xml = R"xml(
<Page id="p">
  <Style id="pageStl" background="#FF000000" radius="4"/>
  <Style id="cellStl" foreground="#FFFFFFFF"/>
  <Dwell id="slow" scanGrace="300" activation="800"/>
  <Grid id="g" style="pageStl" dwell="slow" rows="1" columns="1" size="100,100">
    <Cell id="c" style="cellStl" radius="12" scanGrace="50" label="X"/>
  </Grid>
</Page>
)xml";
    PageDocument doc;
    QString err;
    QVERIFY2(PageLoader::loadFromXml(xml, doc, &err), qPrintable(err));
    QVERIFY(!doc.grids.isEmpty());
    const PageGrid& g = doc.grids[0];
    QVERIFY(!g.cells.isEmpty());
    const PageCell& cell = g.cells[0];
    const QVector<const PageGrid*> chain{&g};
    PageChrome system;
    system.thickness = 1.0;
    const PageChrome st = PageResolve::style(doc, system, chain, cell.styleId, cell.style);
    QVERIFY(st.background.has_value());
    QCOMPARE(st.background->rgb(), QColor(QStringLiteral("#FF000000")).rgb());
    QVERIFY(st.foreground.has_value());
    QCOMPARE(st.radius.value_or(0), 12.0);
    QCOMPARE(st.thickness.value_or(0), 1.0);

    PageDwell sysDwell;
    sysDwell.dwellGrace = 999;
    const PageDwell dw = PageResolve::dwell(doc, sysDwell, chain, cell.dwellId, cell.dwell);
    QCOMPARE(dw.scanGrace.value_or(-1), 50);
    QCOMPARE(dw.dwellGrace.value_or(-1), 999);
    QVERIFY(dw.activation.has_value());
    QCOMPARE(dw.activation->at(0), 800);
}

void PageLoaderTest::zoneDwellDefaultsAndOffset()
{
    const QByteArray xml = R"xml(
<Page id="p">
  <Zone id="z" anchor="Bottom" size="300,150"/>
</Page>
)xml";
    PageDocument doc;
    QString err;
    QVERIFY2(PageLoader::loadFromXml(xml, doc, &err), qPrintable(err));
    QCOMPARE(doc.zones.size(), 1);
    QCOMPARE(doc.zones[0].dwellSize.x.value, 300.0);
    QCOMPARE(doc.zones[0].dwellOffset.y.value, 0.0);
    QCOMPARE(doc.zones[0].size.y.value, 150.0);
}

void PageLoaderTest::rejectMissingPageId()
{
    PageDocument doc;
    QString err;
    QVERIFY(!PageLoader::loadFromXml(QByteArray("<Page></Page>"), doc, &err));
    QVERIFY(err.contains(QStringLiteral("id")));
}

void PageLoaderTest::rejectUnknownChild()
{
    PageDocument doc;
    QString err;
    QVERIFY(!PageLoader::loadFromXml(QByteArray("<Page id=\"p\"><Nope/></Page>"), doc, &err));
    QVERIFY(err.contains(QStringLiteral("Nope")));
}

void PageLoaderTest::rejectNonPageRoot()
{
    PageDocument doc;
    QString err;
    QVERIFY(!PageLoader::loadFromXml(QByteArray("<Keyboard></Keyboard>"), doc, &err));
    QVERIFY(err.contains(QStringLiteral("Page")));
}

void PageLoaderTest::cellDetectorClipsProgress()
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

void PageLoaderTest::loadConvertedBoards()
{
    const QStringList ids = {QStringLiteral("example_mouse"), QStringLiteral("example_assist"),
                             QStringLiteral("uw_right"),
                             QStringLiteral("main_settings_button_timing")};
    for (const QString& id : ids) {
        PageDocument doc;
        QString err;
        const QString path = QStringLiteral(GAZER_SOURCE_DIR)
                             + QStringLiteral("/resources/layouts/") + id + QStringLiteral(".xml");
        QVERIFY2(PageLoader::loadFromFile(path, doc, &err), qPrintable(err));
        QCOMPARE(doc.id, id);
        QVERIFY(!doc.grids.isEmpty());
        QVERIFY(!doc.grids[0].cells.isEmpty());
        if (id.startsWith(QLatin1String("main_settings"))) {
            QCOMPARE(doc.grids[0].anchor, PageAnchor::Top);
            QVERIFY(!doc.grids[0].style.background.has_value());
            const PageGrid* tabs = doc.findGrid(QStringLiteral("tabs"));
            QVERIFY(tabs);
            bool hasTabs = false;
            for (const PageCell& c : tabs->cells) {
                if (c.id == QLatin1String("tab_buttons")) {
                    hasTabs = true;
                    QCOMPARE(c.interactive, id != QLatin1String("main_settings_button_timing"));
                }
            }
            QVERIFY(hasTabs);
        }
    }
}

void PageLoaderTest::loadQwertyXml()
{
    PageDocument doc;
    QString err;
    const QString path =
        QStringLiteral(GAZER_SOURCE_DIR) + QStringLiteral("/resources/layouts/uw_qwerty.xml");
    QVERIFY2(PageLoader::loadFromFile(path, doc, &err), qPrintable(err));
    QCOMPARE(doc.id, QStringLiteral("uw_qwerty"));
    QCOMPARE(doc.grids.size(), 1);
    QVERIFY(doc.grids[0].cells.size() > 40);
    QVERIFY(!doc.zones.isEmpty());
    bool hasSend = false;
    for (const PageCell& c : doc.grids[0].cells) {
        for (const PageAction& a : c.actions) {
            if (a.type == PageActionType::Send) {
                hasSend = true;
            }
        }
    }
    QVERIFY(hasSend);
}

void PageLoaderTest::collectEmitsGridChrome()
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
    const QVector<PageTarget> t = PageHit::collect(doc, frame, {}, {}, {}, false, &grids);
    QCOMPARE(grids.size(), 1);
    QVERIFY(!grids[0].visual.isEmpty());
    QVERIFY(!grids[0].chrome.background.has_value());
    QVERIFY(!t.isEmpty());
    const PageTarget* cell = PageHit::at(t, grids[0].visual.center());
    QVERIFY(cell);
    QCOMPARE(cell->kind, PageTarget::Kind::Cell);
}

void PageLoaderTest::settingsPagesAnchorTop()
{
    const QStringList ids = {QStringLiteral("main_settings_button_timing"),
                             QStringLiteral("main_settings_pointer_timing"),
                             QStringLiteral("main_settings_styles"),
                             QStringLiteral("main_settings_assist"),
                             QStringLiteral("main_settings_lts"),
                             QStringLiteral("main_settings_theme")};
    for (const QString& id : ids) {
        PageDocument doc;
        QString err;
        const QString path = QStringLiteral(GAZER_SOURCE_DIR)
                             + QStringLiteral("/resources/layouts/") + id + QStringLiteral(".xml");
        QVERIFY2(PageLoader::loadFromFile(path, doc, &err), qPrintable(err));
        QCOMPARE(doc.grids[0].anchor, PageAnchor::Top);
        QVERIFY(!doc.grids[0].style.background.has_value());
    }
}

void PageLoaderTest::settingsTabsEqualWidth()
{
    const QStringList ids = {QStringLiteral("main_settings_button_timing"),
                             QStringLiteral("main_settings_pointer_timing"),
                             QStringLiteral("main_settings_styles"),
                             QStringLiteral("main_settings_assist"),
                             QStringLiteral("main_settings_lts"),
                             QStringLiteral("main_settings_theme")};
    for (const QString& id : ids) {
        PageDocument doc;
        QString err;
        const QString path = QStringLiteral(GAZER_SOURCE_DIR)
                             + QStringLiteral("/resources/layouts/") + id + QStringLiteral(".xml");
        QVERIFY2(PageLoader::loadFromFile(path, doc, &err), qPrintable(err));
        const PageGrid* tabs = doc.findGrid(QStringLiteral("tabs"));
        QVERIFY2(tabs, qPrintable(id));
        QCOMPARE(tabs->columns, 6);
        QCOMPARE(tabs->cells.size(), 6);
        for (const PageCell& c : tabs->cells) {
            QCOMPARE(c.colSpan, 1);
        }
    }
}

void PageLoaderTest::sleepKeepsContentWhenSuspended()
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
    const QVector<PageTarget> t = PageHit::collect(doc, frame, hiddenGrids, {}, {}, true);
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
    QVERIFY(sleep->dwellExempt);
    QVERIFY(sleep->interactive);
    QVERIFY(!sleep->label.isEmpty() || !sleep->icon.isEmpty());
    QVERIFY(main);
    QVERIFY(main->interactive);
    QVERIFY(main->dwellExempt);
}

void PageLoaderTest::edgeChipHidesUntilProgress()
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

void PageLoaderTest::keyboardMainOpensDrawer()
{
    PageDocument doc;
    QString err;
    const QString path = QStringLiteral(GAZER_SOURCE_DIR)
                         + QStringLiteral("/resources/layouts/example_keyboard.xml");
    QVERIFY2(PageLoader::loadFromFile(path, doc, &err), qPrintable(err));
    const PageZone* z = doc.findZone(QStringLiteral("edge_main"));
    QVERIFY(z);
    QVERIFY(z->actions.size() >= 2);
    QCOMPARE(z->actions.last().type, PageActionType::Page);
    QCOMPARE(z->actions.last().verb, PageVerb::Open);
    QCOMPARE(z->actions.last().targetKind, PageTargetKind::Grid);
    QCOMPARE(z->actions.last().targetId, QStringLiteral("drawer"));
}

void PageLoaderTest::liveEditorGridHasOpaqueChrome()
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
    g.style.radius = 8.0;
    g.style.thickness = 1.0;
    doc.grids.push_back(g);
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = frame.screen;
    QVector<PageGridPaint> grids;
    (void)PageHit::collect(doc, frame, {}, {}, {}, false, &grids);
    QCOMPARE(grids.size(), 1);
    QVERIFY(grids[0].chrome.background.has_value());
    QCOMPARE(grids[0].chrome.background->alpha(), 255);
    QCOMPARE(grids[0].visual.width(), 1400.0);
    QCOMPARE(grids[0].visual.height(), 980.0);
}

void PageLoaderTest::overlappingBoardOccludesLowerPage()
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
    QVector<PageTarget> t1 = PageHit::collect(settings, frame, {}, {}, {}, false, &g1);
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
    QVector<PageTarget> t2 = PageHit::collect(editor, frame, {}, {}, {}, false, &g2);
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

void PageLoaderTest::zoneProgressCoercedWhenOffScreen()
{
    const QRectF screen(0, 0, 1920, 1080);
    const QRectF visual(810, 1200, 300, 150);
    const QRectF dwell(810, 1300, 300, 200);
    const PageDetectorGeom g = PageDetector::zone(visual, dwell, screen);
    QVERIFY(g.progressCoerced);
    QVERIFY(g.contentOnScreen().isEmpty());
    QVERIFY(screen.contains(g.progressZone));
    QVERIFY(!screen.contains(g.visual.center()));
}

void PageLoaderTest::loadMainPage()
{
    PageDocument doc;
    QString err;
    const QString path =
        QStringLiteral(GAZER_SOURCE_DIR) + QStringLiteral("/resources/layouts/main.xml");
    QVERIFY2(PageLoader::loadFromFile(path, doc, &err), qPrintable(err));
    QCOMPARE(doc.id, QStringLiteral("main"));
    QCOMPARE(doc.master, true);
    QCOMPARE(doc.zones.size(), 2);
    QCOMPARE(doc.grids.size(), 2);
    QVERIFY(doc.findGrid(QStringLiteral("drawer")));
    QVERIFY(doc.findGrid(QStringLiteral("quit")));
    QCOMPARE(doc.findGrid(QStringLiteral("drawer"))->rootSlot, PageRootSlot::Drawer);
    QCOMPARE(doc.findGrid(QStringLiteral("quit"))->rootSlot, PageRootSlot::Quit);
    QCOMPARE(doc.findGrid(QStringLiteral("drawer"))->cells.size(), 9);
    QCOMPARE(doc.zones[0].actions[1].targetKind, PageTargetKind::Page);
    QCOMPARE(doc.zones[0].actions[1].targetId, QStringLiteral("main"));
    QVERIFY(doc.zones[1].dwellExempt);
    QVERIFY(doc.findGrid(QStringLiteral("drawer"))->cells[7].dwellExempt);
    QCOMPARE(doc.findGrid(QStringLiteral("quit"))->cells[0].interactive, false);
    QVERIFY(doc.findGrid(QStringLiteral("drawer"))->shell);
    QVERIFY(doc.findGrid(QStringLiteral("quit"))->shell);
    QVERIFY(doc.zones[0].shell);
}

void PageLoaderTest::hitMainChipOffScreen()
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
    const PageTarget* hit = PageHit::at(t, QPointF(960, 1200));
    QVERIFY(hit);
    QCOMPARE(hit->kind, PageTarget::Kind::Zone);
    QCOMPARE(hit->id, QStringLiteral("mainChip"));
    QVERIFY(hit->geom.progressZone.intersects(frame.screen));
    QVERIFY(!frame.screen.contains(hit->geom.dwellZone.center()));
}

void PageLoaderTest::engagedZoneIncludesProgress()
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
    const PageTarget* chip = PageHit::at(t, QPointF(960, 1200));
    QVERIFY(chip);
    const QPointF onStrip = chip->geom.progressZone.center();
    QVERIFY(!chip->geom.dwellZone.contains(onStrip));
    QVERIFY(PageHit::at(t, onStrip) == nullptr);
    const PageTarget* held = PageHit::at(t, onStrip, 1.0, chip->id);
    QVERIFY(held);
    QCOMPARE(held->id, chip->id);
}

void PageLoaderTest::hitDrawerCell()
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

void PageLoaderTest::visibleWhenHidesMainChip()
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
    const QVector<PageTarget> t = PageHit::collect(doc, frame, hiddenGrids, {}, expanded, false);
    QVERIFY(PageHit::at(t, QPointF(960, 1200)) == nullptr);
    const PageTarget* sleep = PageHit::at(t, QPointF(960 + 400, 1200));
    QVERIFY(sleep);
    QCOMPARE(sleep->id, QStringLiteral("sleep"));
}

void PageLoaderTest::drawerMapIsIdentityAtFullScale()
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

void PageLoaderTest::drawerMapShrinksAboutBottom()
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

void PageLoaderTest::cellRectSpan()
{
    PageGrid g;
    g.rows = 2;
    g.columns = 2;
    g.gapPx = 0;
    g.marginPx = 0;
    const QRectF a = PageHit::cellRect(g, QRectF(0, 0, 200, 200), 0, 0, 1, 2);
    QCOMPARE(a, QRectF(0, 0, 200, 100));
}

void PageLoaderTest::zoneDetectorUnrestrictedDwell()
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
}

QTEST_MAIN(PageLoaderTest)
#include "PageLoaderTest.moc"
