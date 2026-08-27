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
    void expressionSizeRoundTrip();
    void ltsSpeedLadder();
    void parseRowWeightsCsv();
    void loadFixture();
    void ahkCdataRoundTrip();
    void inheritStyleAndDwell();
    void pageChromeDefaults();
    void zoneDwellDefaultsAndOffset();
    void rejectMissingPageId();
    void rejectUnknownChild();
    void rejectNonPageRoot();
    void rejectRemovedActionAliases();
    void loadMainPage();
    void loadQwertyXml();
    void showAttribute();
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
    void cellSendAttribute();
    void rejectTwoActionAttributes();
    void parseSpecificActionElements();
    void parseMoveVariants();
    void parseOpenPageBreadcrumb();
    void parseCloseSpecialsAndGoBack();
    void genericActionAttribute();
    void sessionKeyPrefixedAfterPageId();
    void catalogUserCopyWinsPath();
};

void PageLoaderTest::expressionSizeRoundTrip()
{
    const QByteArray xml = R"xml(
<Page id="p">
  <Grid id="g" size="A_ScreenHeight/9*16, A_ScreenHeight">
    <Cell id="c" label="X"/>
  </Grid>
</Page>
)xml";
    PageDocument doc;
    QString err;
    QVERIFY2(PageLoader::loadFromXml(xml, doc, &err), qPrintable(err));
    QCOMPARE(doc.grids[0].size.x.unit, PageDim::Unit::Expression);
    QCOMPARE(doc.grids[0].size.y.unit, PageDim::Unit::Expression);
    PageDocument written;
    QVERIFY2(PageLoader::loadFromXml(PageWriter::toBytes(doc), written, &err), qPrintable(err));
    QCOMPARE(PageDimParse::token(written.grids[0].size.x), QStringLiteral("A_ScreenHeight/9*16"));
    QCOMPARE(PageDimParse::token(written.grids[0].size.y), QStringLiteral("A_ScreenHeight"));
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
    QCOMPARE(g->cells[0].actions[0].type, PageActionType::Nav);
    QCOMPARE(g->cells[0].actions[0].verb, PageVerb::Open);
    QCOMPARE(g->cells[0].actions[0].targetKind, PageTargetKind::Page);
    QCOMPARE(g->cells[0].actions[0].targetScope, PageNavScope::Id);
    QCOMPARE(g->cells[0].actions[0].targetId, QStringLiteral("uw_qwerty"));
    QCOMPARE(g->cells[0].actions[1].type, PageActionType::Send);
    QCOMPARE(g->cells[0].actions[1].sendKey, QStringLiteral("Enter"));
    QCOMPARE(g->cells[0].actions[1].sendEdge, QStringLiteral("Down"));
    QCOMPARE(g->cells[0].actions[1].sendDurationMs, 50);

    const PageZone* z = doc.findZone(QStringLiteral("edge"));
    QVERIFY(z);
    QCOMPARE(z->suspendExempt, true);
    QCOMPARE(z->dwellOffset.y.value, 240.0);
    QCOMPARE(z->dwellSize.x.value, 300.0);
    QCOMPARE(z->actions[0].targetKind, PageTargetKind::Grid);
    QCOMPARE(z->actions[0].targetId, QStringLiteral("Quick Settings"));

    QVERIFY(doc.styles.contains(QStringLiteral("stl")));
    QVERIFY(doc.dwells.contains(QStringLiteral("dwl")));
    QCOMPARE(doc.dwells.value(QStringLiteral("dwl")).activation->size(), 9);
    QCOMPARE(doc.dwells.value(QStringLiteral("dwl")).activation->at(0), 0);
}

void PageLoaderTest::ahkCdataRoundTrip()
{
    PageDocument doc;
    QString err;
    const QString fixture =
        QStringLiteral(GAZER_TEST_FIXTURES) + QStringLiteral("/example_page.xml");
    QVERIFY2(PageLoader::loadFromFile(fixture, doc, &err), qPrintable(err));
    const QByteArray written = PageWriter::toBytes(doc);
    QVERIFY(QString::fromUtf8(written).contains(QStringLiteral("<![CDATA[")));
    PageDocument round;
    QVERIFY2(PageLoader::loadFromXml(written, round, &err), qPrintable(err));
    const PageGrid* g = round.findGrid(QStringLiteral("Quick Settings"));
    QVERIFY(g);
    QVERIFY(!g->subGrids.isEmpty());
    QVERIFY(!g->subGrids[0].cells.isEmpty());
    QCOMPARE(g->subGrids[0].cells[0].actions[0].type, PageActionType::Ahk);
    QVERIFY(g->subGrids[0].cells[0].actions[0].ahkSource.contains(QStringLiteral("LAlt")));
}

void PageLoaderTest::pageChromeDefaults()
{
    const QByteArray xml = R"xml(
<Page id="p">
  <Grid id="g" size="100,100">
    <Cell id="c" label="X"/>
  </Grid>
  <Zone id="z" size="80,40"/>
</Page>
)xml";
    PageDocument doc;
    QString err;
    QVERIFY2(PageLoader::loadFromXml(xml, doc, &err), qPrintable(err));
    QVERIFY(!doc.grids.isEmpty());
    QVERIFY(!doc.grids[0].style.thickness.has_value());
    QVERIFY(!doc.grids[0].style.radius.has_value());
    const PageChrome grid = PageResolve::style(doc, doc.grids[0].styleId, doc.grids[0].style);
    QCOMPARE(grid.resolvedThickness().first(), PageChrome::kDefaultThickness);
    QCOMPARE(grid.resolvedRadius().first(), PageChrome::kDefaultRadius);
    QVERIFY(!grid.thickness.has_value());
    QVERIFY(!grid.radius.has_value());
    const PageChrome cell =
        PageResolve::style(doc, doc.grids[0].cells[0].styleId, doc.grids[0].cells[0].style);
    QCOMPARE(cell.resolvedThickness().first(), PageChrome::kDefaultThickness);
    QCOMPARE(cell.resolvedRadius().first(), PageChrome::kDefaultRadius);
    QCOMPARE(cell.resolvedRadius(true).first(), PageChrome::kClusteredRadius);
    QVERIFY(!cell.thickness.has_value());
    QVERIFY(!doc.zones.isEmpty());
    const PageChrome zone = PageResolve::zoneStyle(doc, doc.zones[0]);
    QCOMPARE(zone.resolvedThickness().first(), PageChrome::kDefaultThickness);
    QCOMPARE(zone.resolvedRadius().first(), PageChrome::kDefaultRadius);
    QVERIFY(!zone.radius.has_value());
}

void PageLoaderTest::inheritStyleAndDwell()
{
    const QByteArray xml = R"xml(
<Page id="p" background="#FF000000" radius="4" scanGrace="300" activation="800">
  <Style id="pageStl" background="#111111" radius="2"/>
  <Style id="cellStl" foreground="#FFFFFFFF"/>
  <Dwell id="slow" scanGrace="999" activation="50"/>
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
    const PageChrome st = PageResolve::style(doc, cell.styleId, cell.style);
    QVERIFY(st.background.has_value());
    QCOMPARE(st.background->rgb(), QColor(QStringLiteral("#FF000000")).rgb());
    QVERIFY(st.foreground.has_value());
    QCOMPARE(st.radius ? st.radius->first() : 0.0, 12.0);
    QVERIFY(!st.thickness.has_value());
    QCOMPARE(st.resolvedThickness().first(), PageChrome::kDefaultThickness);
    QVERIFY(!st.progressStyle.has_value());
    const PageChrome gridSt = PageResolve::style(doc, g.styleId, g.style);
    QCOMPARE(gridSt.background->rgb(), QColor(QStringLiteral("#111111")).rgb());
    QCOMPARE(gridSt.radius ? gridSt.radius->first() : 0.0, 2.0);

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
  <Style id="chip" radius="0,0,12,12" progressStyle="fillup,border" progressColor="#00DCFF"/>
  <Zone id="z" style="chip" size="100,40"/>
</Page>
)xml";
    PageDocument styled;
    QVERIFY2(PageLoader::loadFromXml(styleXml, styled, &err), qPrintable(err));
    const PageChrome named = styled.styles.value(QStringLiteral("chip"));
    QVERIFY(named.progressStyle.has_value());
    QCOMPARE(named.progressStyle->toCsv(), QStringLiteral("fillup,border"));
    QVERIFY(named.progressColor.has_value());
    QCOMPARE(named.progressColor->name(QColor::HexRgb).toUpper(), QStringLiteral("#00DCFF"));
    QCOMPARE(named.radius ? named.radius->at(0) : -1.0, 0.0);
    QCOMPARE(named.radius ? named.radius->at(1) : -1.0, 0.0);
    QCOMPARE(named.radius ? named.radius->at(2) : -1.0, 12.0);
    QCOMPARE(named.radius ? named.radius->at(3) : -1.0, 12.0);
    QVERIFY(!styled.zones.isEmpty());
    const PageChrome zst = PageResolve::zoneStyle(styled, styled.zones[0]);
    QVERIFY(zst.progressStyle.has_value());
    QCOMPARE(zst.progressStyle->toCsv(), QStringLiteral("fillup,border"));
    QVERIFY(zst.progressColor.has_value());
    QCOMPARE(zst.progressColor->name(QColor::HexRgb).toUpper(), QStringLiteral("#00DCFF"));
    QCOMPARE(zst.radius ? zst.radius->at(2) : -1.0, 12.0);

    PageDocument written;
    QVERIFY2(PageLoader::loadFromXml(PageWriter::toBytes(styled), written, &err), qPrintable(err));
    QCOMPARE(written.styles.value(QStringLiteral("chip")).progressStyle->toCsv(),
             QStringLiteral("fillup,border"));
    QCOMPARE(written.styles.value(QStringLiteral("chip")).progressColor->name(QColor::HexRgb).toUpper(),
             QStringLiteral("#00DCFF"));
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

    const PageDwell dw = PageResolve::dwell(doc, cell.dwellId, cell.dwell);
    QCOMPARE(dw.scanGrace.value_or(-1), 50);
    QVERIFY(!dw.dwellGrace.has_value());
    QVERIFY(dw.activation.has_value());
    QCOMPARE(dw.activation->at(0), 800);
    const PageDwell gridDw = PageResolve::dwell(doc, g.dwellId, g.dwell);
    QCOMPARE(gridDw.scanGrace.value_or(-1), 999);
    QCOMPARE(gridDw.activation->at(0), 50);
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
    QCOMPARE(doc.grids.size(), 3);
    const PageGrid* board = doc.findGrid(QStringLiteral("board"));
    const PageGrid* vert1 = doc.findGrid(QStringLiteral("vert1"));
    const PageGrid* vert2 = doc.findGrid(QStringLiteral("vert2"));
    QVERIFY(board);
    QVERIFY(vert1);
    QVERIFY(vert2);
    QVERIFY(board->cells.size() > 40);
    QCOMPARE(board->show, true);
    QCOMPARE(vert1->show, true);
    QCOMPARE(vert2->show, false);
    QVERIFY(doc.zones.isEmpty());
    bool hasSend = false;
    for (const PageCell& c : board->cells) {
        for (const PageAction& a : c.actions) {
            if (a.type == PageActionType::Send) {
                hasSend = true;
            }
        }
    }
    QVERIFY(hasSend);
    QCOMPARE(vert1->cells[1].actions.size(), 1);
    QCOMPARE(vert1->cells[1].actions[0].type, PageActionType::MoveAndClick);
    QCOMPARE(vert1->cells[1].actions[0].button, QStringLiteral("left"));
    QCOMPARE(vert1->cells[1].actions[0].zoomMode, PageZoomMode::Settings);
}

void PageLoaderTest::showAttribute()
{
    PageDocument doc;
    QString err;
    const QByteArray xml = R"xml(
<Page id="p">
  <Grid id="shown" size="100,100">
    <Cell id="cshow"/>
    <Cell id="chide" show="false"/>
    <Cell id="cvis" visible="false"/>
    <Cell id="cexem" dwellExempt="true"/>
    <Cell id="csus" suspendExempt="true"/>
  </Grid>
  <Grid id="hidden" size="100,100" show="false"/>
  <Grid id="legacy" size="100,100" open="false"/>
  <Zone id="zshow" size="80,40"/>
  <Zone id="zhide" size="80,40" show="false"/>
  <Zone id="zvis" size="80,40" visible="false"/>
</Page>
)xml";
    QVERIFY2(PageLoader::loadFromXml(xml, doc, &err), qPrintable(err));
    QCOMPARE(doc.findGrid(QStringLiteral("shown"))->show, true);
    QCOMPARE(doc.findGrid(QStringLiteral("hidden"))->show, false);
    QCOMPARE(doc.findGrid(QStringLiteral("legacy"))->show, false);
    QCOMPARE(doc.findCell(QStringLiteral("cshow"))->show, true);
    QCOMPARE(doc.findCell(QStringLiteral("chide"))->show, false);
    QCOMPARE(doc.findCell(QStringLiteral("cvis"))->show, false);
    QCOMPARE(doc.findCell(QStringLiteral("cexem"))->suspendExempt, true);
    QCOMPARE(doc.findCell(QStringLiteral("csus"))->suspendExempt, true);
    QCOMPARE(doc.findZone(QStringLiteral("zshow"))->show, true);
    QCOMPARE(doc.findZone(QStringLiteral("zhide"))->show, false);
    QCOMPARE(doc.findZone(QStringLiteral("zvis"))->show, false);
    const QByteArray written = PageWriter::toBytes(doc);
    const QString text = QString::fromUtf8(written);
    QVERIFY(text.contains(QStringLiteral("show=\"false\"")));
    QVERIFY(!text.contains(QStringLiteral("show=\"true\"")));
    QVERIFY(!text.contains(QStringLiteral("open=\"")));
    QVERIFY(!text.contains(QStringLiteral("visible=\"")));
    QVERIFY(text.contains(QStringLiteral("suspendExempt=\"true\"")));
    QVERIFY(!text.contains(QStringLiteral("dwellExempt")));
    PageDocument round;
    QVERIFY2(PageLoader::loadFromXml(written, round, &err), qPrintable(err));
    QCOMPARE(round.findGrid(QStringLiteral("hidden"))->show, false);
    QCOMPARE(round.findGrid(QStringLiteral("shown"))->show, true);
    QCOMPARE(round.findGrid(QStringLiteral("legacy"))->show, false);
    QCOMPARE(round.findCell(QStringLiteral("chide"))->show, false);
    QCOMPARE(round.findZone(QStringLiteral("zhide"))->show, false);
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
    QCOMPARE(z->actions.last().type, PageActionType::Nav);
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
    QVERIFY(doc.zones[0].suspendExempt);
    QVERIFY(doc.zones[1].suspendExempt);
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
    QVERIFY(pause->suspendExempt);
    QCOMPARE(pause->actions[0].command, QStringLiteral("toggleDwellSuspend"));
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
    mv.moveMode = PageMoveMode::Gaze;
    mv.zoomMode = PageZoomMode::Level;
    mv.zoomLevel = 2;
    PageAction mac;
    mac.type = PageActionType::MoveAndClick;
    mac.button = QStringLiteral("right");
    mac.zoomMode = PageZoomMode::Level;
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
    QCOMPARE(grid->cells[0].actions[1].moveMode, PageMoveMode::Gaze);
    QCOMPARE(grid->cells[0].actions[1].zoomMode, PageZoomMode::Level);
    QCOMPARE(grid->cells[0].actions[1].zoomLevel, 2);
    QCOMPARE(grid->cells[0].actions[2].zoomMode, PageZoomMode::Level);
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
    click.clickKind = PageClickKind::Double;
    PageAction mac;
    mac.type = PageActionType::MoveAndClick;
    mac.button = QStringLiteral("right");
    mac.zoomMode = PageZoomMode::Level;
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
    QCOMPARE(out.grids[0].cells[0].actions[1].clickKind, PageClickKind::Double);
    QCOMPARE(out.grids[0].cells[0].actions[1].button.toLower(), QStringLiteral("left"));
    QCOMPARE(out.grids[0].cells[0].actions[2].zoomMode, PageZoomMode::Level);
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
    QCOMPARE(a.clickKind, PageClickKind::Down);
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

void PageLoaderTest::cellSendAttribute()
{
    PageDocument doc;
    QString err;
    const QByteArray xml =
        QByteArray("<Page id=\"p\"><Grid id=\"g\"><Cell id=\"c\" row=\"0\" col=\"9\" "
                   "colSpan=\"10\" label=\"1\" send=\"1\"/></Grid></Page>");
    QVERIFY2(PageLoader::loadFromXml(xml, doc, &err), qPrintable(err));
    QCOMPARE(doc.grids[0].cells[0].label, QStringLiteral("1"));
    QCOMPARE(doc.grids[0].cells[0].actions.size(), 1);
    QCOMPARE(doc.grids[0].cells[0].actions[0].type, PageActionType::Send);
    QCOMPARE(doc.grids[0].cells[0].actions[0].sendKey, QStringLiteral("1"));

    PageDocument comma;
    const QByteArray commaXml =
        QByteArray("<Page id=\"p\"><Grid id=\"g\"><Cell id=\"c\" label=\",\" send=\",\"/>"
                   "</Grid></Page>");
    QVERIFY2(PageLoader::loadFromXml(commaXml, comma, &err), qPrintable(err));
    QCOMPARE(comma.grids[0].cells[0].actions[0].sendKey, QStringLiteral(","));
    PageAction a;
    QVERIFY2(loadOneAction(QByteArray("<Send value=\",\"/>"), a, &err), qPrintable(err));
    QCOMPARE(a.sendKey, QStringLiteral(","));
    QVERIFY2(loadOneAction(QByteArray("<Send value=\", Down\"/>"), a, &err), qPrintable(err));
    QCOMPARE(a.sendKey, QStringLiteral(","));
    QCOMPARE(a.sendEdge.toLower(), QStringLiteral("down"));

    const QByteArray written = PageWriter::toBytes(doc);
    QVERIFY(QString::fromUtf8(written).contains(QStringLiteral("send=\"1\"")));

    PageDocument mac;
    const QByteArray macXml =
        QByteArray("<Page id=\"p\"><Grid id=\"g\"><Cell id=\"c\" MoveAndClick=\"left\"/>"
                   "</Grid></Page>");
    QVERIFY2(PageLoader::loadFromXml(macXml, mac, &err), qPrintable(err));
    QCOMPARE(mac.grids[0].cells[0].actions.size(), 1);
    QCOMPARE(mac.grids[0].cells[0].actions[0].type, PageActionType::MoveAndClick);
    QCOMPARE(mac.grids[0].cells[0].actions[0].button, QStringLiteral("left"));
    QCOMPARE(mac.grids[0].cells[0].actions[0].zoomMode, PageZoomMode::Settings);
}

void PageLoaderTest::rejectTwoActionAttributes()
{
    PageDocument doc;
    QString err;
    const QByteArray xml =
        QByteArray("<Page id=\"p\"><Grid id=\"g\"><Cell id=\"c\" send=\"a\" click=\"left\"/>"
                   "</Grid></Page>");
    QVERIFY(!PageLoader::loadFromXml(xml, doc, &err));
    QVERIFY(err.contains(QStringLiteral("1 action attribute")));
}

void PageLoaderTest::parseSpecificActionElements()
{
    PageAction a;
    QString err;
    QVERIFY2(loadOneAction(QByteArray("<Send value=\"a\"/>"), a, &err), qPrintable(err));
    QCOMPARE(a.type, PageActionType::Send);
    QCOMPARE(a.sendKey, QStringLiteral("a"));
    QVERIFY2(loadOneAction(QByteArray("<Click value=\"left\"/>"), a, &err), qPrintable(err));
    QCOMPARE(a.type, PageActionType::Click);
    QCOMPARE(a.button, QStringLiteral("left"));
    QVERIFY2(loadOneAction(QByteArray("<Command value=\"toggleLookToScroll\"/>"), a, &err),
             qPrintable(err));
    QCOMPARE(a.command, QStringLiteral("toggleLookToScroll"));
    QVERIFY2(loadOneAction(QByteArray("<MoveAndClick value=\"left, 3\"/>"), a, &err),
             qPrintable(err));
    QCOMPARE(a.type, PageActionType::MoveAndClick);
    QCOMPARE(a.button, QStringLiteral("left"));
    QCOMPARE(a.zoomMode, PageZoomMode::Level);
    QCOMPARE(a.zoomLevel, 3);
    QVERIFY2(loadOneAction(QByteArray("<MoveAndClick value=\"right\"/>"), a, &err),
             qPrintable(err));
    QCOMPARE(a.button, QStringLiteral("right"));
    QCOMPARE(a.zoomMode, PageZoomMode::Settings);
    QVERIFY2(loadOneAction(QByteArray("<MoveAndClick value=\"left, 0\"/>"), a, &err),
             qPrintable(err));
    QCOMPARE(a.zoomMode, PageZoomMode::Off);
    QVERIFY2(loadOneAction(QByteArray("<LeftClick value=\"toggle\"/>"), a, &err),
             qPrintable(err));
    QCOMPARE(a.type, PageActionType::Click);
    QCOMPARE(a.button.toLower(), QStringLiteral("left"));
    QCOMPARE(a.clickKind, PageClickKind::Toggle);
    QVERIFY2(loadOneAction(QByteArray("<RightClick value=\"double\"/>"), a, &err),
             qPrintable(err));
    QCOMPARE(a.button.toLower(), QStringLiteral("right"));
    QCOMPARE(a.clickKind, PageClickKind::Double);
    QVERIFY2(loadOneAction(QByteArray("<LeftClickAtGaze value=\"-1\"/>"), a, &err),
             qPrintable(err));
    QCOMPARE(a.type, PageActionType::MoveAndClick);
    QCOMPARE(a.button.toLower(), QStringLiteral("left"));
    QCOMPARE(a.zoomMode, PageZoomMode::Foresight);
    QVERIFY2(loadOneAction(QByteArray("<MiddleClickAtGaze value=\"-2\"/>"), a, &err),
             qPrintable(err));
    QCOMPARE(a.button.toLower(), QStringLiteral("middle"));
    QCOMPARE(a.zoomMode, PageZoomMode::ForesightBonus);
    QVERIFY2(loadOneAction(QByteArray("<MouseMoveToGaze value=\"0\"/>"), a, &err),
             qPrintable(err));
    QCOMPARE(a.type, PageActionType::Move);
    QCOMPARE(a.moveMode, PageMoveMode::Gaze);
    QCOMPARE(a.zoomMode, PageZoomMode::Off);
    QVERIFY2(loadOneAction(QByteArray("<MouseMoveByDirection value=\"se, 40\"/>"), a, &err),
             qPrintable(err));
    QCOMPARE(a.moveMode, PageMoveMode::Direction);
    QCOMPARE(a.moveDirection, PageAnchor::BottomRight);
    QCOMPARE(a.moveAmount, 40);
    QVERIFY2(loadOneAction(QByteArray("<MouseMoveToPoint value=\"100, 200\"/>"), a, &err),
             qPrintable(err));
    QCOMPARE(a.moveMode, PageMoveMode::Absolute);
    QCOMPARE(int(a.moveX.value), 100);
    QCOMPARE(int(a.moveY.value), 200);
}

void PageLoaderTest::parseMoveVariants()
{
    PageAction a;
    QString err;
    QVERIFY2(loadOneAction(QByteArray("<Move value=\"gaze\"/>"), a, &err), qPrintable(err));
    QCOMPARE(a.moveMode, PageMoveMode::Gaze);
    QCOMPARE(a.zoomMode, PageZoomMode::Settings);
    QVERIFY2(loadOneAction(QByteArray("<Move value=\"gaze, 0\"/>"), a, &err), qPrintable(err));
    QCOMPARE(a.moveMode, PageMoveMode::Gaze);
    QCOMPARE(a.zoomMode, PageZoomMode::Off);
    {
        PageDocument doc;
        doc.id = QStringLiteral("t");
        PageGrid g;
        g.id = QStringLiteral("g");
        PageCell c;
        c.id = QStringLiteral("c");
        c.actions.push_back(a);
        g.cells.push_back(c);
        doc.grids.push_back(g);
        const QString xml = QString::fromUtf8(PageWriter::toBytes(doc));
        QVERIFY(xml.contains(QStringLiteral("mouseMoveToGaze=\"0\""))
                || xml.contains(QStringLiteral("MouseMoveToGaze")));
    }
    QVERIFY2(loadOneAction(QByteArray("<Move value=\"gaze, -1\"/>"), a, &err), qPrintable(err));
    QCOMPARE(a.zoomMode, PageZoomMode::Foresight);
    QVERIFY2(loadOneAction(QByteArray("<Move value=\"gaze, -2\"/>"), a, &err), qPrintable(err));
    QCOMPARE(a.zoomMode, PageZoomMode::ForesightBonus);
    QVERIFY2(loadOneAction(QByteArray("<Move value=\"gaze, 4\"/>"), a, &err), qPrintable(err));
    QCOMPARE(a.moveMode, PageMoveMode::Gaze);
    QCOMPARE(a.zoomMode, PageZoomMode::Level);
    QCOMPARE(a.zoomLevel, 4);
    QVERIFY2(loadOneAction(QByteArray("<Move value=\"up\"/>"), a, &err), qPrintable(err));
    QCOMPARE(a.moveMode, PageMoveMode::Direction);
    QCOMPARE(a.moveDirection, PageAnchor::Top);
    QCOMPARE(a.moveAmount, -1);
    QVERIFY2(loadOneAction(QByteArray("<Move value=\"down, 40\"/>"), a, &err), qPrintable(err));
    QCOMPARE(a.moveDirection, PageAnchor::Bottom);
    QCOMPARE(a.moveAmount, 40);
    QVERIFY2(loadOneAction(QByteArray("<Move value=\"TopLeft, 12\"/>"), a, &err), qPrintable(err));
    QCOMPARE(a.moveDirection, PageAnchor::TopLeft);
    QCOMPARE(a.moveAmount, 12);
    QVERIFY2(loadOneAction(QByteArray("<Move value=\"100, 200\"/>"), a, &err), qPrintable(err));
    QCOMPARE(a.moveMode, PageMoveMode::Absolute);
    QCOMPARE(int(a.moveX.value), 100);
    QCOMPARE(int(a.moveY.value), 200);
}

void PageLoaderTest::parseOpenPageBreadcrumb()
{
    PageAction a;
    QString err;
    QVERIFY2(loadOneAction(QByteArray("<OpenPage value=\"uw_qwerty, true\"/>"), a, &err),
             qPrintable(err));
    QCOMPARE(a.type, PageActionType::Nav);
    QCOMPARE(a.verb, PageVerb::Open);
    QCOMPARE(a.targetKind, PageTargetKind::Page);
    QCOMPARE(a.targetScope, PageNavScope::Id);
    QCOMPARE(a.targetId, QStringLiteral("uw_qwerty"));
    QCOMPARE(a.breadcrumb, true);
    QVERIFY2(loadOneAction(QByteArray("<ShowGrid value=\"board\"/>"), a, &err), qPrintable(err));
    QCOMPARE(a.type, PageActionType::Nav);
    QCOMPARE(a.verb, PageVerb::Open);
    QCOMPARE(a.targetKind, PageTargetKind::Grid);
    QCOMPARE(a.breadcrumb, false);
    QCOMPARE(a.targetId, QStringLiteral("board"));
    QVERIFY2(loadOneAction(QByteArray("<OpenGrid value=\"board\"/>"), a, &err), qPrintable(err));
    QCOMPARE(a.targetKind, PageTargetKind::Grid);
    QCOMPARE(a.targetId, QStringLiteral("board"));
    QVERIFY2(loadOneAction(QByteArray("<ShowCell value=\"k_q\"/>"), a, &err), qPrintable(err));
    QCOMPARE(a.verb, PageVerb::Open);
    QCOMPARE(a.targetKind, PageTargetKind::Cell);
    QCOMPARE(a.targetId, QStringLiteral("k_q"));
    PageAction show;
    QVERIFY2(loadOneAction(QByteArray("<ShowGrid value=\"board\"/>"), show, &err), qPrintable(err));
    PageDocument written;
    written.id = QStringLiteral("t");
    PageGrid g;
    g.id = QStringLiteral("g");
    PageCell c;
    c.id = QStringLiteral("c");
    c.actions.push_back(show);
    g.cells.push_back(c);
    written.grids.push_back(g);
    const QString xml = QString::fromUtf8(PageWriter::toBytes(written));
    QVERIFY(xml.contains(QStringLiteral("showGrid=\"board\"")));
    QVERIFY(!xml.contains(QStringLiteral("openGrid")));
}

void PageLoaderTest::parseCloseSpecialsAndGoBack()
{
    PageAction a;
    QString err;
    QVERIFY2(loadOneAction(QByteArray("<ClosePage value=\"-self\"/>"), a, &err), qPrintable(err));
    QCOMPARE(a.type, PageActionType::Nav);
    QCOMPARE(a.verb, PageVerb::Close);
    QCOMPARE(a.targetKind, PageTargetKind::Page);
    QCOMPARE(a.targetScope, PageNavScope::Self);
    QVERIFY2(loadOneAction(QByteArray("<HideGrid value=\"-all, true\"/>"), a, &err),
             qPrintable(err));
    QCOMPARE(a.targetKind, PageTargetKind::Grid);
    QCOMPARE(a.targetScope, PageNavScope::All);
    QCOMPARE(a.breadcrumb, true);
    QVERIFY2(loadOneAction(QByteArray("<CloseGrid value=\"-all, true\"/>"), a, &err),
             qPrintable(err));
    QCOMPARE(a.targetKind, PageTargetKind::Grid);
    QCOMPARE(a.targetScope, PageNavScope::All);
    QVERIFY2(loadOneAction(QByteArray("<HideZone value=\"-!self\"/>"), a, &err), qPrintable(err));
    QCOMPARE(a.targetKind, PageTargetKind::Zone);
    QCOMPARE(a.targetScope, PageNavScope::Others);
    QVERIFY2(loadOneAction(QByteArray("<HideCell value=\"k_q\"/>"), a, &err), qPrintable(err));
    QCOMPARE(a.verb, PageVerb::Close);
    QCOMPARE(a.targetKind, PageTargetKind::Cell);
    QCOMPARE(a.targetId, QStringLiteral("k_q"));
    QVERIFY2(loadOneAction(QByteArray("<GoBack/>"), a, &err), qPrintable(err));
    QCOMPARE(a.type, PageActionType::GoBack);
}

void PageLoaderTest::genericActionAttribute()
{
    PageAction a;
    QString err;
    QVERIFY2(loadOneAction(QByteArray("<Action send=\"x\"/>"), a, &err), qPrintable(err));
    QCOMPARE(a.type, PageActionType::Send);
    QCOMPARE(a.sendKey, QStringLiteral("x"));
    QVERIFY2(loadOneAction(QByteArray("<Action openPage=\"uw_qwerty, true\"/>"), a, &err),
             qPrintable(err));
    QCOMPARE(a.type, PageActionType::Nav);
    QCOMPARE(a.targetKind, PageTargetKind::Page);
    QCOMPARE(a.targetId, QStringLiteral("uw_qwerty"));
    QCOMPARE(a.breadcrumb, true);
    QVERIFY2(loadOneAction(QByteArray("<Action goBack=\"true\"/>"), a, &err), qPrintable(err));
    QCOMPARE(a.type, PageActionType::GoBack);
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
