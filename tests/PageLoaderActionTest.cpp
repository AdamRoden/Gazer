#include "layout/PageLoader.h"
#include "layout/PageWriter.h"

#include <QStringList>
#include <QtTest>

using namespace gazer;

class PageLoaderActionTest final : public QObject {
    Q_OBJECT

private slots:
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
};

void PageLoaderActionTest::actionExtrasRoundTrip()
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

void PageLoaderActionTest::actionEmptyEdgeRoundTrip()
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

void PageLoaderActionTest::parseClickDownAsEdge()
{
    PageAction a;
    QString err;
    QVERIFY2(loadOneAction(QByteArray("<Action id=\"Click\" value=\"left, Down\"/>"), a, &err),
             qPrintable(err));
    QCOMPARE(a.type, PageActionType::Click);
    QCOMPARE(a.button, QStringLiteral("left"));
    QCOMPARE(a.clickKind, PageClickKind::Down);
}

void PageLoaderActionTest::parseSendDurationWithoutEdge()
{
    PageAction a;
    QString err;
    QVERIFY2(loadOneAction(QByteArray("<Action id=\"Send\" value=\"Enter, 40\"/>"), a, &err),
             qPrintable(err));
    QCOMPARE(a.sendKey, QStringLiteral("Enter"));
    QCOMPARE(a.sendEdge, QString());
    QCOMPARE(a.sendDurationMs, 40);
}

void PageLoaderActionTest::rejectNonIntegerClickCount()
{
    PageAction a;
    QString err;
    QVERIFY(!loadOneAction(QByteArray("<Action id=\"Click\" value=\"left, foo\"/>"), a, &err));
    QVERIFY(err.contains(QStringLiteral("integer")));
}

void PageLoaderActionTest::cellSendAttribute()
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

void PageLoaderActionTest::rejectTwoActionAttributes()
{
    PageDocument doc;
    QString err;
    const QByteArray xml =
        QByteArray("<Page id=\"p\"><Grid id=\"g\"><Cell id=\"c\" send=\"a\" click=\"left\"/>"
                   "</Grid></Page>");
    QVERIFY(!PageLoader::loadFromXml(xml, doc, &err));
    QVERIFY(err.contains(QStringLiteral("1 action attribute")));
}

void PageLoaderActionTest::parseSpecificActionElements()
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

void PageLoaderActionTest::parseMoveVariants()
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

void PageLoaderActionTest::parseOpenPageBreadcrumb()
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

void PageLoaderActionTest::parseCloseSpecialsAndGoBack()
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

void PageLoaderActionTest::genericActionAttribute()
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


QObject* createPageLoaderActionTest()
{
    return new PageLoaderActionTest;
}

#include "PageLoaderActionTest.moc"
