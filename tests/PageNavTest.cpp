#include "layout/PageHit.h"
#include "layout/PageLoader.h"
#include "layout/PageNav.h"
#include "layout/PageWriter.h"

#include <QtTest>

using namespace gazer;

class PageNavTest final : public QObject {
    Q_OBJECT

private slots:
    void pageFindsRootThenAttached();
    void defaultLayersAreOne();
    void layersCsvRoundTrip();
    void offLayerGridIsHidden();
    void showLayersFiltersCollect();
    void dropLayersLeavesDefault();
    void reconcileDrawerAppearDismissSnap();
};

namespace {

[[nodiscard]] bool loadXml(const QByteArray& xml, PageDocument& doc)
{
    QString err;
    if (!PageLoader::loadFromXml(xml, doc, &err)) {
        qWarning("%s", qPrintable(err));
        return false;
    }
    return true;
}

PageNav::Docs docsOf(PageDocument& root, QVector<PageDocument>& attached)
{
    PageNav::Docs d;
    d.root = &root;
    d.attached.reserve(attached.size());
    for (PageDocument& a : attached) {
        d.attached.push_back(&a);
    }
    return d;
}

} // namespace

void PageNavTest::pageFindsRootThenAttached()
{
    PageDocument root;
    PageDocument extra;
    QVERIFY(loadXml(R"xml(
<Page id="root">
  <Grid id="shared" size="10,10"><Cell id="rc" label="r"/></Grid>
</Page>
)xml",
                    root));
    QVERIFY(loadXml(R"xml(
<Page id="extra">
  <Grid id="shared" size="10,10"><Cell id="ec" label="e"/></Grid>
</Page>
)xml",
                    extra));
    QVector<PageDocument> attached{extra};
    const PageNav::Docs docs = docsOf(root, attached);
    QCOMPARE(PageNav::page(docs, QStringLiteral("root")), &root);
    QCOMPARE(PageNav::page(docs, QStringLiteral("extra")), &attached[0]);
    QVERIFY(!PageNav::page(docs, QStringLiteral("missing")));
}

void PageNavTest::defaultLayersAreOne()
{
    PageDocument root;
    QVERIFY(loadXml(R"xml(
<Page id="root">
  <Grid id="board" size="100,100"><Cell id="c" label="C"/></Grid>
  <Zone id="chip" size="40,20"/>
</Page>
)xml",
                    root));
    QCOMPARE(root.showLayers, QVector<int>({1}));
    QCOMPARE(root.findGrid(QStringLiteral("board"))->layers, QVector<int>({1}));
    QCOMPARE(root.findZone(QStringLiteral("chip"))->layers, QVector<int>({1}));
    const QString text = QString::fromUtf8(PageWriter::toBytes(root));
    QVERIFY(!text.contains(QStringLiteral("layers=")));
    QVERIFY(!text.contains(QStringLiteral("showLayers=")));
    QVERIFY(!text.contains(QStringLiteral("show=")));
}

void PageNavTest::layersCsvRoundTrip()
{
    PageDocument root;
    QVERIFY(loadXml(R"xml(
<Page id="root" showLayers="1,3">
  <Grid id="drawer" layers="2" size="200,80"><Cell id="x" label="x"/></Grid>
  <Zone id="sleep" layers="1,2,3" size="40,20"/>
</Page>
)xml",
                    root));
    QCOMPARE(root.showLayers, (QVector<int>{1, 3}));
    QCOMPARE(root.findGrid(QStringLiteral("drawer"))->layers, QVector<int>({2}));
    QCOMPARE(root.findZone(QStringLiteral("sleep"))->layers, (QVector<int>{1, 2, 3}));
    PageDocument round;
    QString err;
    QVERIFY2(PageLoader::loadFromXml(PageWriter::toBytes(root), round, &err), qPrintable(err));
    QCOMPARE(round.showLayers, root.showLayers);
    QCOMPARE(round.findGrid(QStringLiteral("drawer"))->layers, QVector<int>({2}));
    QCOMPARE(round.findZone(QStringLiteral("sleep"))->layers, (QVector<int>{1, 2, 3}));
}

void PageNavTest::offLayerGridIsHidden()
{
    QVERIFY(!layersVisible({2}, {1}));
    QVERIFY(layersVisible({1, 2}, {2}));
    QVERIFY(layersVisible({}, {1}));
    QVERIFY(layersVisible({1}, {}));
}

void PageNavTest::showLayersFiltersCollect()
{
    PageDocument root;
    QVERIFY(loadXml(R"xml(
<Page id="root">
  <Grid id="board" size="100,100"><Cell id="on" label="on"/></Grid>
  <Grid id="drawer" layers="2" size="200,80"><Cell id="off" label="off"/></Grid>
  <Zone id="chip" size="40,20"/>
  <Zone id="hidden" layers="2" size="40,20"/>
</Page>
)xml",
                    root));
    PageFrame frame;
    frame.screen = QRectF(0, 0, 400, 300);
    frame.desktop = frame.screen;
    auto idOf = [](const QVector<PageTarget>& t, const QString& id) {
        for (const PageTarget& x : t) {
            if (x.id == id) {
                return true;
            }
        }
        return false;
    };
    QVector<PageTarget> shown = PageHit::collect(root, frame);
    QVERIFY(idOf(shown, QStringLiteral("on")));
    QVERIFY(!idOf(shown, QStringLiteral("off")));
    QVERIFY(idOf(shown, QStringLiteral("chip")));
    QVERIFY(!idOf(shown, QStringLiteral("hidden")));
    root.showLayers = {2};
    shown = PageHit::collect(root, frame);
    QVERIFY(!idOf(shown, QStringLiteral("on")));
    QVERIFY(idOf(shown, QStringLiteral("off")));
    QVERIFY(!idOf(shown, QStringLiteral("chip")));
    QVERIFY(idOf(shown, QStringLiteral("hidden")));
}

void PageNavTest::dropLayersLeavesDefault()
{
    QVector<int> shown{1, 2};
    PageNav::dropLayers(shown, {2});
    QCOMPARE(shown, QVector<int>({1}));
    PageNav::dropLayers(shown, {1});
    QCOMPARE(shown, QVector<int>({1}));
}

void PageNavTest::reconcileDrawerAppearDismissSnap()
{
    using A = PageNav::DrawerAnim;
    QCOMPARE(PageNav::reconcileDrawer(false, true, false, false), A::Appear);
    QCOMPARE(PageNav::reconcileDrawer(true, true, false, false), A::Keep);
    QCOMPARE(PageNav::reconcileDrawer(true, false, false, false), A::Dismiss);
    QCOMPARE(PageNav::reconcileDrawer(true, false, true, false), A::Snap);
    QCOMPARE(PageNav::reconcileDrawer(false, false, true, true), A::Snap);
    QCOMPARE(PageNav::reconcileDrawer(false, true, false, true), A::Appear);
    QCOMPARE(PageNav::reconcileDrawer(false, false, false, true), A::Keep);
    QCOMPARE(PageNav::reconcileDrawer(false, false, false, false), A::Keep);

    PageDocument root;
    QVERIFY(loadXml(R"xml(
<Page id="main" master="true" showLayers="1,2">
  <Grid id="drawer" layers="2" drawerMotion="true" size="200,80"/>
  <Grid id="quit" layers="3" size="80,40"/>
</Page>
)xml",
                    root));
    const bool wasDrawer = layersVisible(root.findGrid(QStringLiteral("drawer"))->layers,
                                         root.showLayers)
                           && root.findGrid(QStringLiteral("drawer"))->drawerMotion;
    root.showLayers = {1, 3};
    const bool nowDrawer = layersVisible(root.findGrid(QStringLiteral("drawer"))->layers,
                                         root.showLayers)
                           && root.findGrid(QStringLiteral("drawer"))->drawerMotion;
    const bool otherRoot = layersVisible(root.findGrid(QStringLiteral("quit"))->layers,
                                         root.showLayers)
                           && !root.findGrid(QStringLiteral("quit"))->drawerMotion;
    QCOMPARE(PageNav::reconcileDrawer(wasDrawer, nowDrawer, otherRoot, false), A::Snap);
    QVERIFY(!nowDrawer);
    QVERIFY(otherRoot);

    QCOMPARE(PageNav::reconcileDrawer(true, false, false, false), A::Dismiss);
}

QObject* createPageNavTest()
{
    return new PageNavTest;
}

#include "PageNavTest.moc"
