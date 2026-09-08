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
    void clampSizeRoundTrip();
    void ltsSpeedLadder();
    void parseRowWeightsCsv();
    void loadFixture();
    void ahkCdataRoundTrip();
    void inheritStyleAndDwell();
    void namedColorTokensRoundTrip();
    void pageChromeDefaults();
    void zoneDwellDefaultsAndOffset();
    void rejectMissingPageId();
    void rejectUnknownChild();
    void rejectNonPageRoot();
    void rejectRemovedActionAliases();
    void loadMainPage();
    void loadComposePage();
    void loadQwertyXml();
    void layersAttribute();
    void rejectInvalidLayers();
    void cellDropsShellAndInteractive();
    void loadConvertedBoards();
    void loadLtsMenu();
    void keyboardMainOpensDrawer();
    void pageWriterRoundTripMain();
    void rowWeightsRoundTrip();
    void trackSizesRoundTrip();
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

void PageLoaderTest::clampSizeRoundTrip()
{
    const QByteArray xml = R"xml(
<Page id="p">
  <Grid id="g" size="clamp(1.8*A_ScreenHeight, 1080, A_ScreenWidth), A_ScreenHeight">
    <Cell id="c" label="X"/>
  </Grid>
</Page>
)xml";
    PageDocument doc;
    QString err;
    QVERIFY2(PageLoader::loadFromXml(xml, doc, &err), qPrintable(err));
    QCOMPARE(doc.grids[0].size.x.unit, PageDim::Unit::Expression);
    QCOMPARE(doc.grids[0].size.y.unit, PageDim::Unit::Expression);
    QCOMPARE(doc.grids[0].size.x.resolve(0, 0, 3440, 1440), 2592.0);
    PageDocument written;
    QVERIFY2(PageLoader::loadFromXml(PageWriter::toBytes(doc), written, &err), qPrintable(err));
    QCOMPARE(PageDimParse::token(written.grids[0].size.x),
             QStringLiteral("clamp(1.8*A_ScreenHeight, 1080, A_ScreenWidth)"));
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
    QCOMPARE(nudgeLtsSpeed(40.0, +1), 40.0);
    QCOMPARE(nudgeLtsSpeed(20.0, +1), 40.0);
    QCOMPARE(snapLtsSpeed(50.0), 40.0);
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
    QCOMPARE(z->actions[0].type, PageActionType::ShowLayers);
    QCOMPARE(z->actions[0].layers, (QVector<int>{1, 2}));

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
    QVERIFY(st.background.isSet());
    QCOMPARE(st.background.parsed().rgb(), QColor(QStringLiteral("#FF000000")).rgb());
    QVERIFY(st.foreground.isSet());
    QCOMPARE(st.radius ? st.radius->first() : 0.0, 12.0);
    QVERIFY(!st.thickness.has_value());
    QCOMPARE(st.resolvedThickness().first(), PageChrome::kDefaultThickness);
    QVERIFY(!st.progressStyle.has_value());
    const PageChrome gridSt = PageResolve::gridStyle(doc, g.styleId, g.style);
    QCOMPARE(gridSt.background.parsed().rgb(), QColor(QStringLiteral("#111111")).rgb());
    QCOMPARE(gridSt.radius ? gridSt.radius->first() : 0.0, 2.0);
    QVERIFY(!gridSt.foreground.isSet());
    QVERIFY(!gridSt.progressStyle.has_value());
    QVERIFY(!gridSt.progressColor.isSet());

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
    QVERIFY(named.progressColor.isSet());
    QCOMPARE(named.progressColor.parsed().name(QColor::HexRgb).toUpper(), QStringLiteral("#00DCFF"));
    QCOMPARE(named.radius ? named.radius->at(0) : -1.0, 0.0);
    QCOMPARE(named.radius ? named.radius->at(1) : -1.0, 0.0);
    QCOMPARE(named.radius ? named.radius->at(2) : -1.0, 12.0);
    QCOMPARE(named.radius ? named.radius->at(3) : -1.0, 12.0);
    QVERIFY(!styled.zones.isEmpty());
    const PageChrome zst = PageResolve::zoneStyle(styled, styled.zones[0]);
    QVERIFY(zst.progressStyle.has_value());
    QCOMPARE(zst.progressStyle->toCsv(), QStringLiteral("fillup,border"));
    QVERIFY(zst.progressColor.isSet());
    QCOMPARE(zst.progressColor.parsed().name(QColor::HexRgb).toUpper(), QStringLiteral("#00DCFF"));
    QCOMPARE(zst.radius ? zst.radius->at(2) : -1.0, 12.0);

    PageDocument gridInherit;
    QVERIFY2(PageLoader::loadFromXml(R"xml(
<Page id="p">
  <Style id="chip" foreground="#ffffff" progressStyle="fillup,border" progressColor="#00DCFF"
         background="#111111"/>
  <Grid id="g" style="chip" size="100,100" foreground="#ff0000" progressStyle="pie"/>
</Page>
)xml",
                                    gridInherit, &err),
            qPrintable(err));
    QVERIFY(gridInherit.grids[0].style.foreground.isSet());
    QVERIFY(gridInherit.grids[0].style.progressStyle.has_value());
    const PageChrome resolvedGrid =
        PageResolve::gridStyle(gridInherit, gridInherit.grids[0].styleId, gridInherit.grids[0].style);
    QVERIFY(resolvedGrid.background.isSet());
    QVERIFY(!resolvedGrid.foreground.isSet());
    QVERIFY(!resolvedGrid.progressStyle.has_value());
    QVERIFY(!resolvedGrid.progressColor.isSet());
    const QString gridXml = QString::fromUtf8(PageWriter::toBytes(gridInherit));
    for (const QString& line : gridXml.split(QLatin1Char('\n'))) {
        if (line.contains(QLatin1String("<Grid"))) {
            QVERIFY(!line.contains(QLatin1String("foreground")));
            QVERIFY(!line.contains(QLatin1String("progressStyle")));
        }
    }

    PageDocument written;
    QVERIFY2(PageLoader::loadFromXml(PageWriter::toBytes(styled), written, &err), qPrintable(err));
    QCOMPARE(written.styles.value(QStringLiteral("chip")).progressStyle->toCsv(),
             QStringLiteral("fillup,border"));
    QCOMPARE(written.styles.value(QStringLiteral("chip")).progressColor.parsed().name(QColor::HexRgb).toUpper(),
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
    pv.setStylesFromCsv(QStringLiteral("pie"));
    QVERIFY(pv.style.pie);
    QVERIFY(!pv.style.radial);
    QVERIFY(!pv.style.fillBackground);
    QVERIFY(!pv.style.border);
    QCOMPARE(pv.stylesCsv(), QStringLiteral("pie"));
    pv.setStylesFromCsv(QStringLiteral("radial,pie"));
    QVERIFY(pv.style.radial);
    QVERIFY(pv.style.pie);
    QCOMPARE(pv.stylesCsv(), QStringLiteral("radial,pie"));

    const PageDwell dw = PageResolve::dwell(doc, cell.dwellId, cell.dwell);
    QCOMPARE(dw.scanGrace.value_or(-1), 50);
    QVERIFY(!dw.dwellGrace.has_value());
    QVERIFY(dw.activation.has_value());
    QCOMPARE(dw.activation->at(0), 800);
    const PageDwell gridDw = PageResolve::dwell(doc, g.dwellId, g.dwell);
    QCOMPARE(gridDw.scanGrace.value_or(-1), 999);
    QCOMPARE(gridDw.activation->at(0), 50);
}

void PageLoaderTest::namedColorTokensRoundTrip()
{
    PageDocument doc;
    QString err;
    QVERIFY2(PageLoader::loadFromXml(R"xml(
<Page id="p" background="accent" foreground="foreground" border="surface"
      progressColor="progress">
  <Style id="brand" background="red" foreground="foreground"/>
  <Grid id="g" size="100,100" background="green">
    <Cell id="c" label="X"/>
  </Grid>
</Page>
)xml",
                                    doc, &err),
            qPrintable(err));
    QCOMPARE(doc.style.background.token, QStringLiteral("accent"));
    QCOMPARE(doc.style.foreground.token, QStringLiteral("foreground"));
    QCOMPARE(doc.style.borderColor.token, QStringLiteral("surface"));
    QCOMPARE(doc.style.progressColor.token, QStringLiteral("progress"));
    QCOMPARE(doc.styles.value(QStringLiteral("brand")).background.token, QStringLiteral("red"));
    QCOMPARE(doc.grids[0].style.background.token, QStringLiteral("green"));
    PageDocument written;
    QVERIFY2(PageLoader::loadFromXml(PageWriter::toBytes(doc), written, &err), qPrintable(err));
    QCOMPARE(written.style.background.token, QStringLiteral("accent"));
    QCOMPARE(written.style.progressColor.token, QStringLiteral("progress"));
    QCOMPARE(written.styles.value(QStringLiteral("brand")).background.token, QStringLiteral("red"));
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
                             QStringLiteral("MouseMoveAndClick"), QStringLiteral("ShowGrid"),
                             QStringLiteral("HideGrid"), QStringLiteral("ToggleGrid")};
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
                             QStringLiteral("uw_qwerty"),
                             QStringLiteral("main_settings_speed")};
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
            QVERIFY(!doc.grids[0].style.background.isSet());
            const PageGrid* tabs = doc.findGrid(QStringLiteral("tabs"));
            QVERIFY(tabs);
            bool hasTabs = false;
            for (const PageCell& c : tabs->cells) {
                if (c.id == QLatin1String("tab_speed")) {
                    hasTabs = true;
                    QCOMPARE(c.isInteractive(), id != QLatin1String("main_settings_speed"));
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
    QCOMPARE(board->layers, QVector<int>({1}));
    QCOMPARE(vert1->layers, (QVector<int>{1, 2}));
    QCOMPARE(vert2->layers, QVector<int>({2}));
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
    const PageCell* kq = doc.findCell(QStringLiteral("k_q"));
    QVERIFY(kq);
    QCOMPARE(kq->textStyle, QStringLiteral("key"));
    QCOMPARE(vert1->cells[1].actions.size(), 1);
    QCOMPARE(vert1->cells[1].actions[0].type, PageActionType::MoveAndClick);
    QCOMPARE(vert1->cells[1].actions[0].button, QStringLiteral("left"));
    QCOMPARE(vert1->cells[1].actions[0].zoomMode, PageZoomMode::Settings);
}

void PageLoaderTest::layersAttribute()
{
    PageDocument doc;
    QString err;
    const QByteArray xml = R"xml(
<Page id="p" showLayers="2">
  <Grid id="shown" size="100,100">
    <Cell id="cshow"/>
    <Cell id="cexem" dwellExempt="true"/>
    <Cell id="csus" suspendExempt="true"/>
  </Grid>
  <Grid id="hidden" size="100,100" layers="2"/>
  <Zone id="zshow" size="80,40"/>
  <Zone id="zhide" size="80,40" layers="2"/>
</Page>
)xml";
    QVERIFY2(PageLoader::loadFromXml(xml, doc, &err), qPrintable(err));
    QCOMPARE(doc.showLayers, QVector<int>({2}));
    QCOMPARE(doc.findGrid(QStringLiteral("shown"))->layers, QVector<int>({1}));
    QCOMPARE(doc.findGrid(QStringLiteral("hidden"))->layers, QVector<int>({2}));
    QCOMPARE(doc.findCell(QStringLiteral("cexem"))->suspendExempt, true);
    QCOMPARE(doc.findCell(QStringLiteral("csus"))->suspendExempt, true);
    QCOMPARE(doc.findZone(QStringLiteral("zshow"))->layers, QVector<int>({1}));
    QCOMPARE(doc.findZone(QStringLiteral("zhide"))->layers, QVector<int>({2}));
    const QByteArray written = PageWriter::toBytes(doc);
    const QString text = QString::fromUtf8(written);
    QVERIFY(text.contains(QStringLiteral("showLayers=\"2\"")));
    QVERIFY(text.contains(QStringLiteral("layers=\"2\"")));
    QVERIFY(!text.contains(QStringLiteral("show=")));
    QVERIFY(text.contains(QStringLiteral("suspendExempt=\"true\"")));
    QVERIFY(!text.contains(QStringLiteral("dwellExempt")));
    PageDocument round;
    QVERIFY2(PageLoader::loadFromXml(written, round, &err), qPrintable(err));
    QCOMPARE(round.showLayers, QVector<int>({2}));
    QCOMPARE(round.findGrid(QStringLiteral("hidden"))->layers, QVector<int>({2}));
    QCOMPARE(round.findGrid(QStringLiteral("shown"))->layers, QVector<int>({1}));
}

void PageLoaderTest::rejectInvalidLayers()
{
    PageDocument doc;
    QString err;
    QVERIFY(!PageLoader::loadFromXml(R"xml(
<Page id="p"><Grid id="g" size="10,10" layers="nope"><Cell id="c"/></Grid></Page>
)xml",
                                    doc, &err));
    QVERIFY(err.contains(QStringLiteral("layers")));
    err.clear();
    QVERIFY(!PageLoader::loadFromXml(R"xml(
<Page id="p" showLayers="1,foo"><Grid id="g" size="10,10"><Cell id="c"/></Grid></Page>
)xml",
                                    doc, &err));
    QVERIFY(err.contains(QStringLiteral("showLayers")));
}

void PageLoaderTest::cellDropsShellAndInteractive()
{
    const QByteArray xml = R"xml(
<Page id="p">
  <Grid id="g" size="100,100" shell="true">
    <Cell id="btn" shell="true" interactive="false"/>
    <Cell id="lab" role="label"/>
    <Cell id="cur" role="tab"/>
    <Cell id="nav" role="tab" openPage="other"/>
  </Grid>
  <Zone id="chip" size="40,20" shell="true"/>
</Page>
)xml";
    PageDocument doc;
    QString err;
    QVERIFY2(PageLoader::loadFromXml(xml, doc, &err), qPrintable(err));
    QCOMPARE(doc.findGrid(QStringLiteral("g"))->shell, true);
    QCOMPARE(doc.findCell(QStringLiteral("btn"))->shell, false);
    QVERIFY(doc.findCell(QStringLiteral("btn"))->isInteractive());
    QVERIFY(!doc.findCell(QStringLiteral("lab"))->isInteractive());
    QVERIFY(!doc.findCell(QStringLiteral("cur"))->isInteractive());
    QVERIFY(doc.findCell(QStringLiteral("nav"))->isInteractive());
    QCOMPARE(doc.findZone(QStringLiteral("chip"))->shell, true);
    const QString text = QString::fromUtf8(PageWriter::toBytes(doc));
    QVERIFY(!text.contains(QStringLiteral("interactive")));
    QVERIFY(text.contains(QStringLiteral("shell=\"true\"")));
    for (const QString& line : text.split(QLatin1Char('\n'))) {
        if (line.contains(QLatin1String("<Cell"))) {
            QVERIFY(!line.contains(QLatin1String("shell=")));
        }
    }
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
    QVERIFY(doc.grids[0].style.background.isSet());
    QCOMPARE(doc.grids[0].style.background.parsed().alpha(), 0);
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
    QCOMPARE(doc.grids.size(), 4);
    QCOMPARE(doc.findGrid(QStringLiteral("board"))->layers, QVector<int>({1}));
    QCOMPARE(doc.findGrid(QStringLiteral("board_shift"))->layers, QVector<int>({2}));
    QCOMPARE(doc.findGrid(QStringLiteral("board_sym"))->layers, QVector<int>({3}));
    QCOMPARE(doc.findGrid(QStringLiteral("board_sym_shift"))->layers, QVector<int>({4}));
    const PageCell* shift = doc.findCell(QStringLiteral("shift"));
    const PageCell* sym = doc.findCell(QStringLiteral("sym"));
    QVERIFY(shift);
    QVERIFY(sym);
    QCOMPARE(shift->actions[0].type, PageActionType::ShowLayers);
    QCOMPARE(shift->actions[0].layers, QVector<int>({2}));
    QCOMPARE(sym->actions[0].layers, QVector<int>({3}));
    const PageZone* z = doc.findZone(QStringLiteral("edge_main"));
    QVERIFY(z);
    QCOMPARE(z->layers, (QVector<int>{1, 2, 3, 4}));
    QVERIFY(z->actions.size() >= 2);
    QCOMPARE(z->actions.last().type, PageActionType::ShowLayers);
    QCOMPARE(z->actions.last().layers, (QVector<int>{1, 2}));
}

void PageLoaderTest::loadComposePage()
{
    PageDocument doc;
    QString err;
    const QString path =
        QStringLiteral(GAZER_SOURCE_DIR) + QStringLiteral("/resources/layouts/compose.xml");
    QVERIFY2(PageLoader::loadFromFile(path, doc, &err), qPrintable(err));
    QCOMPARE(doc.id, QStringLiteral("compose"));
    QVERIFY(doc.findGrid(QStringLiteral("board")));
    QCOMPARE(doc.findGrid(QStringLiteral("board"))->size.x.unit, PageDim::Unit::Expression);
    QVERIFY(doc.findGrid(QStringLiteral("keys")));
    QCOMPARE(doc.findGrid(QStringLiteral("keys"))->rows, 3);
    QCOMPARE(doc.findGrid(QStringLiteral("keys"))->columns, 120);
    QCOMPARE(doc.findGrid(QStringLiteral("keys"))->cells.size(), 36);
    QCOMPARE(doc.findGrid(QStringLiteral("board"))->layers, (QVector<int>{1, 2, 3, 4}));
    QCOMPARE(doc.findGrid(QStringLiteral("keys"))->layers, QVector<int>({1}));
    QCOMPARE(doc.findGrid(QStringLiteral("keys_shift"))->layers, QVector<int>({2}));
    QCOMPARE(doc.findGrid(QStringLiteral("keys_sym"))->layers, QVector<int>({3}));
    QCOMPARE(doc.findGrid(QStringLiteral("keys_sym_shift"))->layers, QVector<int>({4}));
    QVERIFY(doc.findCell(QStringLiteral("phrase")));
    QCOMPARE(doc.findCell(QStringLiteral("phrase"))->isInteractive(), false);
    QVERIFY(!doc.findCell(QStringLiteral("done")));
    QCOMPARE(doc.findCell(QStringLiteral("page_title"))->colSpan, 4);
    QVERIFY(doc.findGrid(QStringLiteral("sys_vol")));
    QCOMPARE(doc.findGrid(QStringLiteral("sys_vol"))->columns, 5);
    QVERIFY(doc.findCell(QStringLiteral("vol_down")));
    QCOMPARE(doc.findCell(QStringLiteral("vol_down"))->actions[0].command,
             QStringLiteral("compose.volume.dec"));
    QVERIFY(doc.findCell(QStringLiteral("vol_track")));
    QVERIFY(!doc.findCell(QStringLiteral("vol_track"))->isInteractive());
    QVERIFY(!doc.findCell(QStringLiteral("vol_val")));
    QVERIFY(doc.findCell(QStringLiteral("vol_up")));
    QCOMPARE(doc.findCell(QStringLiteral("vol_up"))->actions[0].command,
             QStringLiteral("compose.volume.inc"));
    QVERIFY(!doc.findCell(QStringLiteral("topic_new")));
    QVERIFY(!doc.findCell(QStringLiteral("delword")));
    QVERIFY(!doc.findGrid(QStringLiteral("actions")));
    QVERIFY(doc.findGrid(QStringLiteral("edit_keys")));
    QVERIFY(doc.findGrid(QStringLiteral("speak_keys")));
    QCOMPARE(doc.findGrid(QStringLiteral("edit_keys"))->col, 0);
    QCOMPARE(doc.findGrid(QStringLiteral("speak_keys"))->col, 7);
    QCOMPARE(doc.findGrid(QStringLiteral("phrase"))->col, 1);
    const QVector<int> allLayers{1, 2, 3, 4};
    QCOMPARE(doc.findGrid(QStringLiteral("compose"))->layers, allLayers);
    QCOMPARE(doc.findGrid(QStringLiteral("edit_keys"))->layers, allLayers);
    QCOMPARE(doc.findGrid(QStringLiteral("phrase"))->layers, allLayers);
    QCOMPARE(doc.findGrid(QStringLiteral("chips"))->layers, allLayers);
    QCOMPARE(doc.findGrid(QStringLiteral("speak_keys"))->layers, allLayers);
    QCOMPARE(doc.findGrid(QStringLiteral("board"))->rows, 5);
    QCOMPARE(doc.findGrid(QStringLiteral("keys"))->row, 4);
    QVERIFY(doc.findCell(QStringLiteral("speak")));
    QCOMPARE(doc.findCell(QStringLiteral("speak"))->actions[0].command,
             QStringLiteral("compose.speak"));
    QVERIFY(doc.findGrid(QStringLiteral("topics")));
    QVERIFY(doc.findGrid(QStringLiteral("soundboard")));
    QVERIFY(doc.findCell(QStringLiteral("undo")));
    QVERIFY(doc.findCell(QStringLiteral("pin")));
    QCOMPARE(doc.findCell(QStringLiteral("pin"))->actions[0].command, QStringLiteral("compose.pin"));
    QVERIFY(doc.findCell(QStringLiteral("editPins")));
    QCOMPARE(doc.findCell(QStringLiteral("editPins"))->actions[0].command,
             QStringLiteral("compose.editPins"));
    QVERIFY(doc.findCell(QStringLiteral("voices")));
    QCOMPARE(doc.findCell(QStringLiteral("voices"))->actions[0].command,
             QStringLiteral("compose.openVoices"));
    QVERIFY(doc.findCell(QStringLiteral("history")));
    QCOMPARE(doc.findCell(QStringLiteral("history"))->actions[0].command,
             QStringLiteral("compose.openHistory"));
    QVERIFY(doc.findCell(QStringLiteral("delivery")));
    QCOMPARE(doc.findCell(QStringLiteral("delivery"))->label, QStringLiteral("Freestyle"));
    QCOMPARE(doc.findCell(QStringLiteral("delivery"))->actions[0].command,
             QStringLiteral("compose.toggleFreestyle"));
    QCOMPARE(doc.findCell(QStringLiteral("delivery"))->activeState,
             QStringLiteral("compose.freestyleMode"));
    QVERIFY(!doc.findCell(QStringLiteral("tags")));
    QCOMPARE(doc.findCell(QStringLiteral("sym"))->icon, QStringLiteral("numbers"));
    for (const char* gid : {"keys", "keys_shift", "keys_sym", "keys_sym_shift", "edit_keys",
                            "speak_keys"}) {
        const PageGrid* g = doc.findGrid(QLatin1String(gid));
        QVERIFY2(g, gid);
        for (const PageCell& c : g->cells) {
            for (const PageAction& a : c.actions) {
                QVERIFY2(a.command != QLatin1String("tab"), qPrintable(c.id));
                QVERIFY2(a.command != QLatin1String("leftWin"), qPrintable(c.id));
            }
        }
    }
    const PageCell* qKey = doc.findCell(QStringLiteral("ch_113_0_1"));
    QVERIFY(qKey);
    QCOMPARE(qKey->actions[0].type, PageActionType::Send);
    QCOMPARE(qKey->actions[0].sendKey, QStringLiteral("q"));
    QCOMPARE(qKey->col, 10);
    QCOMPARE(qKey->colSpan, 10);
    const PageCell* aKey = doc.findCell(QStringLiteral("ch_97_1_1"));
    const PageCell* zKey = doc.findCell(QStringLiteral("ch_122_2_2"));
    QVERIFY(aKey);
    QVERIFY(zKey);
    QVERIFY(aKey->col > qKey->col);
    QVERIFY(zKey->col > aKey->col);
    QCOMPARE(zKey->colSpan, 9);
    const PageCell* gKey = doc.findCell(QStringLiteral("ch_103_1_5"));
    const PageCell* hKey = doc.findCell(QStringLiteral("ch_104_1_6"));
    const PageCell* space = doc.findCell(QStringLiteral("space"));
    QVERIFY(gKey);
    QVERIFY(hKey);
    QVERIFY(space);
    QVERIFY(space->col > gKey->col);
    QVERIFY(space->col < gKey->col + gKey->colSpan);
    QVERIFY(space->col + space->colSpan > hKey->col);
    QVERIFY(space->col + space->colSpan < hKey->col + hKey->colSpan);
    const PageCell* shift = doc.findCell(QStringLiteral("shift"));
    const PageCell* sym = doc.findCell(QStringLiteral("sym"));
    QVERIFY(shift);
    QVERIFY(sym);
    QCOMPARE(shift->actions[0].type, PageActionType::ShowLayers);
    QCOMPARE(shift->actions[0].layers, QVector<int>({2}));
    QCOMPARE(sym->actions[0].layers, QVector<int>({3}));
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
    QCOMPARE(doc.findGrid(QStringLiteral("drawer"))->layers, QVector<int>({2}));
    QCOMPARE(doc.findGrid(QStringLiteral("quit"))->layers, QVector<int>({3}));
    QCOMPARE(doc.showLayers, QVector<int>({1}));
    QCOMPARE(doc.findGrid(QStringLiteral("drawer"))->cells.size(), 10);
    QCOMPARE(doc.findCell(QStringLiteral("open_compose"))->actions[0].command,
             QStringLiteral("compose.open"));
    QCOMPARE(doc.zones[0].actions[1].type, PageActionType::ShowLayers);
    QCOMPARE(doc.zones[0].actions[1].layers, (QVector<int>{1, 2}));
    QVERIFY(doc.zones[0].suspendExempt);
    QVERIFY(doc.zones[1].suspendExempt);
    const PageCell* pause = nullptr;
    for (const PageCell& c : doc.findGrid(QStringLiteral("drawer"))->cells) {
        if (c.id == QLatin1String("dwell_suspend")) {
            pause = &c;
        }
    }
    QVERIFY(pause);
    QVERIFY(pause->suspendExempt);
    QCOMPARE(pause->actions[0].command, QStringLiteral("toggleDwellSuspend"));
    QCOMPARE(doc.findGrid(QStringLiteral("quit"))->cells[0].isInteractive(), false);
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
    QCOMPARE(dst.findGrid(QStringLiteral("drawer"))->layers, QVector<int>({2}));
    QCOMPARE(dst.findGrid(QStringLiteral("quit"))->layers, QVector<int>({3}));
    const QString roundText = QString::fromUtf8(xml);
    QVERIFY(!roundText.contains(QStringLiteral("chrome=")));
}

void PageLoaderTest::rowWeightsRoundTrip()
{
    PageDocument src;
    src.id = QStringLiteral("p");
    PageGrid g;
    g.id = QStringLiteral("g");
    g.rows = 3;
    g.columns = 1;
    g.rowTracks = starTracks({1.0, 2.0, 2.0});
    src.grids.push_back(g);
    const QByteArray xml = PageWriter::toBytes(src);
    QVERIFY(QString::fromUtf8(xml).contains(QStringLiteral("rowWeights=\"1,2,2\"")));
    QVERIFY(!QString::fromUtf8(xml).contains(QStringLiteral("rowHeights")));
    QString err;
    PageDocument dst;
    QVERIFY2(PageLoader::loadFromXml(xml, dst, &err), qPrintable(err));
    QCOMPARE(PageDimParse::tokenList(dst.grids[0].rowTracks), QStringLiteral("*,2*,2*"));
}

void PageLoaderTest::trackSizesRoundTrip()
{
    const QByteArray xml = R"xml(
<Page id="p">
  <Grid id="g" rows="3" columns="3"
        rowHeights="80px,*,120" columnWidths="200,2*,*">
    <Cell id="c" label="X"/>
  </Grid>
</Page>
)xml";
    PageDocument doc;
    QString err;
    QVERIFY2(PageLoader::loadFromXml(xml, doc, &err), qPrintable(err));
    QCOMPARE(doc.grids[0].rowTracks.size(), 3);
    QCOMPARE(doc.grids[0].rowTracks[0].dim.unit, PageDim::Unit::Pixels);
    QCOMPARE(doc.grids[0].rowTracks[0].dim.value, 80.0);
    QCOMPARE(doc.grids[0].rowTracks[1].kind, PageTrackSize::Kind::Star);
    QCOMPARE(doc.grids[0].columnTracks[1].star, 2.0);
    PageDocument written;
    const QByteArray out = PageWriter::toBytes(doc);
    QVERIFY2(PageLoader::loadFromXml(out, written, &err), qPrintable(err));
    QCOMPARE(PageDimParse::tokenList(written.grids[0].rowTracks), QStringLiteral("80,*,120"));
    QCOMPARE(PageDimParse::tokenList(written.grids[0].columnTracks), QStringLiteral("200,2*,*"));
    QVERIFY(QString::fromUtf8(out).contains(QStringLiteral("rowHeights=")));
    QVERIFY(QString::fromUtf8(out).contains(QStringLiteral("columnWidths=")));
    QVERIFY(!QString::fromUtf8(out).contains(QStringLiteral("columnWeights")));
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
