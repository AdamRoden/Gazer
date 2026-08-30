#include "layout/PageLoader.h"
#include "layout/PageNav.h"
#include "layout/PageWriter.h"

#include <QtTest>

using namespace gazer;

class PageNavTest final : public QObject {
    Q_OBJECT

private slots:
    void locatePrefersPageThenRootThenAttached();
    void hideById();
    void hideGridAllIncludesDrawerAndQuit();
    void legacyChromeAttrLoadsHidden();
    void hideZoneAndCellAll();
    void hideOthersSkipsSelf();
    void toggleAndShow();
    void breadcrumbCopyRestoresShow();
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

void PageNavTest::locatePrefersPageThenRootThenAttached()
{
    PageDocument root;
    PageDocument extra;
    QVERIFY(loadXml(R"xml(
<Page id="root">
  <Grid id="shared" size="10,10"><Cell id="rc" label="r"/></Grid>
  <Zone id="zroot" size="10,10"/>
</Page>
)xml",
                    root));
    QVERIFY(loadXml(R"xml(
<Page id="extra">
  <Grid id="shared" size="10,10"><Cell id="ec" label="e"/></Grid>
  <Zone id="zextra" size="10,10"/>
</Page>
)xml",
                    extra));
    QVector<PageDocument> attached{extra};
    const PageNav::Docs docs = docsOf(root, attached);

    PageGrid* prefer = PageNav::locate(docs, QStringLiteral("extra"), QStringLiteral("shared"),
                                       &PageDocument::findGrid);
    QVERIFY(prefer);
    QCOMPARE(prefer, extra.findGrid(QStringLiteral("shared")));

    PageGrid* fallback = PageNav::locate(docs, {}, QStringLiteral("shared"), &PageDocument::findGrid);
    QVERIFY(fallback);
    QCOMPARE(fallback, root.findGrid(QStringLiteral("shared")));

    PageZone* z = PageNav::locate(docs, {}, QStringLiteral("zextra"), &PageDocument::findZone);
    QVERIFY(z);
    QCOMPARE(z, extra.findZone(QStringLiteral("zextra")));

    PageCell* c = PageNav::locate(docs, QStringLiteral("extra"), QStringLiteral("ec"),
                                  &PageDocument::findCell);
    QVERIFY(c);
    QCOMPARE(c->id, QStringLiteral("ec"));
}

void PageNavTest::hideById()
{
    PageDocument root;
    QVERIFY(loadXml(R"xml(
<Page id="root">
  <Grid id="board" size="100,100">
    <Cell id="k_q" label="Q"/>
    <Cell id="k_w" label="W"/>
  </Grid>
  <Zone id="chip" size="40,20"/>
</Page>
)xml",
                    root));
    QVector<PageDocument> attached;
    const PageNav::Docs docs = docsOf(root, attached);

    PageGrid* g = PageNav::locate(docs, {}, QStringLiteral("board"), &PageDocument::findGrid);
    QVERIFY(g);
    QVERIFY(g->show);
    g->show = PageNav::shownAfter(PageVerb::Close, g->show);
    QVERIFY(!g->show);

    PageCell* c = PageNav::locate(docs, {}, QStringLiteral("k_q"), &PageDocument::findCell);
    QVERIFY(c);
    c->show = PageNav::shownAfter(PageVerb::Close, c->show);
    QVERIFY(!c->show);
    QVERIFY(root.findCell(QStringLiteral("k_w"))->show);

    PageZone* z = PageNav::locate(docs, {}, QStringLiteral("chip"), &PageDocument::findZone);
    QVERIFY(z);
    z->show = PageNav::shownAfter(PageVerb::Toggle, z->show);
    QVERIFY(!z->show);
    z->show = PageNav::shownAfter(PageVerb::Toggle, z->show);
    QVERIFY(z->show);
}

void PageNavTest::hideGridAllIncludesDrawerAndQuit()
{
    PageDocument root;
    QVERIFY(loadXml(R"xml(
<Page id="main" master="true">
  <Grid id="dock" size="100,40"><Cell id="d" label="d"/></Grid>
  <Grid id="drawer" show="false" size="200,80"><Cell id="x" label="x"/></Grid>
  <Grid id="quit" show="false" size="80,40"><Cell id="q" label="q"/></Grid>
  <Grid id="board" size="300,100">
    <SubGrid id="nested" row="0" col="0"><Cell id="n" label="n"/></SubGrid>
  </Grid>
</Page>
)xml",
                    root));
    QVector<PageDocument> attached;
    const PageNav::Docs docs = docsOf(root, attached);

    QCOMPARE(root.findGrid(QStringLiteral("drawer"))->show, false);
    QCOMPARE(root.findGrid(QStringLiteral("quit"))->show, false);

    PageNav::applyScope(docs, PageTargetKind::Grid, PageVerb::Open, {}, {});
    QVERIFY(root.findGrid(QStringLiteral("dock"))->show);
    QVERIFY(root.findGrid(QStringLiteral("board"))->show);
    QVERIFY(root.findGrid(QStringLiteral("nested"))->show);
    QVERIFY(root.findGrid(QStringLiteral("drawer"))->show);
    QVERIFY(root.findGrid(QStringLiteral("quit"))->show);

    PageNav::applyScope(docs, PageTargetKind::Grid, PageVerb::Close, {}, {});
    QVERIFY(!root.findGrid(QStringLiteral("dock"))->show);
    QVERIFY(!root.findGrid(QStringLiteral("board"))->show);
    QVERIFY(!root.findGrid(QStringLiteral("nested"))->show);
    QVERIFY(!root.findGrid(QStringLiteral("drawer"))->show);
    QVERIFY(!root.findGrid(QStringLiteral("quit"))->show);
}

void PageNavTest::legacyChromeAttrLoadsHidden()
{
    PageDocument root;
    QVERIFY(loadXml(R"xml(
<Page id="main" master="true">
  <Grid id="drawer" chrome="drawer" size="200,80"><Cell id="x" label="x"/></Grid>
  <Grid id="quit" chrome="quit" size="80,40"><Cell id="q" label="q"/></Grid>
  <Grid id="shown" chrome="drawer" show="true" size="10,10"/>
</Page>
)xml",
                    root));
    QCOMPARE(root.findGrid(QStringLiteral("drawer"))->show, false);
    QCOMPARE(root.findGrid(QStringLiteral("quit"))->show, false);
    QCOMPARE(root.findGrid(QStringLiteral("shown"))->show, true);
    const QString text = QString::fromUtf8(PageWriter::toBytes(root));
    QVERIFY(!text.contains(QStringLiteral("chrome=")));
}

void PageNavTest::hideZoneAndCellAll()
{
    PageDocument root;
    QVERIFY(loadXml(R"xml(
<Page id="root">
  <Grid id="g" size="100,100">
    <Cell id="a" label="A"/>
    <Cell id="b" label="B"/>
  </Grid>
  <Zone id="z1" size="10,10"/>
  <Zone id="z2" size="10,10"/>
</Page>
)xml",
                    root));
    QVector<PageDocument> attached;
    const PageNav::Docs docs = docsOf(root, attached);
    PageNav::applyScope(docs, PageTargetKind::Zone, PageVerb::Close, {}, {});
    QVERIFY(!root.findZone(QStringLiteral("z1"))->show);
    QVERIFY(!root.findZone(QStringLiteral("z2"))->show);
    PageNav::applyScope(docs, PageTargetKind::Cell, PageVerb::Close, {}, {});
    QVERIFY(!root.findCell(QStringLiteral("a"))->show);
    QVERIFY(!root.findCell(QStringLiteral("b"))->show);
}

void PageNavTest::hideOthersSkipsSelf()
{
    PageDocument root;
    QVERIFY(loadXml(R"xml(
<Page id="root">
  <Grid id="keep" size="100,100">
    <Cell id="self" label="S"/>
    <Cell id="other" label="O"/>
  </Grid>
  <Grid id="gone" size="50,50"><Cell id="g" label="g"/></Grid>
  <Zone id="here" size="10,10"/>
  <Zone id="away" size="10,10"/>
</Page>
)xml",
                    root));
    QVector<PageDocument> attached;
    const PageNav::Docs docs = docsOf(root, attached);
    PageNav::applyScope(docs, PageTargetKind::Grid, PageVerb::Close, QStringLiteral("keep"),
                        QStringLiteral("root"));
    QVERIFY(root.findGrid(QStringLiteral("keep"))->show);
    QVERIFY(!root.findGrid(QStringLiteral("gone"))->show);

    PageNav::applyScope(docs, PageTargetKind::Cell, PageVerb::Close, QStringLiteral("self"),
                        QStringLiteral("root"));
    QVERIFY(root.findCell(QStringLiteral("self"))->show);
    QVERIFY(!root.findCell(QStringLiteral("other"))->show);

    PageNav::applyScope(docs, PageTargetKind::Zone, PageVerb::Close, QStringLiteral("here"), {});
    QVERIFY(root.findZone(QStringLiteral("here"))->show);
    QVERIFY(!root.findZone(QStringLiteral("away"))->show);
}

void PageNavTest::toggleAndShow()
{
    PageDocument root;
    QVERIFY(loadXml(R"xml(
<Page id="root">
  <Grid id="g" show="false" size="100,100"><Cell id="c" show="false" label="C"/></Grid>
</Page>
)xml",
                    root));
    QVector<PageDocument> attached;
    const PageNav::Docs docs = docsOf(root, attached);
    PageNav::applyScope(docs, PageTargetKind::Grid, PageVerb::Open, {}, {});
    QVERIFY(root.findGrid(QStringLiteral("g"))->show);
    PageNav::applyScope(docs, PageTargetKind::Cell, PageVerb::Toggle, {}, {});
    QVERIFY(root.findCell(QStringLiteral("c"))->show);
    PageNav::applyScope(docs, PageTargetKind::Cell, PageVerb::Toggle, {}, {});
    QVERIFY(!root.findCell(QStringLiteral("c"))->show);
}

void PageNavTest::breadcrumbCopyRestoresShow()
{
    PageDocument root;
    QVERIFY(loadXml(R"xml(
<Page id="root">
  <Grid id="g" size="100,100">
    <Cell id="c" label="C"/>
  </Grid>
  <Zone id="z" size="10,10"/>
</Page>
)xml",
                    root));
    const PageDocument snap = root;
    QVector<PageDocument> attached;
    const PageNav::Docs docs = docsOf(root, attached);
    PageNav::applyScope(docs, PageTargetKind::Grid, PageVerb::Close, {}, {});
    PageNav::applyScope(docs, PageTargetKind::Cell, PageVerb::Close, {}, {});
    PageNav::applyScope(docs, PageTargetKind::Zone, PageVerb::Close, {}, {});
    QVERIFY(!root.findGrid(QStringLiteral("g"))->show);
    QVERIFY(!root.findCell(QStringLiteral("c"))->show);
    QVERIFY(!root.findZone(QStringLiteral("z"))->show);

    root = snap;
    QVERIFY(root.findGrid(QStringLiteral("g"))->show);
    QVERIFY(root.findCell(QStringLiteral("c"))->show);
    QVERIFY(root.findZone(QStringLiteral("z"))->show);

    PageDocument written;
    QString err;
    QVERIFY2(PageLoader::loadFromXml(PageWriter::toBytes(snap), written, &err), qPrintable(err));
    QVERIFY(written.findGrid(QStringLiteral("g"))->show);
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
<Page id="main" master="true">
  <Grid id="drawer" show="true" drawerMotion="true" size="200,80"/>
  <Grid id="quit" show="false" size="80,40"/>
</Page>
)xml",
                    root));
    const bool wasDrawer = root.findGrid(QStringLiteral("drawer"))->show
                           && root.findGrid(QStringLiteral("drawer"))->drawerMotion;
    QVector<PageDocument> attached;
    const PageNav::Docs docs = docsOf(root, attached);
    PageAction hide;
    hide.type = PageActionType::Nav;
    hide.verb = PageVerb::Close;
    hide.targetKind = PageTargetKind::Grid;
    hide.targetId = QStringLiteral("drawer");
    PageAction showQuit;
    showQuit.type = PageActionType::Nav;
    showQuit.verb = PageVerb::Open;
    showQuit.targetKind = PageTargetKind::Grid;
    showQuit.targetId = QStringLiteral("quit");
    PageGrid* drawer = PageNav::locate(docs, {}, hide.targetId, &PageDocument::findGrid);
    PageGrid* quit = PageNav::locate(docs, {}, showQuit.targetId, &PageDocument::findGrid);
    QVERIFY(drawer);
    QVERIFY(quit);
    drawer->show = PageNav::shownAfter(hide.verb, drawer->show);
    quit->show = PageNav::shownAfter(showQuit.verb, quit->show);
    const bool nowDrawer = drawer->show && drawer->drawerMotion;
    const bool otherRoot = quit->show && !quit->drawerMotion;
    QCOMPARE(PageNav::reconcileDrawer(wasDrawer, nowDrawer, otherRoot, false), A::Snap);
    QVERIFY(!drawer->show);
    QVERIFY(quit->show);

    drawer->show = false;
    quit->show = false;
    QCOMPARE(PageNav::reconcileDrawer(true, false, false, false), A::Dismiss);
}

QObject* createPageNavTest()
{
    return new PageNavTest;
}

#include "PageNavTest.moc"
