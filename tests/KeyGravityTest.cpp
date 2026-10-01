#include "layout/KeyGravity.h"

#include <QtTest>

using gazer::KeyGravityCell;
using gazer::KeyGravityQuery;
using gazer::PageAction;
using gazer::PageActionType;
using gazer::PageTarget;

namespace {

KeyGravityCell key(const char* id, double x, double width, QChar symbol, const char* grid = "board")
{
    KeyGravityCell cell;
    cell.id = QString::fromLatin1(id);
    cell.gridId = QString::fromLatin1(grid);
    cell.rect = QRectF(x, 0, width, 40);
    cell.symbol = symbol;
    return cell;
}

QVector<KeyGravityCell> letterRow()
{
    return {key("g", 0, 100, QLatin1Char('g')),
            key("h", 100, 100, QLatin1Char('h')),
            key("j", 200, 100, QLatin1Char('j')),
            key("backspace", 300, 100, QChar()),
            key("y", 1000, 100, QLatin1Char('y'))};
}

int pickAt(const QVector<KeyGravityCell>& cells, int geometric, const QPointF& gaze, int strength,
           const QHash<QChar, double>& probability, const QString& sticky = {})
{
    KeyGravityQuery query;
    query.cells = cells;
    query.geometricIndex = geometric;
    query.gaze = gaze;
    query.strength = strength;
    query.probability = &probability;
    query.stickyId = sticky;
    return gazer::keyGravityPick(query);
}

PageAction sendAction(const QString& key)
{
    PageAction action;
    action.type = PageActionType::Send;
    action.sendKey = key;
    return action;
}

PageAction commandAction(const QString& command)
{
    PageAction action;
    action.type = PageActionType::Command;
    action.command = command;
    return action;
}

} // namespace

class KeyGravityTest final : public QObject {
    Q_OBJECT

private slots:
    void coreStaysPut();
    void outerQuarterYieldsAtStrength55();
    void strengthZeroIsGeometric();
    void distantKeyStays();
    void backspaceIsProtected();
    void spaceStealsOuterQuarterOnly();
    void stickinessHoldsWeakWin();
    void equalProbabilityKeepsGeometry();
    void gapStaysMiss();
    void otherGridDoesNotSteal();
    void authoredActivationIsNotScaled();
    void rapidScaleFollowsSurprise();
    void characterSymbolLettersAndSpace();
};

void KeyGravityTest::coreStaysPut()
{
    QHash<QChar, double> probability;
    probability.insert(QLatin1Char('h'), 1000);
    probability.insert(QLatin1Char('g'), 1);
    QCOMPARE(pickAt(letterRow(), 0, QPointF(50, 20), 100, probability), 0);
}

void KeyGravityTest::outerQuarterYieldsAtStrength55()
{
    QHash<QChar, double> probability;
    probability.insert(QLatin1Char('h'), 1000);
    probability.insert(QLatin1Char('g'), 1);
    QCOMPARE(pickAt(letterRow(), 0, QPointF(90, 20), 55, probability), 1);
}

void KeyGravityTest::strengthZeroIsGeometric()
{
    QHash<QChar, double> probability;
    probability.insert(QLatin1Char('h'), 1000);
    probability.insert(QLatin1Char('g'), 1);
    QCOMPARE(pickAt(letterRow(), 0, QPointF(90, 20), 0, probability), 0);
}

void KeyGravityTest::distantKeyStays()
{
    QHash<QChar, double> probability;
    probability.insert(QLatin1Char('h'), 1);
    probability.insert(QLatin1Char('y'), 1e-6);
    QCOMPARE(pickAt(letterRow(), 4, QPointF(1010, 20), 100, probability), 4);
}

void KeyGravityTest::backspaceIsProtected()
{
    QHash<QChar, double> probability;
    probability.insert(QLatin1Char('h'), 1000);
    probability.insert(QLatin1Char('j'), 1000);
    QCOMPARE(pickAt(letterRow(), 3, QPointF(350, 20), 100, probability), 3);
    probability.insert(QLatin1Char('g'), 1);
    QCOMPARE(pickAt(letterRow(), 0, QPointF(90, 20), 100, probability), 1);
}

void KeyGravityTest::spaceStealsOuterQuarterOnly()
{
    const QVector<KeyGravityCell> row = {key("v", 0, 100, QLatin1Char('v')),
                                         key("space", 100, 180, QLatin1Char(' ')),
                                         key("b", 280, 100, QLatin1Char('b'))};
    QHash<QChar, double> probability;
    probability.insert(QLatin1Char(' '), 1000);
    probability.insert(QLatin1Char('v'), 1);
    probability.insert(QLatin1Char('b'), 1);
    QCOMPARE(pickAt(row, 0, QPointF(90, 20), 100, probability), 1);
    QCOMPARE(pickAt(row, 0, QPointF(70, 20), 100, probability), 0);
    QCOMPARE(pickAt(row, 0, QPointF(76, 20), 100, probability), 0);
    QCOMPARE(pickAt(row, 2, QPointF(290, 20), 100, probability), 1);
    QCOMPARE(pickAt(row, 2, QPointF(310, 20), 100, probability), 2);
}

void KeyGravityTest::stickinessHoldsWeakWin()
{
    QHash<QChar, double> probability;
    probability.insert(QLatin1Char('h'), 1.1);
    probability.insert(QLatin1Char('g'), 1.0);
    QCOMPARE(pickAt(letterRow(), 0, QPointF(99, 20), 55, probability), 1);
    QCOMPARE(pickAt(letterRow(), 0, QPointF(99, 20), 55, probability, QStringLiteral("g")), 0);
}

void KeyGravityTest::equalProbabilityKeepsGeometry()
{
    QHash<QChar, double> probability;
    probability.insert(QLatin1Char('h'), 0.5);
    probability.insert(QLatin1Char('g'), 0.5);
    QCOMPARE(pickAt(letterRow(), 0, QPointF(90, 20), 100, probability), 0);
}

void KeyGravityTest::gapStaysMiss()
{
    QHash<QChar, double> probability;
    probability.insert(QLatin1Char('h'), 1);
    QCOMPARE(pickAt(letterRow(), -1, QPointF(90, 20), 100, probability), -1);
}

void KeyGravityTest::otherGridDoesNotSteal()
{
    QVector<KeyGravityCell> cells = letterRow();
    cells.push_back(key("z", 100, 100, QLatin1Char('z'), "other"));
    QHash<QChar, double> probability;
    probability.insert(QLatin1Char('z'), 1);
    probability.insert(QLatin1Char('g'), 1e-4);
    QCOMPARE(pickAt(cells, 0, QPointF(90, 20), 100, probability), 0);
}

void KeyGravityTest::authoredActivationIsNotScaled()
{
    const QVector<int> authored{10, 10};
    const QVector<int> settings{400, 600};
    QCOMPARE(gazer::chooseRapidSteps(settings, &authored, 100, 1.0), authored);
    QCOMPARE(gazer::chooseRapidSteps(settings, nullptr, 0, 1.0), settings);
}

void KeyGravityTest::rapidScaleFollowsSurprise()
{
    QVERIFY(qAbs(gazer::rapidDwellScale(0, 1.0) - 1.0) < 1e-12);
    QVERIFY(qAbs(gazer::rapidDwellScale(100, 0.0) - 1.0) < 1e-12);
    QVERIFY(qAbs(gazer::rapidDwellScale(100, 1.0) - 0.75) < 1e-12);
    QVERIFY(qAbs(gazer::rapidDwellScale(100, -1.0) - 1.20) < 1e-12);
    const QVector<int> settings{400, 0, 1};
    QCOMPARE(gazer::chooseRapidSteps(settings, nullptr, 100, 1.0), (QVector<int>{300, 0, 1}));
    QCOMPARE(gazer::chooseRapidSteps(settings, nullptr, 100, -1.0), (QVector<int>{480, 0, 1}));
}

void KeyGravityTest::characterSymbolLettersAndSpace()
{
    PageTarget letter;
    letter.actions = {sendAction(QStringLiteral("H"))};
    QCOMPARE(gazer::characterSymbol(letter), QLatin1Char('h'));

    PageTarget space;
    space.actions = {commandAction(QStringLiteral("space"))};
    QCOMPARE(gazer::characterSymbol(space), QLatin1Char(' '));

    PageTarget named;
    named.actions = {sendAction(QStringLiteral("Space"))};
    QCOMPARE(gazer::characterSymbol(named), QLatin1Char(' '));

    PageTarget backspace;
    backspace.actions = {commandAction(QStringLiteral("backspace"))};
    QVERIFY(gazer::characterSymbol(backspace).isNull());

    PageTarget shift;
    shift.actions = {sendAction(QStringLiteral("shift"))};
    QVERIFY(gazer::characterSymbol(shift).isNull());

    PageTarget dot;
    dot.actions = {sendAction(QStringLiteral("."))};
    QCOMPARE(gazer::characterSymbol(dot), QLatin1Char('.'));
}

QObject* createKeyGravityTest()
{
    return new KeyGravityTest();
}

#include "KeyGravityTest.moc"
