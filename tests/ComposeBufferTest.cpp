#include "assist/ComposeBuffer.h"

#include <QtTest>

using gazer::ComposeBuffer;

class ComposeBufferTest final : public QObject {
    Q_OBJECT

private slots:
    void insertAndBackspace();
    void deleteWord();
    void undoCoalesceInsert();
    void undoDoesNotCoalesceAfterGap();
    void redo();
    void tokenize();
    void visibleWindowMapping();
    void removeVisibleWordMapsLastTwelve();
    void tagPadding();
    void insertTagWrapsBrackets();
    void clearResets();
    void loadReplacesAndDropsHistory();
    void setCaretDoesNotPushUndo();
    void moveCaretToTokenEdge();
};

void ComposeBufferTest::insertAndBackspace()
{
    ComposeBuffer b;
    b.insert(QStringLiteral("ab"));
    QCOMPARE(b.text(), QStringLiteral("ab"));
    QCOMPARE(b.caret(), 2);
    b.backspace();
    QCOMPARE(b.text(), QStringLiteral("a"));
    QCOMPARE(b.caret(), 1);
    b.backspace();
    b.backspace();
    QCOMPARE(b.text(), QString());
    QCOMPARE(b.caret(), 0);
}

void ComposeBufferTest::deleteWord()
{
    ComposeBuffer b;
    b.insert(QStringLiteral("hello world"));
    b.deleteWord();
    QCOMPARE(b.text(), QStringLiteral("hello "));
    b.deleteWord();
    QCOMPARE(b.text(), QString());
}

void ComposeBufferTest::undoCoalesceInsert()
{
    ComposeBuffer b;
    b.setNowMsForTest(0);
    b.insert(QStringLiteral("a"));
    b.setNowMsForTest(100);
    b.insert(QStringLiteral("b"));
    b.setNowMsForTest(200);
    b.insert(QStringLiteral("c"));
    QCOMPARE(b.text(), QStringLiteral("abc"));
    QVERIFY(b.undo());
    QCOMPARE(b.text(), QString());
    QVERIFY(!b.undo());
}

void ComposeBufferTest::undoDoesNotCoalesceAfterGap()
{
    ComposeBuffer b;
    b.setNowMsForTest(0);
    b.insert(QStringLiteral("a"));
    b.setNowMsForTest(ComposeBuffer::kCoalesceMs + 1);
    b.insert(QStringLiteral("b"));
    QCOMPARE(b.text(), QStringLiteral("ab"));
    QVERIFY(b.undo());
    QCOMPARE(b.text(), QStringLiteral("a"));
    QVERIFY(b.undo());
    QCOMPARE(b.text(), QString());
}

void ComposeBufferTest::redo()
{
    ComposeBuffer b;
    b.setNowMsForTest(0);
    b.insert(QStringLiteral("hi"));
    b.undo();
    QVERIFY(b.redo());
    QCOMPARE(b.text(), QStringLiteral("hi"));
    QCOMPARE(b.caret(), 2);
}

void ComposeBufferTest::tokenize()
{
    ComposeBuffer b;
    b.insert(QStringLiteral("  Hello   [laugh]  there "));
    const auto toks = b.tokens();
    QCOMPARE(toks.size(), 3);
    QCOMPARE(toks[0].text, QStringLiteral("Hello"));
    QCOMPARE(toks[1].text, QStringLiteral("[laugh]"));
    QCOMPARE(toks[2].text, QStringLiteral("there"));
}

void ComposeBufferTest::visibleWindowMapping()
{
    ComposeBuffer b;
    b.setNowMsForTest(0);
    for (int i = 0; i < 15; ++i) {
        b.setNowMsForTest(i * (ComposeBuffer::kCoalesceMs + 1));
        b.insertPadded(QString::number(i));
    }
    QCOMPARE(b.tokens().size(), 15);
    QCOMPARE(b.visibleTokens().size(), 12);
    QCOMPARE(b.tokenIndexForVisibleSlot(0), 3);
    QCOMPARE(b.tokenIndexForVisibleSlot(11), 14);
    QCOMPARE(b.tokenIndexForVisibleSlot(12), -1);
    QCOMPARE(b.visibleTokens().front().text, QStringLiteral("3"));
    QCOMPARE(b.visibleTokens().back().text, QStringLiteral("14"));
}

void ComposeBufferTest::removeVisibleWordMapsLastTwelve()
{
    ComposeBuffer b;
    b.setNowMsForTest(0);
    for (int i = 0; i < 15; ++i) {
        b.setNowMsForTest(i * (ComposeBuffer::kCoalesceMs + 1));
        b.insertPadded(QString::number(i));
    }
    // slot 0 = token 3 ("3")
    b.removeVisibleWord(0);
    const auto toks = b.tokens();
    QCOMPARE(toks.size(), 14);
    QCOMPARE(toks[3].text, QStringLiteral("4"));
}

void ComposeBufferTest::tagPadding()
{
    ComposeBuffer b;
    b.insert(QStringLiteral("Hello"));
    b.insertTag(QStringLiteral("laugh"));
    QCOMPARE(b.text(), QStringLiteral("Hello [laugh]"));
}

void ComposeBufferTest::insertTagWrapsBrackets()
{
    ComposeBuffer b;
    b.insertTag(QStringLiteral("[cry]"));
    QCOMPARE(b.text(), QStringLiteral("[cry]"));
    b.insertTag(QStringLiteral("loud"));
    QCOMPARE(b.text(), QStringLiteral("[cry] [loud]"));
}

void ComposeBufferTest::clearResets()
{
    ComposeBuffer b;
    b.insert(QStringLiteral("x"));
    b.clear();
    QCOMPARE(b.text(), QString());
    QVERIFY(b.undo());
    QCOMPARE(b.text(), QStringLiteral("x"));
}

void ComposeBufferTest::loadReplacesAndDropsHistory()
{
    ComposeBuffer b;
    b.insert(QStringLiteral("hello"));
    b.load(QStringLiteral("Topic 1"));
    QCOMPARE(b.text(), QStringLiteral("Topic 1"));
    QCOMPARE(b.caret(), 7);
    QVERIFY(!b.undo());
    b.insert(QStringLiteral("x"));
    QCOMPARE(b.text(), QStringLiteral("Topic 1x"));
    QVERIFY(b.undo());
    QCOMPARE(b.text(), QStringLiteral("Topic 1"));
}

void ComposeBufferTest::setCaretDoesNotPushUndo()
{
    ComposeBuffer b;
    b.insert(QStringLiteral("hello"));
    b.setCaret(1);
    QCOMPARE(b.caret(), 1);
    QVERIFY(b.undo());
    QCOMPARE(b.text(), QString());
}

void ComposeBufferTest::moveCaretToTokenEdge()
{
    ComposeBuffer b;
    b.insert(QStringLiteral("hello world"));
    b.moveCaretToTokenEdge(0, true);
    QCOMPARE(b.caret(), 0);
    b.moveCaretToTokenEdge(0, false);
    QCOMPARE(b.caret(), 5);
    b.moveCaretToTokenEdge(1, true);
    QCOMPARE(b.caret(), 6);
    b.moveCaretToTokenEdge(1, false);
    QCOMPARE(b.caret(), 11);
    b.moveCaretToVisibleWordEdge(0, true);
    QCOMPARE(b.caret(), 0);
    b.moveCaretToVisibleWordEdge(1, false);
    QCOMPARE(b.caret(), 11);
}

QObject* createComposeBufferTest()
{
    return new ComposeBufferTest;
}

#include "ComposeBufferTest.moc"
