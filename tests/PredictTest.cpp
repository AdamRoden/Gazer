#include "predict/WordPredictor.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QtTest>

using gazer::WordPredictor;

namespace {

WordPredictor::Fixture fixture()
{
    WordPredictor::Fixture f;
    const auto add = [&](const char* text, double uni, QVector<qint8> emb = {}) {
        WordPredictor::WordSpec w;
        w.text = QString::fromLatin1(text);
        w.unigram = uni;
        w.embedding = std::move(emb);
        f.words.push_back(w);
    };
    const QVector<qint8> hospital{100, 0, 0, 0};
    const QVector<qint8> doctor{100, 0, 0, 0};
    const QVector<qint8> hill{0, 100, 0, 0};
    const QVector<qint8> pizza{0, 0, 100, 0};
    add("i", 0.08);
    add("the", 0.08);
    add("on", 0.04);
    add("want", 0.04);
    add("water", 0.03);
    add("wart", 0.01);
    add("sa", 0.02);
    add("hospital", 0.02, hospital);
    add("doctor", 0.01, doctor);
    add("hill", 0.01, hill);
    add("pizza", 0.02, pizza);
    add("form", 0.001);
    add("from", 0.5);

    WordPredictor::BigramSpec bi;
    bi.left = QStringLiteral("i");
    bi.next = {{QStringLiteral("want"), 0.55},
               {QStringLiteral("water"), 0.08},
               {QStringLiteral("wart"), 0.01}};
    f.bigrams.push_back(bi);

    WordPredictor::BigramSpec hillBi;
    hillBi.left = QStringLiteral("hill");
    hillBi.next = {{QStringLiteral("pizza"), 0.01}};
    f.bigrams.push_back(hillBi);

    WordPredictor::TrigramSpec tri;
    tri.left2 = QStringLiteral("the");
    tri.left1 = QStringLiteral("hill");
    tri.next = {{QStringLiteral("pizza"), 0.01}};
    f.trigrams.push_back(tri);
    return f;
}

WordPredictor load()
{
    WordPredictor p;
    QString err;
    const bool ok = p.loadFixture(fixture(), &err);
    if (!ok) {
        qWarning() << err;
    }
    return p;
}

QString joinHits(const QVector<WordPredictor::Hit>& hits)
{
    QStringList parts;
    for (const WordPredictor::Hit& h : hits) {
        parts.push_back(h.word);
    }
    return parts.join(QLatin1Char(','));
}

int indexOf(const QVector<WordPredictor::Hit>& hits, const QString& word)
{
    for (int i = 0; i < hits.size(); ++i) {
        if (hits[i].word == word) {
            return i;
        }
    }
    return -1;
}

} // namespace

class PredictTest final : public QObject {
    Q_OBJECT

private slots:
    void editsSurfaceIntendedWord();
    void farKeyDoesNot();
    void prefixBeatsNeighborEdit();
    void anchorsRankDoctorAboveUnrelated();
    void sentenceBoundaryDropsLeadingTopic();
    void realWordSurvivesMildEdit();
    void blobRoundTrip();
    void suggestIsQuick();
    void starterModelOffersWant();
    void dictionaryCompletesUnknownPrefix();
    void finishedWordIsNotExtended();
    void dictionaryOnlyWordIsNotANextWord();
};

void PredictTest::editsSurfaceIntendedWord()
{
    WordPredictor p = load();
    QVERIFY(p.isLoaded());
    WordPredictor::Query q;
    q.typed = QStringLiteral("hsopital");
    const auto swapped = p.suggest(q, 8);
    QVERIFY2(!swapped.isEmpty() && swapped[0].word == QLatin1String("hospital"),
             qPrintable(joinHits(swapped)));

    q.typed = QStringLiteral("hosptal");
    const auto missing = p.suggest(q, 8);
    QVERIFY2(!missing.isEmpty() && missing[0].word == QLatin1String("hospital"),
             qPrintable(joinHits(missing)));

    q.typed = QStringLiteral("hospiral");
    const auto neighbor = p.suggest(q, 8);
    QVERIFY2(!neighbor.isEmpty() && neighbor[0].word == QLatin1String("hospital"),
             qPrintable(joinHits(neighbor)));
}

void PredictTest::farKeyDoesNot()
{
    WordPredictor p = load();
    WordPredictor::Query q;
    q.typed = QStringLiteral("hospqtal");
    const auto hits = p.suggest(q, 8);
    QVERIFY2(indexOf(hits, QStringLiteral("hospital")) < 0, qPrintable(joinHits(hits)));
}

void PredictTest::prefixBeatsNeighborEdit()
{
    WordPredictor p = load();
    WordPredictor::Query q;
    q.sentenceWords = {QStringLiteral("i")};
    q.typed = QStringLiteral("wa");
    const auto hits = p.suggest(q, 8);
    QVERIFY2(!hits.isEmpty() && hits[0].word == QLatin1String("want"), qPrintable(joinHits(hits)));
    const int edit = indexOf(hits, QStringLiteral("sa"));
    if (edit >= 0) {
        QVERIFY(edit > 0);
    }
}

void PredictTest::anchorsRankDoctorAboveUnrelated()
{
    WordPredictor p = load();
    WordPredictor::Query q;
    q.sentenceWords = {QStringLiteral("hospital"), QStringLiteral("on"), QStringLiteral("the"),
                       QStringLiteral("hill")};
    const auto hits = p.suggest(q, 8);
    const int doctor = indexOf(hits, QStringLiteral("doctor"));
    const int pizza = indexOf(hits, QStringLiteral("pizza"));
    QVERIFY2(doctor >= 0 && pizza >= 0 && doctor < pizza, qPrintable(joinHits(hits)));
}

void PredictTest::sentenceBoundaryDropsLeadingTopic()
{
    const QString phrase = QStringLiteral("hospital is on the hill. pizza is");
    const WordPredictor::Query atEnd = WordPredictor::fromPhrase(phrase, phrase.size());
    QCOMPARE(atEnd.typed, QStringLiteral("is"));
    QCOMPARE(atEnd.sentenceWords, QVector<QString>({QStringLiteral("pizza")}));
    QVERIFY(!atEnd.offerPreviousCorrection);

    WordPredictor p = load();
    WordPredictor::Query topic;
    topic.sentenceWords = {QStringLiteral("hospital"), QStringLiteral("on"), QStringLiteral("the"),
                           QStringLiteral("hill")};
    const auto withTopic = p.suggest(topic, 8);
    WordPredictor::Query after;
    after.sentenceWords = {QStringLiteral("pizza")};
    const auto without = p.suggest(after, 8);
    QVERIFY2(indexOf(withTopic, QStringLiteral("doctor")) >= 0
                 && indexOf(withTopic, QStringLiteral("pizza")) >= 0
                 && indexOf(withTopic, QStringLiteral("doctor"))
                        < indexOf(withTopic, QStringLiteral("pizza")),
             qPrintable(joinHits(withTopic)));
    const int pizza = indexOf(without, QStringLiteral("pizza"));
    const int doctor = indexOf(without, QStringLiteral("doctor"));
    QVERIFY2(pizza >= 0 && (doctor < 0 || pizza < doctor), qPrintable(joinHits(without)));
}

void PredictTest::realWordSurvivesMildEdit()
{
    WordPredictor p = load();
    WordPredictor::Query q;
    q.typed = QStringLiteral("form");
    const auto hits = p.suggest(q, 8);
    QVERIFY2(!hits.isEmpty() && hits[0].word == QLatin1String("form"), qPrintable(joinHits(hits)));
}

void PredictTest::blobRoundTrip()
{
    WordPredictor p = load();
    const QString path = QDir::temp().filePath(QStringLiteral("gazer-predict-roundtrip.bin"));
    QFile::remove(path);
    QString err;
    QVERIFY2(p.saveFile(path, &err), qPrintable(err));
    WordPredictor loaded;
    QVERIFY2(loaded.loadFile(path, &err), qPrintable(err));
    WordPredictor::Query q;
    q.typed = QStringLiteral("hsopital");
    const auto hits = loaded.suggest(q, 8);
    QVERIFY2(!hits.isEmpty() && hits[0].word == QLatin1String("hospital"),
             qPrintable(joinHits(hits)));
    const auto next = loaded.suggest(WordPredictor::Query{}, 8);
    QVERIFY2(indexOf(next, QStringLiteral("from")) >= 0, qPrintable(joinHits(next)));
}

void PredictTest::suggestIsQuick()
{
    WordPredictor p = load();
    WordPredictor::Query q;
    q.typed = QStringLiteral("hsopital");
    for (int i = 0; i < 5; ++i) {
        QVERIFY(!p.suggest(q, 8).isEmpty());
    }
    QElapsedTimer timer;
    timer.start();
    constexpr int kN = 20;
    for (int i = 0; i < kN; ++i) {
        QVERIFY(!p.suggest(q, 8).isEmpty());
    }
    const double avg = double(timer.nsecsElapsed()) / double(kN) / 1e6;
    QVERIFY2(avg < 5.0, qPrintable(QString::number(avg)));
}

void PredictTest::starterModelOffersWant()
{
    const QStringList candidates = {
        QStringLiteral("resources/predict/model.bin"),
        QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("resources/predict/model.bin")),
    };
    QString path;
    for (const QString& c : candidates) {
        if (QFile::exists(c)) {
            path = c;
            break;
        }
    }
    if (path.isEmpty()) {
        QSKIP("starter model is not built");
    }
    WordPredictor p;
    QString err;
    QVERIFY2(p.loadFile(path, &err), qPrintable(err));
    const QString phrase = QStringLiteral("I ");
    const WordPredictor::Query q = WordPredictor::fromPhrase(phrase, phrase.size());
    const auto hits = p.suggest(q, 8);
    QVERIFY2(indexOf(hits, QStringLiteral("want")) >= 0, qPrintable(joinHits(hits)));
    WordPredictor::Query typed;
    typed.typed = QStringLiteral("keyb");
    const auto keys = p.suggest(typed, 8);
    QVERIFY2(indexOf(keys, QStringLiteral("keyboard")) >= 0, qPrintable(joinHits(keys)));
}

void PredictTest::dictionaryCompletesUnknownPrefix()
{
    WordPredictor p;
    QString err;
    const QStringList lines{QStringLiteral("I want water.")};
    const QVector<WordPredictor::UsageCount> usage{
        {QStringLiteral("water"), 500},
        {QStringLiteral("keyboard"), 80},
        {QStringLiteral("key"), 40},
    };
    QVERIFY2(p.buildFromSentences(lines, usage, &err), qPrintable(err));
    QVERIFY2(p.isLexiconWord(QStringLiteral("keyboard")), "keyboard missing from lexicon");
    WordPredictor::Query q;
    q.typed = QStringLiteral("keyb");
    const auto hits = p.suggest(q, 8);
    QVERIFY2(indexOf(hits, QStringLiteral("keyboard")) == 0, qPrintable(joinHits(hits)));
    q.typed = QStringLiteral("key");
    const auto both = p.suggest(q, 8);
    const int keyboard = indexOf(both, QStringLiteral("keyboard"));
    const int key = indexOf(both, QStringLiteral("key"));
    QVERIFY2(keyboard >= 0 && key >= 0 && keyboard < key, qPrintable(joinHits(both)));
}

void PredictTest::finishedWordIsNotExtended()
{
    WordPredictor p;
    QString err;
    const QStringList lines{QStringLiteral("I want water.")};
    const QVector<WordPredictor::UsageCount> usage{
        {QStringLiteral("water"), 500},
        {QStringLiteral("catch"), 90},
        {QStringLiteral("cat"), 54},
    };
    QVERIFY2(p.buildFromSentences(lines, usage, &err), qPrintable(err));
    const QString phrase = QStringLiteral("cat ");
    const WordPredictor::Query done = WordPredictor::fromPhrase(phrase, phrase.size());
    QVERIFY(done.offerPreviousCorrection);
    QVERIFY(done.typed.isEmpty());
    const auto hits = p.suggest(done, 8);
    QVERIFY2(!hits.isEmpty() && indexOf(hits, QStringLiteral("catch")) < 0, qPrintable(joinHits(hits)));

    WordPredictor::Query typing;
    typing.typed = QStringLiteral("cat");
    const auto open = p.suggest(typing, 8);
    const int catchAt = indexOf(open, QStringLiteral("catch"));
    const int catAt = indexOf(open, QStringLiteral("cat"));
    QVERIFY2(catchAt >= 0 && catAt >= 0 && catchAt < catAt, qPrintable(joinHits(open)));
}

void PredictTest::dictionaryOnlyWordIsNotANextWord()
{
    WordPredictor p;
    QString err;
    const QStringList lines{QStringLiteral("I want water.")};
    const QVector<WordPredictor::UsageCount> usage{
        {QStringLiteral("and"), 50000},
        {QStringLiteral("water"), 10},
    };
    QVERIFY2(p.buildFromSentences(lines, usage, &err), qPrintable(err));
    WordPredictor::Query q;
    const auto hits = p.suggest(q, 8);
    QVERIFY2(indexOf(hits, QStringLiteral("and")) < 0, qPrintable(joinHits(hits)));
    QVERIFY2(indexOf(hits, QStringLiteral("want")) >= 0, qPrintable(joinHits(hits)));

    const QString path = QDir::temp().filePath(QStringLiteral("gazer-predict-dict-roundtrip.bin"));
    QFile::remove(path);
    QVERIFY2(p.saveFile(path, &err), qPrintable(err));
    WordPredictor loaded;
    QVERIFY2(loaded.loadFile(path, &err), qPrintable(err));
    const auto again = loaded.suggest(q, 8);
    QVERIFY2(indexOf(again, QStringLiteral("and")) < 0, qPrintable(joinHits(again)));
    QVERIFY2(indexOf(again, QStringLiteral("want")) >= 0, qPrintable(joinHits(again)));
    WordPredictor::Query typed;
    typed.typed = QStringLiteral("an");
    const auto comp = loaded.suggest(typed, 8);
    QVERIFY2(indexOf(comp, QStringLiteral("and")) >= 0, qPrintable(joinHits(comp)));
}

QObject* createPredictTest()
{
    return new PredictTest();
}

#include "PredictTest.moc"
