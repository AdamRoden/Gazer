#include "assist/LtsSpeed.h"
#include "layout/PageCatalog.h"
#include "layout/PageDim.h"
#include "layout/PageHit.h"
#include "layout/PageLoader.h"
#include "layout/PageResolve.h"
#include "layout/PageWriter.h"
#include "ui/ProgressVisuals.h"

#include <QDir>
#include <QFile>
#include <QStringList>
#include <QTemporaryDir>
#include <QtTest>

using namespace gazer;

class PageLoaderTest final : public QObject {
    Q_OBJECT

private slots:
    void dimPixelsVsProportion();
    void dimFraction();
    void dimHeightRelative();
    void placeRectBottom();
    void placeRectHeightSquareAtPoint();
    void ltsSpeedLadder();
    void parseRowWeightsCsv();
    void loadFixture();
    void inheritStyleAndDwell();
    void zoneDwellDefaultsAndOffset();
    void rejectMissingPageId();
    void rejectUnknownChild();
    void rejectNonPageRoot();
    void rejectRemovedActionAliases();
    void loadMainPage();
    void loadQwertyXml();
    void loadConvertedBoards();
    void loadLtsMenu();
    void keyboardMainOpensDrawer();
    void pageWriterRoundTripMain();
    void rowWeightsRoundTrip();
    void actionExtrasRoundTrip();
    void actionEmptyEdgeRoundTrip();
    void parseClickDownAsEdge();
    void parseSendDurationWithoutEdge();
    void rejectNonIntegerClickCount();
    void sessionKeyPrefixedAfterPageId();
    void catalogUserCopyWinsPath();
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

void PageLoaderTest::dimHeightRelative()
{
    const PageDim h = PageDimParse::parse(QStringLiteral("0.25h"));
    QCOMPARE(h.unit, PageDim::Unit::HeightProportion);
    QCOMPARE(h.value, 0.25);
    QCOMPARE(h.resolve(1920, 1080), 270.0);
    const PageDim frac = PageDimParse::parse(QStringLiteral("1/4h"));
    QCOMPARE(frac.unit, PageDim::Unit::HeightProportion);
    QCOMPARE(frac.resolve(800, 400), 100.0);
    QString err;
    QVERIFY(PageDimParse::parse(QStringLiteral("150h"), &err).unit == PageDim::Unit::Unset);
    QVERIFY(!err.isEmpty());
}

void PageLoaderTest::placeRectHeightSquareAtPoint()
{
    const QRectF desk(0, 0, 1920, 1040);
    PageDimPair size;
    size.x = PageDimParse::parse(QStringLiteral("0.25h"));
    size.y = PageDimParse::parse(QStringLiteral("0.25h"));
    const QPointF origin(1000, 500);
    PageDimPair offset;
    offset.x = PageDim::pixels(origin.x() - desk.center().x());
    offset.y = PageDim::pixels(origin.y() - desk.center().y());
    const QRectF r = PageDimParse::placeRect(desk, PageAnchor::Center, offset, size);
    QCOMPARE(r.width(), 260.0);
    QCOMPARE(r.height(), 260.0);
    QCOMPARE(r.center().x(), origin.x());
    QCOMPARE(r.center().y(), origin.y());
}

void PageLoaderTest::ltsSpeedLadder()
{
    QCOMPARE(snapLtsSpeed(4.4), 5.0);
    QCOMPARE(snapLtsSpeed(2.0), 1.0);
    QCOMPARE(snapLtsSpeed(4.0), 5.0);
    QCOMPARE(snapLtsSpeed(1.0), 1.0);
    QCOMPARE(nudgeLtsSpeed(5.0, +1), 10.0);
    QCOMPARE(nudgeLtsSpeed(5.0, -1), 1.0);
    QCOMPARE(nudgeLtsSpeed(1.0, -1), 1.0);
    QCOMPARE(nudgeLtsSpeed(50.0, +1), 50.0);
    QCOMPARE(nudgeLtsSpeed(20.0, +1), 50.0);
}

void PageLoaderTest::parseRowWeightsCsv()
{
    QCOMPARE(parseRowWeights(QStringLiteral("1,2,2,2")), QVector<double>({1.0, 2.0, 2.0, 2.0}));
    QCOMPARE(parseRowWeights(QStringLiteral(" 0, -1, 3 ")), QVector<double>({3.0}));
    QVERIFY(parseRowWeights(QStringLiteral("")).isEmpty());
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
    system.thickness = PageBox::all(1.0);
    const PageChrome st = PageResolve::style(doc, system, chain, cell.styleId, cell.style);
    QVERIFY(st.background.has_value());
    QCOMPARE(st.background->rgb(), QColor(QStringLiteral("#FF000000")).rgb());
    QVERIFY(st.foreground.has_value());
    QCOMPARE(st.radius ? st.radius->first() : 0.0, 12.0);
    QCOMPARE(st.thickness ? st.thickness->first() : 0.0, 1.0);
    QVERIFY(!st.progressStyle.has_value());

    PageChrome sides;
    sides.thickness = PageBox::fromToken(QStringLiteral("1,2,3,4"));
    sides.radius = PageBox::fromToken(QStringLiteral("8,4"));
    QCOMPARE(sides.thickness->at(0), 1.0);
    QCOMPARE(sides.thickness->at(1), 2.0);
    QCOMPARE(sides.thickness->at(2), 3.0);
    QCOMPARE(sides.thickness->at(3), 4.0);
    QCOMPARE(sides.radius->at(0), 8.0);
    QCOMPARE(sides.radius->at(1), 4.0);
    QCOMPARE(sides.radius->at(2), 8.0);
    QCOMPARE(sides.radius->at(3), 4.0);
    QCOMPARE(PageBox::fromToken(QStringLiteral("5")).toToken(), QStringLiteral("5"));

    const QByteArray styleXml = R"xml(
<Page id="p">
  <Style id="chip" radius="0,0,12,12" progressStyle="fillup,border"/>
  <Zone id="z" style="chip" size="100,40"/>
</Page>
)xml";
    PageDocument styled;
    QVERIFY2(PageLoader::loadFromXml(styleXml, styled, &err), qPrintable(err));
    const PageChrome named = styled.styles.value(QStringLiteral("chip"));
    QVERIFY(named.progressStyle.has_value());
    QCOMPARE(named.progressStyle->toCsv(), QStringLiteral("fillup,border"));
    QCOMPARE(named.radius ? named.radius->at(0) : -1.0, 0.0);
    QCOMPARE(named.radius ? named.radius->at(1) : -1.0, 0.0);
    QCOMPARE(named.radius ? named.radius->at(2) : -1.0, 12.0);
    QCOMPARE(named.radius ? named.radius->at(3) : -1.0, 12.0);
    QVERIFY(!styled.zones.isEmpty());
    const PageChrome zst =
        PageResolve::zoneStyle(styled, PageChrome{}, styled.zones[0]);
    QVERIFY(zst.progressStyle.has_value());
    QCOMPARE(zst.progressStyle->toCsv(), QStringLiteral("fillup,border"));
    QCOMPARE(zst.radius ? zst.radius->at(2) : -1.0, 12.0);

    PageDocument written;
    QVERIFY2(PageLoader::loadFromXml(PageWriter::toBytes(styled), written, &err), qPrintable(err));
    QCOMPARE(written.styles.value(QStringLiteral("chip")).progressStyle->toCsv(),
             QStringLiteral("fillup,border"));
    QCOMPARE(written.styles.value(QStringLiteral("chip")).radius->toToken(),
             QStringLiteral("0,0,12,12"));

    ProgressVisuals pv;
    pv.setStylesFromCsv(QStringLiteral("fillup,border"));
    QVERIFY(pv.style.fillBackground);
    QVERIFY(pv.style.border);
    QVERIFY(!pv.style.radial);
    QCOMPARE(pv.style.fillDir, ProgressFillDir::Up);
    QCOMPARE(pv.stylesCsv(), QStringLiteral("fillup,border"));
    pv.setStylesFromCsv(QStringLiteral("fillleft"));
    QCOMPARE(pv.style.fillDir, ProgressFillDir::Left);
    QVERIFY(!pv.style.border);

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

void PageLoaderTest::rejectRemovedActionAliases()
{
    const QStringList ids = {QStringLiteral("MouseClick"), QStringLiteral("MouseMove"),
                             QStringLiteral("MouseMoveAndClick")};
    for (const QString& id : ids) {
        const QByteArray xml =
            QByteArray("<Page id=\"p\"><Grid id=\"g\"><Cell id=\"c\">"
                       "<Action id=\"")
            + id.toUtf8() + QByteArray("\" value=\"left\"/></Cell></Grid></Page>");
        PageDocument doc;
        QString err;
        QVERIFY2(!PageLoader::loadFromXml(xml, doc, &err), qPrintable(id));
        QVERIFY2(err.contains(QStringLiteral("Unknown Action id")), qPrintable(err));
    }
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

void PageLoaderTest::loadLtsMenu()
{
    PageDocument doc;
    QString err;
    const QString path = QStringLiteral(GAZER_SOURCE_DIR)
                         + QStringLiteral("/resources/layouts/lts_menu.xml");
    QVERIFY2(PageLoader::loadFromFile(path, doc, &err), qPrintable(err));
    QCOMPARE(doc.id, QStringLiteral("lts_menu"));
    QCOMPARE(doc.grids.size(), 1);
    QCOMPARE(doc.grids[0].rows, 3);
    QCOMPARE(doc.grids[0].columns, 3);
    QCOMPARE(doc.grids[0].gapPx, 0);
    QCOMPARE(doc.grids[0].size.x.unit, PageDim::Unit::HeightProportion);
    QCOMPARE(doc.grids[0].size.y.unit, PageDim::Unit::HeightProportion);
    QCOMPARE(doc.grids[0].size.x.value, 0.25);
    QCOMPARE(doc.grids[0].cells.size(), 5);
    QVERIFY(doc.grids[0].style.background.has_value());
    QCOMPARE(doc.grids[0].style.background->alpha(), 0);
    QVERIFY(doc.grids[0].style.thickness.has_value());
    QCOMPARE(doc.grids[0].style.thickness->first(), 0.0);
    QVERIFY(doc.styles.contains(QStringLiteral("hub")));
    QVERIFY(doc.styles.contains(QStringLiteral("slow")));
    QVERIFY(doc.styles.contains(QStringLiteral("fast")));
    QVERIFY(doc.styles.contains(QStringLiteral("quit")));
    QVERIFY(doc.styles.contains(QStringLiteral("reset")));
    QCOMPARE(doc.styles.value(QStringLiteral("hub")).radius->toToken(), QStringLiteral("0"));
    QCOMPARE(doc.styles.value(QStringLiteral("reset")).radius->toToken(),
             QStringLiteral("900,900,0,0"));
    QCOMPARE(doc.styles.value(QStringLiteral("fast")).radius->toToken(),
             QStringLiteral("0,900,900,0"));
    QCOMPARE(doc.styles.value(QStringLiteral("quit")).radius->toToken(),
             QStringLiteral("0,0,900,900"));
    QCOMPARE(doc.styles.value(QStringLiteral("slow")).radius->toToken(),
             QStringLiteral("900,0,0,900"));
    QCOMPARE(doc.styles.value(QStringLiteral("hub")).blur.value_or(-1.0), 15.0);
    QCOMPARE(doc.styles.value(QStringLiteral("slow")).blur.value_or(-1.0), 15.0);
    QCOMPARE(doc.styles.value(QStringLiteral("fast")).blur.value_or(-1.0), 15.0);
    QCOMPARE(doc.styles.value(QStringLiteral("quit")).blur.value_or(-1.0), 15.0);
    QCOMPARE(doc.styles.value(QStringLiteral("reset")).blur.value_or(-1.0), 15.0);

    PageDocument written;
    QVERIFY2(PageLoader::loadFromXml(PageWriter::toBytes(doc), written, &err), qPrintable(err));
    QCOMPARE(written.grids[0].size.x.unit, PageDim::Unit::HeightProportion);
    QCOMPARE(written.grids[0].size.x.value, 0.25);
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
    QCOMPARE(doc.findGrid(QStringLiteral("drawer"))->cells.size(), 10);
    QCOMPARE(doc.zones[0].actions[1].targetKind, PageTargetKind::Page);
    QCOMPARE(doc.zones[0].actions[1].targetId, QStringLiteral("main"));
    QVERIFY(doc.zones[1].dwellExempt);
    const PageCell* editor = nullptr;
    const PageCell* pause = nullptr;
    for (const PageCell& c : doc.findGrid(QStringLiteral("drawer"))->cells) {
        if (c.id == QLatin1String("open_editor")) {
            editor = &c;
        }
        if (c.id == QLatin1String("dwell_suspend")) {
            pause = &c;
        }
    }
    QVERIFY(editor);
    QCOMPARE(editor->actions[0].type, PageActionType::Command);
    QCOMPARE(editor->actions[0].command, QStringLiteral("openLayoutEditor"));
    QVERIFY(pause);
    QVERIFY(pause->dwellExempt);
    QCOMPARE(doc.findGrid(QStringLiteral("quit"))->cells[0].interactive, false);
    QVERIFY(doc.findGrid(QStringLiteral("drawer"))->shell);
    QVERIFY(doc.findGrid(QStringLiteral("quit"))->shell);
    QVERIFY(doc.zones[0].shell);
}

void PageLoaderTest::pageWriterRoundTripMain()
{
    PageDocument src;
    QString err;
    const QString path =
        QStringLiteral(GAZER_SOURCE_DIR) + QStringLiteral("/resources/layouts/main.xml");
    QVERIFY2(PageLoader::loadFromFile(path, src, &err), qPrintable(err));
    const QByteArray xml = PageWriter::toBytes(src);
    PageDocument dst;
    QVERIFY2(PageLoader::loadFromXml(xml, dst, &err), qPrintable(err));
    QCOMPARE(dst.id, src.id);
    QCOMPARE(dst.master, src.master);
    QCOMPARE(dst.grids.size(), src.grids.size());
    QCOMPARE(dst.zones.size(), src.zones.size());
    QCOMPARE(dst.findZone(QStringLiteral("mainChip")) != nullptr, true);
    QVERIFY(!dst.findZone(QStringLiteral("mainChip"))->aboveTaskbar);
    QCOMPARE(dst.findGrid(QStringLiteral("drawer")) != nullptr, true);
}

void PageLoaderTest::rowWeightsRoundTrip()
{
    PageDocument src;
    src.id = QStringLiteral("p");
    PageGrid g;
    g.id = QStringLiteral("g");
    g.rows = 3;
    g.columns = 1;
    g.rowWeights = {1.0, 2.0, 2.0};
    src.grids.push_back(g);
    QString err;
    PageDocument dst;
    QVERIFY2(PageLoader::loadFromXml(PageWriter::toBytes(src), dst, &err), qPrintable(err));
    QCOMPARE(dst.grids[0].rowWeights, QVector<double>({1.0, 2.0, 2.0}));
}

void PageLoaderTest::actionExtrasRoundTrip()
{
    PageDocument doc;
    doc.id = QStringLiteral("t");
    PageGrid g;
    g.id = QStringLiteral("g");
    PageCell c;
    c.id = QStringLiteral("c");
    c.shell = true;
    c.styleId = QStringLiteral("chip");
    PageAction send;
    send.type = PageActionType::Send;
    send.sendKey = QStringLiteral("a");
    send.sendEdge = QStringLiteral("Down");
    send.sendDurationMs = 40;
    PageAction mv;
    mv.type = PageActionType::Move;
    mv.moveMode = PageMoveMode::Absolute;
    mv.moveX = PageDim::pixels(10);
    mv.moveY = PageDim::pixels(20);
    mv.speed = 5;
    mv.zoomLevel = 2;
    PageAction mac;
    mac.type = PageActionType::MoveAndClick;
    mac.button = QStringLiteral("right");
    mac.clickCount = 2;
    mac.clickEdge = QStringLiteral("Up");
    mac.speed = 3;
    mac.zoomLevel = 1;
    PageAction cmd;
    cmd.type = PageActionType::Command;
    cmd.command = QStringLiteral("quitApp");
    cmd.args = QStringLiteral("now");
    c.actions = {send, mv, mac, cmd};
    g.cells.push_back(c);
    doc.grids.push_back(g);
    doc.styles.insert(QStringLiteral("chip"), PageChrome{});

    QString err;
    PageDocument out;
    QVERIFY2(PageLoader::loadFromXml(PageWriter::toBytes(doc), out, &err), qPrintable(err));
    const PageGrid* grid = out.findGrid(QStringLiteral("g"));
    QVERIFY(grid);
    QCOMPARE(grid->cells.size(), 1);
    QCOMPARE(grid->cells[0].shell, true);
    QCOMPARE(grid->cells[0].styleId, QStringLiteral("chip"));
    QCOMPARE(grid->cells[0].actions.size(), 4);
    QCOMPARE(grid->cells[0].actions[0].sendDurationMs, 40);
    QCOMPARE(grid->cells[0].actions[0].sendEdge, QStringLiteral("Down"));
    QCOMPARE(grid->cells[0].actions[1].moveMode, PageMoveMode::Absolute);
    QCOMPARE(int(grid->cells[0].actions[1].moveX.value), 10);
    QCOMPARE(grid->cells[0].actions[1].speed, 5);
    QCOMPARE(grid->cells[0].actions[1].zoomLevel, 2);
    QCOMPARE(grid->cells[0].actions[2].clickCount, 2);
    QCOMPARE(grid->cells[0].actions[2].zoomLevel, 1);
    QCOMPARE(grid->cells[0].actions[3].command, QStringLiteral("quitApp"));
    QCOMPARE(grid->cells[0].actions[3].args, QStringLiteral("now"));
}

namespace {

bool loadOneAction(const QByteArray& actionXml, PageAction& out, QString* err)
{
    const QByteArray xml = QByteArray("<Page id=\"p\"><Grid id=\"g\"><Cell id=\"c\">") + actionXml
                           + QByteArray("</Cell></Grid></Page>");
    PageDocument doc;
    if (!PageLoader::loadFromXml(xml, doc, err)) {
        return false;
    }
    if (doc.grids.isEmpty() || doc.grids[0].cells.isEmpty()
        || doc.grids[0].cells[0].actions.isEmpty()) {
        if (err) {
            *err = QStringLiteral("no action");
        }
        return false;
    }
    out = doc.grids[0].cells[0].actions[0];
    return true;
}

} // namespace

void PageLoaderTest::actionEmptyEdgeRoundTrip()
{
    PageDocument doc;
    doc.id = QStringLiteral("t");
    PageGrid g;
    g.id = QStringLiteral("g");
    PageCell c;
    c.id = QStringLiteral("c");
    PageAction send;
    send.type = PageActionType::Send;
    send.sendKey = QStringLiteral("Enter");
    send.sendDurationMs = 40;
    PageAction click;
    click.type = PageActionType::Click;
    click.button = QStringLiteral("left");
    click.speed = 5;
    PageAction mac;
    mac.type = PageActionType::MoveAndClick;
    mac.button = QStringLiteral("right");
    mac.zoomLevel = 2;
    c.actions = {send, click, mac};
    g.cells.push_back(c);
    doc.grids.push_back(g);

    QString err;
    PageDocument out;
    QVERIFY2(PageLoader::loadFromXml(PageWriter::toBytes(doc), out, &err), qPrintable(err));
    QCOMPARE(out.grids[0].cells[0].actions.size(), 3);
    QCOMPARE(out.grids[0].cells[0].actions[0].sendKey, QStringLiteral("Enter"));
    QCOMPARE(out.grids[0].cells[0].actions[0].sendEdge, QString());
    QCOMPARE(out.grids[0].cells[0].actions[0].sendDurationMs, 40);
    QCOMPARE(out.grids[0].cells[0].actions[1].clickCount, 1);
    QCOMPARE(out.grids[0].cells[0].actions[1].clickEdge, QString());
    QCOMPARE(out.grids[0].cells[0].actions[1].speed, 5);
    QCOMPARE(out.grids[0].cells[0].actions[2].clickCount, 1);
    QCOMPARE(out.grids[0].cells[0].actions[2].zoomLevel, 2);
}

void PageLoaderTest::parseClickDownAsEdge()
{
    PageAction a;
    QString err;
    QVERIFY2(loadOneAction(QByteArray("<Action id=\"Click\" value=\"left, Down\"/>"), a, &err),
             qPrintable(err));
    QCOMPARE(a.type, PageActionType::Click);
    QCOMPARE(a.button, QStringLiteral("left"));
    QCOMPARE(a.clickCount, 1);
    QCOMPARE(a.clickEdge.toLower(), QStringLiteral("down"));
}

void PageLoaderTest::parseSendDurationWithoutEdge()
{
    PageAction a;
    QString err;
    QVERIFY2(loadOneAction(QByteArray("<Action id=\"Send\" value=\"Enter, 40\"/>"), a, &err),
             qPrintable(err));
    QCOMPARE(a.sendKey, QStringLiteral("Enter"));
    QCOMPARE(a.sendEdge, QString());
    QCOMPARE(a.sendDurationMs, 40);
}

void PageLoaderTest::rejectNonIntegerClickCount()
{
    PageAction a;
    QString err;
    QVERIFY(!loadOneAction(QByteArray("<Action id=\"Click\" value=\"left, foo\"/>"), a, &err));
    QVERIFY(err.contains(QStringLiteral("integer")));
}

void PageLoaderTest::sessionKeyPrefixedAfterPageId()
{
    PageTarget t;
    t.pageId = QStringLiteral("main");
    t.id = QStringLiteral("sleep");
    QCOMPARE(sessionKey(t), QStringLiteral("main/sleep"));
    t.pageId.clear();
    QCOMPARE(sessionKey(t), QStringLiteral("sleep"));
}

void PageLoaderTest::catalogUserCopyWinsPath()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString shipped = tmp.path() + QStringLiteral("/shipped");
    const QString user = tmp.path() + QStringLiteral("/user");
    QVERIFY(QDir().mkpath(shipped));
    QVERIFY(QDir().mkpath(user));
    auto write = [](const QString& path, const QString& body) {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return false;
        }
        return f.write(body.toUtf8()) > 0;
    };
    QVERIFY(write(shipped + QStringLiteral("/kb.xml"),
                  QStringLiteral("<Page id=\"kb\" name=\"Shipped\"><Grid id=\"g\"/></Page>")));
    QCOMPARE(PageCatalog::resolvePath(QStringLiteral("kb"), user, shipped),
             QDir(shipped).filePath(QStringLiteral("kb.xml")));
    QVERIFY(write(user + QStringLiteral("/kb.xml"),
                  QStringLiteral("<Page id=\"kb\" name=\"User\"><Grid id=\"g\"/></Page>")));
    QCOMPARE(PageCatalog::resolvePath(QStringLiteral("kb"), user, shipped),
             QDir(user).filePath(QStringLiteral("kb.xml")));
}

QObject* createPageLoaderTest()
{
    return new PageLoaderTest;
}

#include "PageLoaderTest.moc"
