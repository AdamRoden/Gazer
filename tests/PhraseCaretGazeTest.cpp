#include "ui/PhraseCaretChoose.h"
#include "ui/PhraseLayout.h"

#include <QtTest>

using namespace gazer;

class PhraseCaretChooseTest final : public QObject {
    Q_OBJECT

private slots:
    void wordHitPicksTheGlyph();
    void wordsSplitOnNonAlphanumeric();
    void gapBetweenWordsMisses();
    void lookShortOfDwellDoesNotFire();
    void lookAtDwellFiresAndLatches();
    void wordGrowsByHalf();
    void wordScanThenDwell();
    void shiftLayerSelects();
    void arrowKeyRepeatsItsLastStep();
    void latchedLookDoesNotRefire();
    void leavingTheTargetResetsTheLook();
    void aNewTargetStartsOver();
    void leftMovesOneCharacterAndClearsSelection();
    void rightMovesOneCharacterAndClearsSelection();
    void arrowsStepOneCharacter();
    void shiftArrowExtendsOneCharacter();
    void shiftArrowShrinksFromTheCaret();
    void arrowsStayInsideThePhrase();
    void shiftWordExtendsAcrossTheGap();
    void wordInsideTheSelectionStaysPut();
    void homeWithoutShiftMovesToTheStart();
    void endWithoutShiftMovesToTheEnd();
    void homeAlreadyAtTheStartDoesNotChange();
    void shiftHomeExtendsToTheStart();
    void shiftEndExtendsToTheEnd();
    void shiftHomeKeepsTheFarEdge();
    void shiftEndKeepsTheNearEdge();
    void boldWordDwellIsThreeLineHeights();
    void wrappedWordDoesNotFillTheGap();
    void wordPastTheLineEdgeIsNotAHit();
    void caretOnAWrapStaysOnTheEarlierLine();
    void copyBottomSitsOnThePhraseTop();
    void copyClampsIntoTheScreen();
    void pointBelowThePhraseMissesCopy();
    void captionUsesLabelRect();
    void phraseFontStaysBodySize();
    void wrappedBlockIsTaller();
    void rowGrowsPastTheBase();
};

QVector<PhraseCaretToken> helloWorld()
{
    return {PhraseCaretToken{0, 5}, PhraseCaretToken{6, 11}};
}

PhraseCaretLine oneLine()
{
    PhraseCaretLine line;
    line.start = 0;
    line.end = 11;
    line.rect = QRectF(10, 100, 110, 20);
    line.cursorX.resize(12);
    for (int i = 0; i < line.cursorX.size(); ++i) {
        line.cursorX[i] = 10.0 + i * 10.0;
    }
    return line;
}

void PhraseCaretChooseTest::wordHitPicksTheGlyph()
{
    const QVector<PhraseWordSpan> words = phraseWordSpans({oneLine()}, helloWorld());
    QCOMPARE(words.size(), 2);
    QCOMPARE(phraseWordIndexAt(words, QPointF(15, 110)), 0);
    QCOMPARE(phraseWordIndexAt(words, QPointF(75, 110)), 1);
    QCOMPARE(words[0].start, 0);
    QCOMPARE(words[0].end, 5);
}

void PhraseCaretChooseTest::wordsSplitOnNonAlphanumeric()
{
    const QVector<PhraseCaretToken> file =
        alphanumericCaretTokens(QStringLiteral("aac-workspace-2026.json"));
    QCOMPARE(file.size(), 4);
    QCOMPARE(file[0].start, 0);
    QCOMPARE(file[0].end, 3);
    QCOMPARE(file[1].start, 4);
    QCOMPARE(file[1].end, 13);
    QCOMPARE(file[2].start, 14);
    QCOMPARE(file[2].end, 18);
    QCOMPARE(file[3].start, 19);
    QCOMPARE(file[3].end, 23);
    const QVector<PhraseCaretToken> phrase = alphanumericCaretTokens(QStringLiteral("Hello, world!"));
    QCOMPARE(phrase.size(), 2);
    QCOMPARE(phrase[0].end, 5);
    QCOMPARE(phrase[1].start, 7);
    QCOMPARE(phrase[1].end, 12);
    QCOMPARE(alphanumericCaretTokens(QStringLiteral("---")).size(), 0);
}

void PhraseCaretChooseTest::lookShortOfDwellDoesNotFire()
{
    const PhraseLookStep step = stepPhraseLook({}, 0, kPhraseWordDwellMs - 1.0, kPhraseWordDwellMs);
    QVERIFY(!step.fired);
    QVERIFY(!step.look.latched);
    QCOMPARE(step.look.id, 0);
    QCOMPARE(step.look.elapsedMs, kPhraseWordDwellMs - 1.0);
}

void PhraseCaretChooseTest::wordGrowsByHalf()
{
    QCOMPARE(phraseWordGrowScale(0.0, kPhraseWordDwellMs), 1.0);
    QCOMPARE(phraseWordGrowScale(kPhraseWordDwellMs * 0.5, kPhraseWordDwellMs), 1.25);
    QCOMPARE(phraseWordGrowScale(kPhraseWordDwellMs, kPhraseWordDwellMs), 1.5);
}

void PhraseCaretChooseTest::wordScanThenDwell()
{
    const PhraseWordStep scanning = stepPhraseWord({}, 0, kPhraseWordScanMs - 1.0, kPhraseWordScanMs,
                                                   kPhraseWordDwellMs);
    QVERIFY(scanning.look.scanning);
    QVERIFY(!scanning.fired);
    QCOMPARE(scanning.look.elapsedMs, kPhraseWordScanMs - 1.0);

    const PhraseWordStep armed =
        stepPhraseWord(scanning.look, 0, 1.0, kPhraseWordScanMs, kPhraseWordDwellMs);
    QVERIFY(!armed.look.scanning);
    QVERIFY(!armed.fired);
    QCOMPARE(armed.look.elapsedMs, 0.0);
    QCOMPARE(phraseWordGrowScale(armed.look.elapsedMs, kPhraseWordDwellMs), 1.0);

    const PhraseWordStep jump = stepPhraseWord({}, 0, kPhraseWordScanMs + kPhraseWordDwellMs,
                                               kPhraseWordScanMs, kPhraseWordDwellMs);
    QVERIFY(!jump.look.scanning);
    QVERIFY(!jump.fired);
    QCOMPARE(jump.look.elapsedMs, 0.0);

    const PhraseWordStep half = stepPhraseWord(armed.look, 0, kPhraseWordDwellMs * 0.5,
                                               kPhraseWordScanMs, kPhraseWordDwellMs);
    QVERIFY(!half.look.scanning);
    QVERIFY(!half.fired);
    QCOMPARE(half.look.elapsedMs, kPhraseWordDwellMs * 0.5);
    QCOMPARE(phraseWordGrowScale(half.look.elapsedMs, kPhraseWordDwellMs), 1.25);

    const PhraseWordStep done = stepPhraseWord(half.look, 0, kPhraseWordDwellMs * 0.5,
                                               kPhraseWordScanMs, kPhraseWordDwellMs);
    QVERIFY(done.fired);
    QVERIFY(done.look.latched);
    QCOMPARE(phraseWordGrowScale(done.look.elapsedMs, kPhraseWordDwellMs), 1.5);

    const PhraseWordStep held =
        stepPhraseWord(done.look, 0, kPhraseWordDwellMs, kPhraseWordScanMs, kPhraseWordDwellMs);
    QVERIFY(!held.fired);
    QVERIFY(held.look.latched);

    const PhraseWordStep left =
        stepPhraseWord(done.look, -1, 40.0, kPhraseWordScanMs, kPhraseWordDwellMs);
    QCOMPARE(left.look.id, -1);
    QVERIFY(left.look.scanning);

    const PhraseWordStep moved =
        stepPhraseWord(half.look, 2, 40.0, kPhraseWordScanMs, kPhraseWordDwellMs);
    QVERIFY(moved.look.scanning);
    QCOMPARE(moved.look.id, 2);
    QCOMPARE(moved.look.elapsedMs, 40.0);
    QVERIFY(!moved.fired);
}

void PhraseCaretChooseTest::shiftLayerSelects()
{
    QVERIFY(phraseShiftSelects(true, {}));
    QVERIFY(phraseShiftSelects(false, {2}));
    QVERIFY(phraseShiftSelects(false, {1, 4}));
    QVERIFY(!phraseShiftSelects(false, {1}));
    QVERIFY(!phraseShiftSelects(false, {3}));
    QVERIFY(!phraseShiftSelects(false, {5}));
}

void PhraseCaretChooseTest::arrowKeyRepeatsItsLastStep()
{
    const QVector<int> seq{400, 100};
    PhraseKeyLook look;
    const PhraseKeyStep scan = stepPhraseKey(look, 2, 100.0, seq, 100);
    QVERIFY(scan.look.scanDone);
    QVERIFY(!scan.fired);
    QCOMPARE(scan.look.step, 0);

    const PhraseKeyStep first = stepPhraseKey(scan.look, 2, 400.0, seq, 100);
    QVERIFY(first.fired);
    QCOMPARE(first.look.step, 1);

    const PhraseKeyStep again = stepPhraseKey(first.look, 2, 100.0, seq, 100);
    QVERIFY(again.fired);
    QCOMPARE(again.look.step, 1);
    const PhraseKeyStep held = stepPhraseKey(again.look, 2, 100.0, seq, 100);
    QVERIFY(held.fired);
    QCOMPARE(held.look.step, 1);

    const PhraseKeyStep left = stepPhraseKey(held.look, -1, 100.0, seq, 100);
    QCOMPARE(left.look.id, -1);
    QVERIFY(!left.fired);
    const PhraseKeyStep other = stepPhraseKey(held.look, 0, 100.0, seq, 100);
    QVERIFY(other.look.scanDone);
    QCOMPARE(other.look.step, 0);
    QVERIFY(!other.fired);
}

void PhraseCaretChooseTest::lookAtDwellFiresAndLatches()
{
    PhraseLook look;
    look.id = 1;
    look.elapsedMs = kPhraseWordDwellMs - 80.0;
    const PhraseLookStep step = stepPhraseLook(look, 1, 80.0, kPhraseWordDwellMs);
    QVERIFY(step.fired);
    QVERIFY(step.look.latched);
    QCOMPARE(step.look.id, 1);
    QCOMPARE(step.look.elapsedMs, kPhraseWordDwellMs);
}

void PhraseCaretChooseTest::latchedLookDoesNotRefire()
{
    PhraseLook look;
    look.id = 2;
    look.elapsedMs = kPhraseWordDwellMs;
    look.latched = true;
    const PhraseLookStep step = stepPhraseLook(look, 2, kPhraseWordDwellMs, kPhraseWordDwellMs);
    QVERIFY(!step.fired);
    QVERIFY(step.look.latched);
    QCOMPARE(step.look.elapsedMs, kPhraseWordDwellMs);
}

void PhraseCaretChooseTest::leavingTheTargetResetsTheLook()
{
    PhraseLook look;
    look.id = 0;
    look.elapsedMs = kPhraseWordDwellMs;
    look.latched = true;
    const PhraseLookStep gone = stepPhraseLook(look, -1, 40.0, kPhraseWordDwellMs);
    QVERIFY(!gone.fired);
    QVERIFY(!gone.look.latched);
    QCOMPARE(gone.look.id, -1);
    QCOMPARE(gone.look.elapsedMs, 0.0);

    const PhraseLookStep again = stepPhraseLook(gone.look, 0, kPhraseWordDwellMs, kPhraseWordDwellMs);
    QVERIFY(again.fired);
    QVERIFY(again.look.latched);
}

void PhraseCaretChooseTest::aNewTargetStartsOver()
{
    PhraseLook look;
    look.id = 0;
    look.elapsedMs = 500.0;
    const PhraseLookStep step = stepPhraseLook(look, 3, 40.0, kPhraseWordDwellMs);
    QVERIFY(!step.fired);
    QVERIFY(!step.look.latched);
    QCOMPARE(step.look.id, 3);
    QCOMPARE(step.look.elapsedMs, 40.0);
}

void PhraseCaretChooseTest::gapBetweenWordsMisses()
{
    const QVector<PhraseWordSpan> words = phraseWordSpans({oneLine()}, helloWorld());
    QCOMPARE(phraseWordIndexAt(words, QPointF(65, 110)), -1);
}

void PhraseCaretChooseTest::leftMovesOneCharacterAndClearsSelection()
{
    const PhraseCaretEdit edit = applyPhraseCaretChoice(6, 6, 11, PhraseCaretChoice::Left, false, 11);
    QCOMPARE(edit.caret, 5);
    QCOMPARE(edit.selStart, -1);
    QCOMPARE(edit.selEnd, -1);
    QVERIFY(edit.changed);
    QVERIFY(!edit.keepChooser);
}

void PhraseCaretChooseTest::rightMovesOneCharacterAndClearsSelection()
{
    const PhraseCaretEdit edit = applyPhraseCaretChoice(0, 0, 5, PhraseCaretChoice::Right, false, 11);
    QCOMPARE(edit.caret, 1);
    QCOMPARE(edit.selStart, -1);
    QCOMPARE(edit.selEnd, -1);
    QVERIFY(edit.changed);
    QVERIFY(!edit.keepChooser);
}

void PhraseCaretChooseTest::arrowsStepOneCharacter()
{
    const PhraseCaretEdit first =
        applyPhraseCaretChoice(0, 0, 5, PhraseCaretChoice::Right, false, 11);
    QCOMPARE(first.caret, 1);
    QCOMPARE(first.selStart, -1);

    const PhraseCaretEdit next = applyPhraseCaretChoice(first.caret, first.selStart, first.selEnd,
                                                        PhraseCaretChoice::Right, false, 11);
    QCOMPARE(next.caret, 2);
    QCOMPARE(next.selStart, -1);

    const PhraseCaretEdit back =
        applyPhraseCaretChoice(next.caret, -1, -1, PhraseCaretChoice::Left, false, 11);
    QCOMPARE(back.caret, 1);
}

void PhraseCaretChooseTest::shiftArrowExtendsOneCharacter()
{
    const PhraseCaretEdit right = applyPhraseCaretChoice(5, -1, -1, PhraseCaretChoice::Right, true, 11);
    QCOMPARE(right.caret, 6);
    QCOMPARE(right.selStart, 5);
    QCOMPARE(right.selEnd, 6);
    QVERIFY(right.keepChooser);
    QVERIFY(right.changed);

    const PhraseCaretEdit left = applyPhraseCaretChoice(5, -1, -1, PhraseCaretChoice::Left, true, 11);
    QCOMPARE(left.caret, 4);
    QCOMPARE(left.selStart, 4);
    QCOMPARE(left.selEnd, 5);
    QVERIFY(left.keepChooser);
    QVERIFY(left.changed);
}

void PhraseCaretChooseTest::shiftArrowShrinksFromTheCaret()
{
    const PhraseCaretEdit left = applyPhraseCaretChoice(11, 6, 11, PhraseCaretChoice::Left, true, 11);
    QCOMPARE(left.caret, 10);
    QCOMPARE(left.selStart, 6);
    QCOMPARE(left.selEnd, 10);
    QVERIFY(left.changed);

    const PhraseCaretEdit right = applyPhraseCaretChoice(6, 6, 11, PhraseCaretChoice::Right, true, 11);
    QCOMPARE(right.caret, 7);
    QCOMPARE(right.selStart, 7);
    QCOMPARE(right.selEnd, 11);
    QVERIFY(right.changed);

    const PhraseCaretEdit cleared = applyPhraseCaretChoice(6, 5, 6, PhraseCaretChoice::Left, true, 11);
    QCOMPARE(cleared.caret, 5);
    QCOMPARE(cleared.selStart, -1);
    QVERIFY(cleared.changed);
}

void PhraseCaretChooseTest::arrowsStayInsideThePhrase()
{
    const PhraseCaretEdit right = applyPhraseCaretChoice(11, -1, -1, PhraseCaretChoice::Right, true, 11);
    QCOMPARE(right.caret, 11);
    QCOMPARE(right.selStart, -1);
    QVERIFY(!right.changed);

    const PhraseCaretEdit left = applyPhraseCaretChoice(0, -1, -1, PhraseCaretChoice::Left, false, 11);
    QCOMPARE(left.caret, 0);
    QCOMPARE(left.selStart, -1);
    QVERIFY(!left.changed);
}

void PhraseCaretChooseTest::homeWithoutShiftMovesToTheStart()
{
    const PhraseCaretEdit edit = applyPhraseCaretChoice(5, 6, 11, PhraseCaretChoice::Home, false, 11);
    QCOMPARE(edit.caret, 0);
    QCOMPARE(edit.selStart, -1);
    QCOMPARE(edit.selEnd, -1);
    QVERIFY(edit.changed);
}

void PhraseCaretChooseTest::endWithoutShiftMovesToTheEnd()
{
    const PhraseCaretEdit edit = applyPhraseCaretChoice(5, 6, 11, PhraseCaretChoice::End, false, 11);
    QCOMPARE(edit.caret, 11);
    QCOMPARE(edit.selStart, -1);
    QCOMPARE(edit.selEnd, -1);
    QVERIFY(edit.changed);
}

void PhraseCaretChooseTest::homeAlreadyAtTheStartDoesNotChange()
{
    const PhraseCaretEdit edit = applyPhraseCaretChoice(0, -1, -1, PhraseCaretChoice::Home, false, 11);
    QCOMPARE(edit.caret, 0);
    QCOMPARE(edit.selStart, -1);
    QVERIFY(!edit.changed);
}

void PhraseCaretChooseTest::shiftHomeExtendsToTheStart()
{
    const PhraseCaretEdit edit = applyPhraseCaretChoice(5, -1, -1, PhraseCaretChoice::Home, true, 11);
    QCOMPARE(edit.caret, 0);
    QCOMPARE(edit.selStart, 0);
    QCOMPARE(edit.selEnd, 5);
    QVERIFY(edit.keepChooser);
    QVERIFY(edit.changed);
}

void PhraseCaretChooseTest::shiftEndExtendsToTheEnd()
{
    const PhraseCaretEdit edit = applyPhraseCaretChoice(5, -1, -1, PhraseCaretChoice::End, true, 11);
    QCOMPARE(edit.caret, 11);
    QCOMPARE(edit.selStart, 5);
    QCOMPARE(edit.selEnd, 11);
    QVERIFY(edit.keepChooser);
    QVERIFY(edit.changed);
}

void PhraseCaretChooseTest::shiftHomeKeepsTheFarEdge()
{
    const PhraseCaretEdit edit = applyPhraseCaretChoice(6, 6, 11, PhraseCaretChoice::Home, true, 11);
    QCOMPARE(edit.caret, 0);
    QCOMPARE(edit.selStart, 0);
    QCOMPARE(edit.selEnd, 11);
    QVERIFY(edit.changed);
}

void PhraseCaretChooseTest::shiftEndKeepsTheNearEdge()
{
    const PhraseCaretEdit edit = applyPhraseCaretChoice(6, 6, 11, PhraseCaretChoice::End, true, 11);
    QCOMPARE(edit.caret, 11);
    QCOMPARE(edit.selStart, 6);
    QCOMPARE(edit.selEnd, 11);
    QVERIFY(edit.changed);
}

void PhraseCaretChooseTest::boldWordDwellIsThreeLineHeights()
{
    PhraseWordSpan above;
    above.rects = {QRectF(10, 100, 40, 20)};
    PhraseWordSpan below;
    below.rects = {QRectF(10, 130, 40, 20)};
    const QVector<PhraseWordSpan> words{above, below};

    QCOMPARE(phraseWordIndexAt(words, QPointF(30, 85)), -1);
    QCOMPARE(phraseWordIndexAt(words, QPointF(30, 85), 0), 0);
    QCOMPARE(phraseWordIndexAt(words, QPointF(30, 70), 0), -1);
    QCOMPARE(phraseWordIndexAt(words, QPointF(30, 125), 0), 0);
    QCOMPARE(phraseWordIndexAt(words, QPointF(30, 135), 0), 1);
    QCOMPARE(phraseWordDwellRect(above.rects.at(0), true).height(), 60.0);
}

void PhraseCaretChooseTest::wrappedWordDoesNotFillTheGap()
{
    PhraseCaretLine top;
    top.start = 0;
    top.end = 6;
    top.rect = QRectF(0, 0, 100, 20);
    top.cursorX = {0, 10, 20, 30, 40, 50, 60};
    PhraseCaretLine bottom;
    bottom.start = 6;
    bottom.end = 11;
    bottom.rect = QRectF(0, 24, 100, 20);
    bottom.cursorX = {0, 10, 20, 30, 40, 50};
    const QVector<PhraseWordSpan> words =
        phraseWordSpans({top, bottom}, {PhraseCaretToken{0, 11}});
    QCOMPARE(words.size(), 1);
    QCOMPARE(words.at(0).rects.size(), 2);
    QCOMPARE(phraseWordIndexAt(words, QPointF(20, 10)), 0);
    QCOMPARE(phraseWordIndexAt(words, QPointF(15, 34)), 0);
    QCOMPARE(phraseWordIndexAt(words, QPointF(80, 10)), -1);
    QCOMPARE(phraseWordIndexAt(words, QPointF(30, 22)), -1);
}

void PhraseCaretChooseTest::wordPastTheLineEdgeIsNotAHit()
{
    PhraseCaretLine line;
    line.start = 0;
    line.end = 10;
    line.rect = QRectF(0, 0, 40, 20);
    line.cursorX.resize(11);
    for (int i = 0; i < line.cursorX.size(); ++i) {
        line.cursorX[i] = i * 10.0;
    }
    const QVector<PhraseWordSpan> visible =
        phraseWordSpans({line}, {PhraseCaretToken{0, 10}});
    QCOMPARE(visible.size(), 1);
    QCOMPARE(phraseWordIndexAt(visible, QPointF(15, 10)), 0);
    QCOMPARE(phraseWordIndexAt(visible, QPointF(70, 10)), -1);
    QCOMPARE(phraseWordSpans({line}, {PhraseCaretToken{5, 10}}).size(), 0);
}

void PhraseCaretChooseTest::caretOnAWrapStaysOnTheEarlierLine()
{
    PhraseCaretLine top;
    top.start = 0;
    top.end = 6;
    PhraseCaretLine bottom;
    bottom.start = 6;
    bottom.end = 11;
    const QVector<PhraseCaretLine> lines{top, bottom};
    QCOMPARE(phraseBoundaryCaretLine(lines, 6), 0);
    QCOMPARE(phraseBoundaryCaretLine(lines, 5), -1);
    QCOMPARE(phraseBoundaryCaretLine(lines, 11), -1);
}

void PhraseCaretChooseTest::shiftWordExtendsAcrossTheGap()
{
    const PhraseCaretEdit right = selectPhraseWordRange(5, 0, 5, 6, 11, true);
    QCOMPARE(right.selStart, 0);
    QCOMPARE(right.selEnd, 11);
    QCOMPARE(right.caret, 11);
    QVERIFY(right.changed);

    const PhraseCaretEdit left = selectPhraseWordRange(11, 6, 11, 0, 5, true);
    QCOMPARE(left.selStart, 0);
    QCOMPARE(left.selEnd, 11);
    QCOMPARE(left.caret, 0);
    QVERIFY(left.changed);

    const PhraseCaretEdit replaced = selectPhraseWordRange(5, 0, 5, 6, 11, false);
    QCOMPARE(replaced.selStart, 6);
    QCOMPARE(replaced.selEnd, 11);
    QCOMPARE(replaced.caret, 11);
}

void PhraseCaretChooseTest::wordInsideTheSelectionStaysPut()
{
    QVERIFY(phraseWordInsideSelection(6, 11, 0, 11));
    QVERIFY(!phraseWordInsideSelection(6, 11, 0, 8));
    const PhraseCaretEdit edit = selectPhraseWordRange(11, 0, 11, 6, 11, true);
    QCOMPARE(edit.caret, 11);
    QCOMPARE(edit.selStart, 0);
    QCOMPARE(edit.selEnd, 11);
    QVERIFY(!edit.changed);
}

void PhraseCaretChooseTest::copyBottomSitsOnThePhraseTop()
{
    PhraseCaretButton button;
    const QRectF anchor(100, 200, 40, 20);
    layoutPhraseCopyButton(anchor, QRectF(0, 0, 800, 600), button);
    QCOMPARE(button.center, QPointF(120, 172));
    QCOMPARE(button.center.y() + kPhraseCaretButtonRadius, anchor.top());
    QVERIFY(phraseCaretButtonAt(button, button.center));
}

void PhraseCaretChooseTest::copyClampsIntoTheScreen()
{
    PhraseCaretButton button;
    layoutPhraseCopyButton(QRectF(100, 4, 40, 20), QRectF(0, 0, 800, 600), button);
    QCOMPARE(button.center, QPointF(120, 36));
}

void PhraseCaretChooseTest::pointBelowThePhraseMissesCopy()
{
    PhraseCaretButton button;
    const QRectF word(100, 200, 40, 20);
    layoutPhraseCopyButton(word, QRectF(0, 0, 800, 600), button);
    QVERIFY(phraseCaretButtonAt(button, QPointF(word.center().x(), word.top())));
    QVERIFY(!phraseCaretButtonAt(button, QPointF(120, 260)));
}

void PhraseCaretChooseTest::captionUsesLabelRect()
{
    const QRectF cell(0, 0, 200, 80);
    const PhraseTextFrame frame = phraseTextFrame(QStringLiteral("Segoe UI"), cell,
                                                   QStringLiteral("Hello"), QStringLiteral("note"),
                                                   QStringLiteral("body"), QStringLiteral("display"),
                                                   true);
    QVERIFY(frame.valid);
    const QRectF pad = phrasePad(cell);
    QCOMPARE(frame.textRect.top(), pad.top());
    QCOMPARE(frame.textRect.left(), pad.left());
    QVERIFY(frame.textRect.bottom() < pad.bottom() - 4.0);

    const PhraseTextFrame bare =
        phraseTextFrame(QStringLiteral("Segoe UI"), cell, QStringLiteral("Hello"), QString(),
                        QStringLiteral("body"), QStringLiteral("display"), true);
    QCOMPARE(bare.textRect, pad);

    const QVector<PhraseLineGeom> lines =
        layoutPhraseLines(frame.textRect, QStringLiteral("Hello"), frame.font, frame.flags);
    QVERIFY(!lines.isEmpty());
    QVERIFY(frame.textRect.contains(lines[0].rect.center()));
    QVERIFY(lines[0].rect.bottom() <= frame.textRect.bottom() + 0.5);
}

void PhraseCaretChooseTest::phraseFontStaysBodySize()
{
    const QRectF cell(0, 0, 80, 40);
    const QString text(80, QLatin1Char('a'));
    const PhraseTextFrame frame = phraseTextFrame(QStringLiteral("Segoe UI"), cell, text, QString(),
                                                   QStringLiteral("body"), QStringLiteral("display"),
                                                   true, false);
    QVERIFY(frame.valid);
    QCOMPARE(frame.font.pointSize(), 18);
}

void PhraseCaretChooseTest::wrappedBlockIsTaller()
{
    const QFont font(QStringLiteral("Segoe UI"), 18);
    const int flags = phraseStyle(QStringLiteral("body"), QStringLiteral("display")).flags;
    const QString text(40, QLatin1Char('W'));
    const double one = phraseBlockHeight(QStringLiteral("W"), font, 400, flags);
    const double wrapped = phraseBlockHeight(text, font, 40, flags);
    QVERIFY(wrapped > one + 1.0);
}

void PhraseCaretChooseTest::rowGrowsPastTheBase()
{
    QCOMPARE(phraseRowPx(96, 40, 24), 96.0);
    QCOMPARE(phraseRowPx(96, 80, 24), 104.0);
}

QObject* createPhraseCaretChooseTest()
{
    return new PhraseCaretChooseTest;
}

#include "PhraseCaretGazeTest.moc"
