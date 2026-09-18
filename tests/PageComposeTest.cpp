#include "layout/PageCompose.h"
#include "layout/PageHit.h"
#include "layout/PageLoader.h"
#include "layout/PageWriter.h"

#include <QHash>
#include <QtTest>
#include <optional>

using namespace gazer;

class PageComposeTest final : public QObject {
    Q_OBJECT

private slots:
    void srcRoundTripAndRejectsChildren();
    void parseHostPage();
    void collectInlinesFragmentInSlot();
    void currentHostTabIsPassive();
    void resolveCycle();
};

namespace {

bool loadXml(const QByteArray& xml, PageDocument& doc, QString* err)
{
    return PageLoader::loadFromXml(xml, doc, err);
}

PageFrame frame800()
{
    PageFrame f;
    f.screen = QRectF(0, 0, 800, 400);
    f.desktop = f.screen;
    return f;
}

const PageTarget* byId(const QVector<PageTarget>& ts, const QString& id)
{
    for (const PageTarget& t : ts) {
        if (t.id == id) {
            return &t;
        }
    }
    return nullptr;
}

} // namespace

void PageComposeTest::srcRoundTripAndRejectsChildren()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadXml(R"xml(
<Page id="host">
  <Grid id="board" size="800,400" rows="2" columns="1">
    <SubGrid id="body" row="1" col="0" src="frag" grid="board"/>
  </Grid>
</Page>
)xml",
                     doc, &err),
             qPrintable(err));
    QCOMPARE(doc.grids[0].subGrids.size(), 1);
    QCOMPARE(doc.grids[0].subGrids[0].src, QStringLiteral("frag"));
    QCOMPARE(doc.grids[0].subGrids[0].srcGrid, QStringLiteral("board"));
    PageDocument written;
    QVERIFY2(PageLoader::loadFromXml(PageWriter::toBytes(doc), written, &err), qPrintable(err));
    QCOMPARE(written.grids[0].subGrids[0].src, QStringLiteral("frag"));
    QCOMPARE(written.grids[0].subGrids[0].srcGrid, QStringLiteral("board"));
    QVERIFY(written.grids[0].subGrids[0].cells.isEmpty());

    PageDocument bad;
    QVERIFY(!loadXml(R"xml(
<Page id="host">
  <Grid id="board" size="800,400">
    <SubGrid id="body" src="frag">
      <Cell id="nope" row="0" col="0"/>
    </SubGrid>
  </Grid>
</Page>
)xml",
                     bad, &err));
    QVERIFY(err.contains(QStringLiteral("src grid cannot have children")));
}

void PageComposeTest::parseHostPage()
{
    PageDocument doc;
    QString err;
    QVERIFY2(loadXml(R"xml(
<Page id="p">
  <Grid id="g" size="100,100">
    <Cell id="a" hostPage="main_settings_speed"/>
    <Cell id="b" hostPage="main_settings_host, main_settings_theme"/>
  </Grid>
</Page>
)xml",
                     doc, &err),
             qPrintable(err));
    const PageCell* a = doc.findCell(QStringLiteral("a"));
    const PageCell* b = doc.findCell(QStringLiteral("b"));
    QVERIFY(a && b);
    QCOMPARE(a->actions.size(), 1);
    QCOMPARE(a->actions[0].type, PageActionType::HostPage);
    QCOMPARE(a->actions[0].targetId, QStringLiteral("main_settings_speed"));
    QVERIFY(a->actions[0].hostId.isEmpty());
    QCOMPARE(b->actions[0].type, PageActionType::HostPage);
    QCOMPARE(b->actions[0].hostId, QStringLiteral("main_settings_host"));
    QCOMPARE(b->actions[0].targetId, QStringLiteral("main_settings_theme"));
}

void PageComposeTest::collectInlinesFragmentInSlot()
{
    PageDocument host;
    PageDocument frag;
    QString err;
    QVERIFY2(loadXml(R"xml(
<Page id="host">
  <Grid id="board" size="800,400" rows="2" columns="1">
    <Cell id="tab" row="0" col="0" label="Tab" command="resumeDwell"/>
    <SubGrid id="body" row="1" col="0" src="frag" grid="board"/>
  </Grid>
</Page>
)xml",
                     host, &err),
             qPrintable(err));
    QVERIFY2(loadXml(R"xml(
<Page id="frag">
  <Grid id="board" size="100,100" rows="1" columns="2">
    <Cell id="hello" row="0" col="0" label="Hi" command="resumeDwell"/>
    <Cell id="world" row="0" col="1" label="Yo" command="resumeDwell"/>
  </Grid>
</Page>
)xml",
                     frag, &err),
             qPrintable(err));

    QHash<QString, PageDocument> hosted;
    hosted.insert(frag.id, frag);
    const auto ptrs = PageCompose::pointers(hosted);
    QVector<PageGridPaint> grids;
    const QVector<PageTarget> t =
        PageHit::collect(host, frame800(), {}, false, &grids, false, std::nullopt, &ptrs);
    const PageTarget* tab = byId(t, QStringLiteral("tab"));
    const PageTarget* hello = byId(t, QStringLiteral("hello"));
    const PageTarget* world = byId(t, QStringLiteral("world"));
    QVERIFY(tab);
    QVERIFY(hello);
    QVERIFY(world);
    QCOMPARE(tab->pageId, QStringLiteral("host"));
    QCOMPARE(hello->pageId, QStringLiteral("frag"));
    QCOMPARE(world->pageId, QStringLiteral("frag"));
    QVERIFY(tab->geom.visual.center().y() < hello->geom.visual.center().y());
    QVERIFY(hello->geom.visual.center().x() < world->geom.visual.center().x());
    QVERIFY(hello->geom.visual.top() >= 190.0);

    const PageTarget* hitTab =
        PageHit::at(t, tab->geom.visual.center(), 1.0, {}, grids);
    const PageTarget* hitBody =
        PageHit::at(t, hello->geom.visual.center(), 1.0, {}, grids);
    QVERIFY(hitTab);
    QCOMPARE(hitTab->id, QStringLiteral("tab"));
    QVERIFY(hitBody);
    QCOMPARE(hitBody->id, QStringLiteral("hello"));
}

void PageComposeTest::currentHostTabIsPassive()
{
    PageDocument host;
    PageDocument frag;
    QString err;
    QVERIFY2(loadXml(R"xml(
<Page id="host">
  <Grid id="board" size="800,400" rows="2" columns="2">
    <Cell id="tab_on" row="0" col="0" role="tab" hostPage="frag"/>
    <Cell id="tab_off" row="0" col="1" role="tab" hostPage="other"/>
    <SubGrid id="body" row="1" col="0" colSpan="2" src="frag"/>
  </Grid>
</Page>
)xml",
                     host, &err),
             qPrintable(err));
    QVERIFY2(loadXml(R"xml(
<Page id="frag">
  <Grid id="board" size="100,100">
    <Cell id="x" row="0" col="0"/>
  </Grid>
</Page>
)xml",
                     frag, &err),
             qPrintable(err));
    QHash<QString, PageDocument> hosted;
    hosted.insert(frag.id, frag);
    const auto ptrs = PageCompose::pointers(hosted);
    const QVector<PageTarget> t =
        PageHit::collect(host, frame800(), {}, false, nullptr, false, std::nullopt, &ptrs);
    const PageTarget* on = byId(t, QStringLiteral("tab_on"));
    const PageTarget* off = byId(t, QStringLiteral("tab_off"));
    QVERIFY(on);
    QVERIFY(off);
    QVERIFY(!on->interactive);
    QVERIFY(on->actions.isEmpty());
    QVERIFY(off->interactive);
    QCOMPARE(off->actions.size(), 1);
}

void PageComposeTest::resolveCycle()
{
    PageDocument a;
    PageDocument b;
    QString err;
    QVERIFY2(loadXml(R"xml(
<Page id="a">
  <Grid id="board" size="10,10">
    <SubGrid id="body" src="b"/>
  </Grid>
</Page>
)xml",
                     a, &err),
             qPrintable(err));
    QVERIFY2(loadXml(R"xml(
<Page id="b">
  <Grid id="board" size="10,10">
    <SubGrid id="body" src="a"/>
  </Grid>
</Page>
)xml",
                     b, &err),
             qPrintable(err));
    QHash<QString, PageDocument> store;
    store.insert(QStringLiteral("a"), a);
    store.insert(QStringLiteral("b"), b);
    const auto load = [&](const QString& id, PageDocument& out, QString* e) {
        if (!store.contains(id)) {
            if (e) {
                *e = QStringLiteral("missing");
            }
            return false;
        }
        out = store.value(id);
        return true;
    };
    QHash<QString, PageDocument> out;
    QString cycleErr;
    QVERIFY(!PageCompose::resolveAll(a, load, out, &cycleErr));
    QVERIFY(cycleErr.contains(QStringLiteral("src cycle")));
}

QObject* createPageComposeTest()
{
    return new PageComposeTest;
}

#include "PageComposeTest.moc"
