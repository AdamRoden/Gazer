#include "predict/CharPrior.h"
#include "predict/TypingContext.h"
#include "predict/WordPredictor.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using gazer::CharPrior;
using gazer::TypingContext;
using gazer::WordPredictor;

namespace {

double massOf(const QVector<WordPredictor::NextChar>& rows, QChar symbol)
{
    double sum = 0;
    for (const WordPredictor::NextChar& row : rows) {
        if (row.symbol == symbol) {
            sum += row.mass;
        }
    }
    return sum;
}

bool near(double got, double want)
{
    return qAbs(got - want) < 1e-9;
}

WordPredictor::WordSpec word(const char* text, double uni)
{
    WordPredictor::WordSpec spec;
    spec.text = QString::fromLatin1(text);
    spec.unigram = uni;
    return spec;
}

} // namespace

class CharPriorTest final : public QObject {
    Q_OBJECT

private slots:
    void contextTPrefersH();
    void punctuationStaysInSourceOrder();
    void finishedWordPrefersSpace();
    void orderByteKeepsContextsApart();
    void personalBlendHalf();
    void personalBlendOneObservation();
    void undoRemovesObservation();
    void backoffSkipsASeenContext();
    void blobRoundTrip();
    void dictionaryExactWordPrefersSpace();
    void userWordPrefersItsLetter();
    void mixesPriorAndDictionary();
    void pagesShareOnePrior();
    void phraseThenKeyResetsContext();
    void adoptKeepsKeyCounts();
    void backspaceUndoesCount();
    void keyStreamObservesClosedWords();
    void contractionAndDigitsStayOneWord();
    void restorePhraseDoesNotLearnTheSuffix();
    void composePageIsNotASecondLearner();
    void arrowClearsWithoutLearning();
    void idleKeepsTheString();
    void shippedPriorPrefersHAfterT();
};

void CharPriorTest::contextTPrefersH()
{
    CharPrior prior;
    QString err;
    QVERIFY2(prior.build({QStringLiteral("the cat"), QStringLiteral("the dog"),
                          QStringLiteral("that thing")},
                         {}, &err),
             qPrintable(err));
    const double h = prior.probability(QChar(), QLatin1Char('t'), QLatin1Char('h'));
    QVERIFY(h > prior.probability(QChar(), QLatin1Char('t'), QLatin1Char('g')));
    QVERIFY(h > prior.probability(QChar(), QLatin1Char('t'), QLatin1Char('j')));
    QVERIFY(h > prior.probability(QChar(), QLatin1Char('t'), QLatin1Char('q')));
    QVERIFY(near(h, 1.0));
}

void CharPriorTest::punctuationStaysInSourceOrder()
{
    CharPrior prior;
    QString err;
    QVERIFY2(prior.build({QStringLiteral("hello, world")}, {}, &err), qPrintable(err));
    const double comma = prior.probability(QLatin1Char('l'), QLatin1Char('o'), QLatin1Char(','));
    QVERIFY(comma > prior.probability(QLatin1Char('l'), QLatin1Char('o'), QLatin1Char(' ')));
    QVERIFY(near(comma, 1.0));
    QVERIFY(near(prior.probability(QLatin1Char('o'), QLatin1Char(','), QLatin1Char(' ')), 1.0));
    QVERIFY(near(prior.probability(QLatin1Char(','), QLatin1Char(' '), QLatin1Char('w')), 1.0));
}

void CharPriorTest::finishedWordPrefersSpace()
{
    CharPrior prior;
    QString err;
    QVERIFY2(prior.build({QStringLiteral("the cat"), QStringLiteral("the dog"),
                          QStringLiteral("that thing")},
                         {}, &err),
             qPrintable(err));
    const double space = prior.probability(QLatin1Char('a'), QLatin1Char('t'), QLatin1Char(' '));
    QVERIFY(space > prior.probability(QLatin1Char('a'), QLatin1Char('t'), QLatin1Char('h')));
    QVERIFY(space > prior.probability(QLatin1Char('a'), QLatin1Char('t'), QLatin1Char('v')));
    QVERIFY(space > prior.probability(QLatin1Char('a'), QLatin1Char('t'), QLatin1Char('g')));
    QVERIFY(space > prior.probability(QLatin1Char('a'), QLatin1Char('t'), QLatin1Char('b')));
    QVERIFY(near(space, 1.0));
}

void CharPriorTest::orderByteKeepsContextsApart()
{
    CharPrior prior;
    QString err;
    QVERIFY2(prior.build({QStringLiteral("ta"), QStringLiteral("at")}, {}, &err), qPrintable(err));
    QVERIFY(near(prior.probability(QChar(), QLatin1Char('t'), QLatin1Char('a')), 1.0));
    QCOMPARE(prior.probability(QChar(), QLatin1Char('t'), QLatin1Char(' ')), 0.0);
}

void CharPriorTest::personalBlendHalf()
{
    CharPrior prior;
    QString err;
    QVERIFY2(prior.build({QStringLiteral("th")}, {}, &err), qPrintable(err));
    for (int i = 0; i < 40; ++i) {
        prior.observe(QChar(), QLatin1Char('t'), QLatin1Char('z'));
    }
    const double h = prior.probability(QChar(), QLatin1Char('t'), QLatin1Char('h'));
    const double z = prior.probability(QChar(), QLatin1Char('t'), QLatin1Char('z'));
    QVERIFY(h > 0.2);
    QVERIFY(z > 0.2);
    QVERIFY(near(h, 0.5));
    QVERIFY(near(z, 0.5));
}

void CharPriorTest::personalBlendOneObservation()
{
    CharPrior prior;
    QString err;
    QVERIFY2(prior.build({QStringLiteral("th")}, {}, &err), qPrintable(err));
    prior.observe(QChar(), QLatin1Char('t'), QLatin1Char('z'));
    QVERIFY(near(prior.probability(QChar(), QLatin1Char('t'), QLatin1Char('z')), 1.0 / 41.0));
    QVERIFY(near(prior.probability(QChar(), QLatin1Char('t'), QLatin1Char('h')), 40.0 / 41.0));
}

void CharPriorTest::undoRemovesObservation()
{
    CharPrior prior;
    QString err;
    QVERIFY2(prior.build({QStringLiteral("th")}, {}, &err), qPrintable(err));
    prior.observe(QChar(), QLatin1Char('t'), QLatin1Char('z'));
    QVERIFY(prior.undo(QChar(), QLatin1Char('t'), QLatin1Char('z')));
    QCOMPARE(prior.probability(QChar(), QLatin1Char('t'), QLatin1Char('z')), 0.0);
    QVERIFY(near(prior.probability(QChar(), QLatin1Char('t'), QLatin1Char('h')), 1.0));
    QVERIFY(!prior.undo(QChar(), QLatin1Char('t'), QLatin1Char('z')));
}

void CharPriorTest::backoffSkipsASeenContext()
{
    CharPrior prior;
    QString err;
    QVERIFY2(prior.build({QStringLiteral("the cat")}, {}, &err), qPrintable(err));
    QVERIFY(near(prior.probability(QLatin1Char('q'), QLatin1Char('e'), QLatin1Char(' ')), 0.4));
    QCOMPARE(prior.probability(QLatin1Char('q'), QLatin1Char('e'), QLatin1Char('h')), 0.0);
    QVERIFY(near(prior.probability(QLatin1Char('q'), QLatin1Char('q'), QLatin1Char('t')), 0.04));
    QCOMPARE(prior.probability(QLatin1Char('q'), QLatin1Char('q'), QLatin1Char('q')), 0.0);
}

void CharPriorTest::blobRoundTrip()
{
    CharPrior prior;
    QString err;
    QVERIFY2(prior.build({QStringLiteral("the cat"), QStringLiteral("the dog"),
                          QStringLiteral("that thing")},
                         {}, &err),
             qPrintable(err));
    prior.observe(QChar(), QLatin1Char('t'), QLatin1Char('z'));
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString ship = dir.filePath(QStringLiteral("charprior.bin"));
    const QString user = dir.filePath(QStringLiteral("char-prior-user.bin"));
    QVERIFY2(prior.saveFile(ship, &err), qPrintable(err));
    QVERIFY2(prior.saveUser(user, &err), qPrintable(err));
    CharPrior loaded;
    QVERIFY2(loaded.loadFile(ship, &err), qPrintable(err));
    QVERIFY(loaded.loadUser(user));
    QVERIFY(near(loaded.probability(QChar(), QLatin1Char('t'), QLatin1Char('h')),
                 prior.probability(QChar(), QLatin1Char('t'), QLatin1Char('h'))));
    QVERIFY(near(loaded.probability(QLatin1Char('a'), QLatin1Char('t'), QLatin1Char(' ')),
                 prior.probability(QLatin1Char('a'), QLatin1Char('t'), QLatin1Char(' '))));
    QVERIFY(near(loaded.probability(QChar(), QLatin1Char('t'), QLatin1Char('z')),
                 prior.probability(QChar(), QLatin1Char('t'), QLatin1Char('z'))));
}

void CharPriorTest::dictionaryExactWordPrefersSpace()
{
    WordPredictor::Fixture fixture;
    fixture.words = {word("cat", 0.5), word("catch", 0.01), word("cath", 0.01)};
    WordPredictor predictor;
    QString err;
    QVERIFY2(predictor.loadFixture(fixture, &err), qPrintable(err));
    WordPredictor::Query query;
    query.typed = QStringLiteral("cat");
    const QVector<WordPredictor::NextChar> rows = predictor.nextCharMass(query);
    QVERIFY(massOf(rows, QLatin1Char(' ')) > massOf(rows, QLatin1Char('h')));
    QVERIFY(massOf(rows, QLatin1Char(' ')) > massOf(rows, QLatin1Char('c')));
    QVERIFY(massOf(rows, QLatin1Char('h')) > massOf(rows, QLatin1Char('q')));
    QVERIFY(massOf(rows, QLatin1Char('c')) > 0);
}

void CharPriorTest::userWordPrefersItsLetter()
{
    WordPredictor learned;
    for (int i = 0; i < 20; ++i) {
        learned.observe({}, QStringLiteral("adam"));
    }
    WordPredictor::Query query;
    query.typed = QStringLiteral("ada");
    QVERIFY(near(massOf(learned.nextCharMass(query), QLatin1Char('m')), 1.0));

    WordPredictor::Fixture fixture;
    fixture.words = {word("adapt", 0.01)};
    WordPredictor mixed;
    QString err;
    QVERIFY2(mixed.loadFixture(fixture, &err), qPrintable(err));
    for (int i = 0; i < 20; ++i) {
        mixed.observe({}, QStringLiteral("adam"));
    }
    const QVector<WordPredictor::NextChar> rows = mixed.nextCharMass(query);
    QVERIFY(massOf(rows, QLatin1Char('m')) > massOf(rows, QLatin1Char('p')));
}

void CharPriorTest::mixesPriorAndDictionary()
{
    WordPredictor::Fixture fixture;
    fixture.words = {word("cat", 1.0)};
    WordPredictor predictor;
    QString err;
    QVERIFY2(predictor.loadFixture(fixture, &err), qPrintable(err));
    TypingContext typing(&predictor);
    QVERIFY2(typing.characters().build({QStringLiteral("th")}, {}, &err), qPrintable(err));
    const QHash<QChar, double> dist = typing.distribution();
    QVERIFY(near(dist.value(QLatin1Char('c')), 0.55));
    QVERIFY(near(dist.value(QLatin1Char('t')), 0.15));
}

void CharPriorTest::pagesShareOnePrior()
{
    WordPredictor predictor;
    TypingContext typing(&predictor);
    typing.noteKey(QStringLiteral("qwerty_main"), QLatin1Char('t'));
    typing.noteKey(QStringLiteral("uw_qwerty"), QLatin1Char('h'));
    QCOMPARE(typing.context(), QStringLiteral("th"));
    QVERIFY(typing.characters().probability(QChar(), QLatin1Char('t'), QLatin1Char('h')) > 0);
}

void CharPriorTest::phraseThenKeyResetsContext()
{
    WordPredictor predictor;
    TypingContext typing(&predictor);
    typing.syncPhrase(QStringLiteral("hello"), 5);
    QCOMPARE(typing.context(), QStringLiteral("hello"));
    QVERIFY(typing.characters().probability(QChar(), QLatin1Char('h'), QLatin1Char('e')) > 0);
    typing.noteKey(QStringLiteral("qwerty_main"), QLatin1Char('q'));
    QCOMPARE(typing.context(), QStringLiteral("q"));
}

void CharPriorTest::adoptKeepsKeyCounts()
{
    WordPredictor predictor;
    TypingContext typing(&predictor);
    typing.noteKey(QStringLiteral("qwerty_main"), QLatin1Char('t'));
    typing.noteKey(QStringLiteral("qwerty_main"), QLatin1Char('z'));
    const double z = typing.characters().probability(QChar(), QLatin1Char('t'), QLatin1Char('z'));
    QVERIFY(z > 0);
    typing.syncPhrase(QStringLiteral("hello"), 5);
    QCOMPARE(typing.context(), QStringLiteral("hello"));
    QVERIFY(near(typing.characters().probability(QChar(), QLatin1Char('t'), QLatin1Char('z')), z));
    QCOMPARE(typing.characters().probability(QChar(), QLatin1Char('h'), QLatin1Char('e')), 0.0);
}

void CharPriorTest::backspaceUndoesCount()
{
    WordPredictor predictor;
    TypingContext typing(&predictor);
    typing.noteKey(QStringLiteral("qwerty_main"), QLatin1Char('t'));
    typing.noteKey(QStringLiteral("qwerty_main"), QLatin1Char('z'));
    QVERIFY(typing.characters().probability(QChar(), QLatin1Char('t'), QLatin1Char('z')) > 0);
    typing.noteBackspace(QStringLiteral("qwerty_main"));
    QCOMPARE(typing.context(), QStringLiteral("t"));
    QCOMPARE(typing.characters().probability(QChar(), QLatin1Char('t'), QLatin1Char('z')), 0.0);
}

void CharPriorTest::keyStreamObservesClosedWords()
{
    WordPredictor predictor;
    TypingContext typing(&predictor);
    typing.noteSend(QStringLiteral("qwerty_main"), QStringLiteral("h"), {});
    typing.noteSend(QStringLiteral("example_keyboard"), QStringLiteral("i"), {});
    typing.noteCommand(QStringLiteral("uw_qwerty"), QStringLiteral("space"));
    QCOMPARE(typing.context(), QStringLiteral("hi "));
    WordPredictor::Query query;
    query.typed = QStringLiteral("h");
    QVERIFY(near(massOf(predictor.nextCharMass(query), QLatin1Char('i')), 1.0));

    WordPredictor shortWord;
    TypingContext one(&shortWord);
    one.noteKey(QStringLiteral("qwerty_main"), QLatin1Char('a'));
    one.noteCommand(QStringLiteral("qwerty_main"), QStringLiteral("space"));
    WordPredictor::Query letter;
    letter.typed = QStringLiteral("a");
    QCOMPARE(massOf(shortWord.nextCharMass(letter), QLatin1Char(' ')), 0.0);

    WordPredictor entered;
    TypingContext line(&entered);
    line.noteKey(QStringLiteral("qwerty_main"), QLatin1Char('g'));
    line.noteKey(QStringLiteral("qwerty_main"), QLatin1Char('o'));
    line.noteCommand(QStringLiteral("qwerty_main"), QStringLiteral("enter"));
    QCOMPARE(line.context(), QString());
    WordPredictor::Query go;
    go.typed = QStringLiteral("g");
    QVERIFY(near(massOf(entered.nextCharMass(go), QLatin1Char('o')), 1.0));
}

void CharPriorTest::contractionAndDigitsStayOneWord()
{
    WordPredictor predictor;
    TypingContext typing(&predictor);
    const QString contraction = QStringLiteral("don't");
    for (const QChar ch : contraction) {
        typing.noteKey(QStringLiteral("qwerty_main"), ch);
    }
    typing.noteCommand(QStringLiteral("qwerty_main"), QStringLiteral("space"));
    WordPredictor::Query prefix;
    prefix.typed = QStringLiteral("don");
    QVERIFY(massOf(predictor.nextCharMass(prefix), QLatin1Char('\'')) > 0.0);
    QCOMPARE(massOf(predictor.nextCharMass(prefix), QLatin1Char(' ')), 0.0);

    WordPredictor digits;
    TypingContext numeric(&digits);
    const QString token = QStringLiteral("cat2dog");
    for (const QChar ch : token) {
        numeric.noteKey(QStringLiteral("qwerty_main"), ch);
    }
    numeric.noteCommand(QStringLiteral("qwerty_main"), QStringLiteral("space"));
    WordPredictor::Query mid;
    mid.typed = QStringLiteral("cat2");
    QVERIFY(near(massOf(digits.nextCharMass(mid), QLatin1Char('d')), 1.0));
}

void CharPriorTest::restorePhraseDoesNotLearnTheSuffix()
{
    WordPredictor predictor;
    TypingContext typing(&predictor);
    typing.syncPhrase(QStringLiteral("hello world"), 5);
    QCOMPARE(typing.context(), QStringLiteral("hello"));
    QCOMPARE(typing.characters().probability(QLatin1Char('l'), QLatin1Char('o'), QLatin1Char(' ')),
             0.0);
    typing.adoptPhrase(QStringLiteral("hello world"), 11);
    QCOMPARE(typing.context(), QStringLiteral("hello world"));
    QCOMPARE(typing.characters().probability(QLatin1Char('l'), QLatin1Char('o'), QLatin1Char(' ')),
             0.0);
    typing.syncPhrase(QStringLiteral("hello world"), 11);
    QCOMPARE(typing.characters().probability(QLatin1Char('l'), QLatin1Char('o'), QLatin1Char(' ')),
             0.0);

    WordPredictor named;
    TypingContext nameEdit(&named);
    nameEdit.syncPhrase(QStringLiteral("hello"), 5);
    const double backedOff =
        nameEdit.characters().probability(QChar(), QLatin1Char('m'), QLatin1Char('o'));
    nameEdit.adoptPhrase(QStringLiteral("Mom"), 3);
    QCOMPARE(nameEdit.context(), QStringLiteral("mom"));
    const double after =
        nameEdit.characters().probability(QChar(), QLatin1Char('m'), QLatin1Char('o'));
    QVERIFY2(qAbs(after - backedOff) < 1e-12, "restoring an unrelated name must not count m -> o");
    QVERIFY(after < 0.5);
}

void CharPriorTest::composePageIsNotASecondLearner()
{
    WordPredictor predictor;
    TypingContext typing(&predictor);
    typing.noteKey(QStringLiteral("compose"), QLatin1Char('z'));
    typing.noteSend(QStringLiteral("compose"), QStringLiteral("a"), {});
    typing.noteCommand(QStringLiteral("compose_item_edit_live"), QStringLiteral("space"));
    typing.noteSend(QStringLiteral("qwerty_main"), QStringLiteral("q"), QStringLiteral("down"));
    QCOMPARE(typing.context(), QString());
}

void CharPriorTest::arrowClearsWithoutLearning()
{
    WordPredictor predictor;
    TypingContext typing(&predictor);
    typing.noteKey(QStringLiteral("qwerty_main"), QLatin1Char('g'));
    typing.noteKey(QStringLiteral("qwerty_main"), QLatin1Char('o'));
    typing.noteCommand(QStringLiteral("qwerty_main"), QStringLiteral("arrowleft"));
    QCOMPARE(typing.context(), QString());
    WordPredictor::Query query;
    query.typed = QStringLiteral("g");
    QCOMPARE(massOf(predictor.nextCharMass(query), QLatin1Char('o')), 0.0);
    typing.noteKey(QStringLiteral("qwerty_main"), QLatin1Char('t'));
    typing.noteCommand(QStringLiteral("qwerty_main"), QStringLiteral("settings.dwell.slow"));
    QCOMPARE(typing.context(), QStringLiteral("t"));
}

void CharPriorTest::idleKeepsTheString()
{
    WordPredictor predictor;
    TypingContext typing(&predictor);
    typing.noteKey(QStringLiteral("qwerty_main"), QLatin1Char('t'));
    typing.noteKey(QStringLiteral("qwerty_main"), QLatin1Char('h'));
    typing.forgetContext();
    QVERIFY(typing.contextStale());
    QCOMPARE(typing.context(), QStringLiteral("th"));
    const QHash<QChar, double> dist = typing.distribution();
    QVERIFY(near(dist.value(QLatin1Char('h')), 0.5));
    QVERIFY(near(dist.value(QLatin1Char('t')), 0.5));
}

void CharPriorTest::shippedPriorPrefersHAfterT()
{
#ifndef GAZER_SOURCE_DIR
    QSKIP("GAZER_SOURCE_DIR is not set");
#else
    const QString path = QDir(QString::fromUtf8(GAZER_SOURCE_DIR))
                             .filePath(QStringLiteral("resources/predict/charprior.bin"));
    if (!QFile::exists(path)) {
        QSKIP("charprior.bin is not built");
    }
    CharPrior prior;
    QString err;
    QVERIFY2(prior.loadFile(path, &err), qPrintable(err));
    const double h = prior.probability(QChar(), QLatin1Char('t'), QLatin1Char('h'));
    QVERIFY2(h > prior.probability(QChar(), QLatin1Char('t'), QLatin1Char('g')),
             qPrintable(QStringLiteral("h=%1 g=%2")
                            .arg(h)
                            .arg(prior.probability(QChar(), QLatin1Char('t'), QLatin1Char('g')))));
    QVERIFY(h > prior.probability(QChar(), QLatin1Char('t'), QLatin1Char('j')));
#endif
}

QObject* createCharPriorTest()
{
    return new CharPriorTest();
}

#include "CharPriorTest.moc"
