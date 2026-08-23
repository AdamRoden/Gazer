#include "assist/LtsSpeed.h"
#include "layout/PageCatalog.h"
#include "layout/PageDetector.h"
#include "layout/PageDim.h"
#include "layout/PageHit.h"
#include "layout/PageLoader.h"
#include "layout/PageResolve.h"
#include "layout/PageWriter.h"
#include "ui/ProgressVisuals.h"
#include "layout/RoundBox.h"
#include "utils/Log.h"

#include <QDir>
#include <QFile>
#include <QSet>
#include <QStringList>
#include <QTemporaryDir>
#include <QtTest>

Q_LOGGING_CATEGORY(lcGazer, "gazer")

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

class PageLoaderTest final : public QObject {
    Q_OBJECT

private slots:
    void dimPixelsVsProportion();
    void dimFraction();
    void dimHeightRelative();
    void placeRectBottom();
    void placeRectHeightSquareAtPoint();
    void ltsSpeedLadder();
    void loadFixture();
    void inheritStyleAndDwell();
    void roundedBoxFitsSemicircle();
    void roundedBoxHitIgnoresSquareCorners();
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
    void loadLtsMenu();
    void settingsTabsEqualWidth();
    void sleepKeepsContentWhenSuspended();
    void edgeChipHidesUntilProgress();
    void keyboardMainOpensDrawer();
    void liveEditorGridHasOpaqueChrome();
    void overlappingBoardOccludesLowerPage();
    void pageWriterRoundTripMain();
    void cellIndexAtMatchesCellRect();
    void actionExtrasRoundTrip();
    void edgeChipGazeHitsOnScreenChrome();
    void aboveTaskbarUsesDesktop();
    void mainChipsSitAboveTaskbar();
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

void PageLoaderTest::roundedBoxFitsSemicircle()
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

void PageLoaderTest::roundedBoxHitIgnoresSquareCorners()
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
    g.style.radius = PageBox::all(8.0);
    g.style.thickness = PageBox::all(1.0);
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
    QVERIFY(dst.findZone(QStringLiteral("mainChip"))->aboveTaskbar);
    QCOMPARE(dst.findGrid(QStringLiteral("drawer")) != nullptr, true);
}

void PageLoaderTest::cellIndexAtMatchesCellRect()
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

void PageLoaderTest::zoneProgressCoercedWhenOffScreen()
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
    const PageTarget* chip = targetById(t, QStringLiteral("mainChip"));
    QVERIFY(chip);
    const PageTarget* hit = PageHit::at(t, chip->geom.dwellZone.center());
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
    QVERIFY(targetById(t, QStringLiteral("mainChip")) == nullptr);
    const PageTarget* sleep = targetById(t, QStringLiteral("sleep"));
    QVERIFY(sleep);
    QVERIFY(PageHit::at(t, sleep->geom.dwellZone.center()) == sleep);
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
    QCOMPARE(grid->cells[0].actions[1].moveMode, PageMoveMode::Absolute);
    QCOMPARE(int(grid->cells[0].actions[1].moveX.value), 10);
    QCOMPARE(grid->cells[0].actions[1].speed, 5);
    QCOMPARE(grid->cells[0].actions[1].zoomLevel, 2);
    QCOMPARE(grid->cells[0].actions[2].clickCount, 2);
    QCOMPARE(grid->cells[0].actions[2].zoomLevel, 1);
    QCOMPARE(grid->cells[0].actions[3].command, QStringLiteral("quitApp"));
    QCOMPARE(grid->cells[0].actions[3].args, QStringLiteral("now"));
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
    // Dwell Bottom-anchor is offset from the progress box's bottom-center, not
    // from the progress top-left.
    QCOMPARE(g.visual.top(), 930.0);
    QCOMPARE(g.visual.bottom(), 1080.0);
    QCOMPARE(g.dwellZone.left(), 810.0);
    QCOMPARE(g.dwellZone.top(), 1120.0);
    QCOMPARE(g.dwellZone.width(), 300.0);
    QCOMPARE(g.dwellZone.height(), 200.0);
}

void PageLoaderTest::edgeChipGazeHitsOnScreenChrome()
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
    QVERIFY(chip->dwellExempt);
    QVERIFY(chip->geom.hidesUntilProgress());
    const QRectF scan = PageHit::gazeHitRect(*chip);
    QVERIFY(scan.contains(chip->geom.dwellZone.center()));
    QVERIFY(!scan.contains(chip->geom.progressZone.center()));
    QVERIFY(PageHit::at(t, chip->geom.progressZone.center()) == nullptr);
    const QRectF acc = PageHit::gazeHitRect(*chip, chip->id);
    QVERIFY(acc.contains(chip->geom.progressZone.center()));
    QVERIFY(PageHit::at(t, chip->geom.progressZone.center(), 1.0, chip->id) == chip);
}

void PageLoaderTest::aboveTaskbarUsesDesktop()
{
    PageGrid g;
    g.id = QStringLiteral("drawer");
    g.desktopMode = false;
    g.aboveTaskbar = true;
    g.anchor = PageAnchor::Bottom;
    g.size.x = PageDim::pixels(400);
    g.size.y = PageDim::pixels(80);
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = QRectF(0, 0, 1920, 1040);
    const QRectF r = PageHit::gridBounds(g, frame);
    QVERIFY(r.bottom() <= frame.desktop.bottom() + 0.51);
    QVERIFY(r.bottom() < frame.screen.bottom() - 1.0);
}

void PageLoaderTest::mainChipsSitAboveTaskbar()
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
    QVERIFY(chip->aboveTaskbar);
    QVERIFY(sleep->aboveTaskbar);
    PageFrame frame;
    frame.screen = QRectF(0, 0, 1920, 1080);
    frame.desktop = QRectF(0, 0, 1920, 1040);
    const QSet<QString> hiddenGrids{QStringLiteral("drawer"), QStringLiteral("quit")};
    const QVector<PageTarget> t = PageHit::collect(doc, frame, hiddenGrids, {});
    const PageTarget* vis = targetById(t, QStringLiteral("mainChip"));
    QVERIFY(vis);
    QVERIFY(vis->geom.visual.bottom() <= frame.desktop.bottom() + 0.51);
    QVERIFY(vis->geom.visual.bottom() < frame.screen.bottom() - 1.0);
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

QTEST_MAIN(PageLoaderTest)
#include "PageLoaderTest.moc"
