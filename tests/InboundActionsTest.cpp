#include "app/InboundActions.h"

#include <QtTest>

using namespace gazer;

class InboundActionsTest final : public QObject {
    Q_OBJECT

private slots:
    void parseEmptyIsOk();
    void parseCommandLine();
    void parseOpenPageAndShowLayersLines();
    void parseXmlFragment();
    void parseAhkElement();
    void parseClosePageBareName();
    void parseSpaceSeparated();
    void rejectUnknown();
    void rejectPhase();
    void payloadFromArgs();
    void forwardPayload();
};

void InboundActionsTest::parseEmptyIsOk()
{
    QVector<PageAction> a;
    QString err;
    QVERIFY2(parseInboundActions(QString(), a, &err), qPrintable(err));
    QCOMPARE(a.size(), 0);
    QVERIFY2(parseInboundActions(QStringLiteral("  \n# comment\n"), a, &err), qPrintable(err));
    QCOMPARE(a.size(), 0);
    QVERIFY2(parseInboundActions(QStringLiteral("raise"), a, &err), qPrintable(err));
    QCOMPARE(a.size(), 0);
    QVERIFY(isInboundRaise(QStringLiteral(" raise ")));
}

void InboundActionsTest::parseCommandLine()
{
    QVector<PageAction> a;
    QString err;
    QVERIFY2(parseInboundActions(QStringLiteral("command=toggleLookToScroll"), a, &err),
             qPrintable(err));
    QCOMPARE(a.size(), 1);
    QCOMPARE(a[0].type, PageActionType::Command);
    QCOMPARE(a[0].command, QStringLiteral("toggleLookToScroll"));
}

void InboundActionsTest::parseOpenPageAndShowLayersLines()
{
    QVector<PageAction> a;
    QString err;
    QVERIFY2(parseInboundActions(QStringLiteral("openPage=qwerty_main\nshowLayers=2"), a, &err),
             qPrintable(err));
    QCOMPARE(a.size(), 2);
    QCOMPARE(a[0].type, PageActionType::Nav);
    QCOMPARE(a[0].targetId, QStringLiteral("qwerty_main"));
    QCOMPARE(a[1].type, PageActionType::ShowLayers);
    QCOMPARE(a[1].layers, QVector<int>({2}));
}

void InboundActionsTest::parseXmlFragment()
{
    QVector<PageAction> a;
    QString err;
    QVERIFY2(parseInboundActions(QStringLiteral(
                                     "<OpenPage value=\"qwerty_main\"/>"
                                     "<ShowLayers value=\"2\"/>"),
                                 a, &err),
             qPrintable(err));
    QCOMPARE(a.size(), 2);
    QCOMPARE(a[0].targetId, QStringLiteral("qwerty_main"));
    QCOMPARE(a[1].layers, QVector<int>({2}));
}

void InboundActionsTest::parseAhkElement()
{
    QVector<PageAction> a;
    QString err;
    QVERIFY2(parseInboundActions(QStringLiteral("<AHK>MsgBox</AHK>"), a, &err), qPrintable(err));
    QCOMPARE(a.size(), 1);
    QCOMPARE(a[0].type, PageActionType::Ahk);
    QCOMPARE(a[0].ahkSource, QStringLiteral("MsgBox"));
}

void InboundActionsTest::parseClosePageBareName()
{
    QVector<PageAction> a;
    QString err;
    QVERIFY2(parseInboundActions(QStringLiteral("closePage"), a, &err), qPrintable(err));
    QCOMPARE(a.size(), 1);
    QCOMPARE(a[0].type, PageActionType::Nav);
    QCOMPARE(a[0].verb, PageVerb::Close);
    QCOMPARE(a[0].targetScope, PageNavScope::Self);
}

void InboundActionsTest::parseSpaceSeparated()
{
    QVector<PageAction> a;
    QString err;
    QVERIFY2(parseInboundActions(QStringLiteral("speak Hello there"), a, &err), qPrintable(err));
    QCOMPARE(a.size(), 1);
    QCOMPARE(a[0].type, PageActionType::Speak);
    QCOMPARE(a[0].speakText, QStringLiteral("Hello there"));
}

void InboundActionsTest::rejectUnknown()
{
    QVector<PageAction> a;
    QString err;
    QVERIFY(!parseInboundActions(QStringLiteral("notAnAction=x"), a, &err));
    QVERIFY(!err.isEmpty());
}

void InboundActionsTest::rejectPhase()
{
    QVector<PageAction> a;
    QString err;
    QVERIFY(!parseInboundActions(QStringLiteral("<Phase command=\"quitApp\"/>"), a, &err));
    QVERIFY(err.contains(QStringLiteral("Phase")));
}

void InboundActionsTest::payloadFromArgs()
{
    const QStringList args{QStringLiteral("Gazer.exe"), QStringLiteral("--action"),
                           QStringLiteral("openPage=qwerty_main"),
                           QStringLiteral("--action=showLayers=2")};
    QCOMPARE(inboundPayloadFromArgs(args), QStringLiteral("openPage=qwerty_main\nshowLayers=2"));
}

void InboundActionsTest::forwardPayload()
{
    const QStringList none{QStringLiteral("Gazer.exe")};
    QCOMPARE(inboundForwardPayload(none), QStringLiteral("raise"));
    const QStringList editor{QStringLiteral("Gazer.exe"), QStringLiteral("--editor")};
    QCOMPARE(inboundForwardPayload(editor), QStringLiteral("command=openPageEditor"));
    const QStringList both{QStringLiteral("Gazer.exe"), QStringLiteral("--editor"),
                           QStringLiteral("--action"),
                           QStringLiteral("command=toggleLookToScroll")};
    QCOMPARE(inboundForwardPayload(both),
             QStringLiteral("command=toggleLookToScroll\ncommand=openPageEditor"));
    const QStringList primary{QStringLiteral("Gazer.exe"), QStringLiteral("--action"),
                              QStringLiteral("command=toggleLookToScroll")};
    QCOMPARE(inboundPayloadFromArgs(primary), QStringLiteral("command=toggleLookToScroll"));
}

QObject* createInboundActionsTest()
{
    return new InboundActionsTest;
}

#include "InboundActionsTest.moc"
