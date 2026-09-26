#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <memory>

namespace gazer {

struct PredictModel;

/// Sentence-scoped spelling and next-word ranker. One query is the current
/// sentence only. Call it when the phrase changes, not from the gaze sample.
class WordPredictor {
public:
    static constexpr int kMaxSuggestions = 8;

    struct Weights {
        double lambda3 = 0.45;
        double lambda2 = 0.22;
        double lambda1 = 0.13;
        double lambdaAnchor = 0.12;
        double lambdaCache = 0.05;
        double lambdaUser = 0.03;
        double beta = 7.7;
        double margin = 100.0;
        double costTranspose = 0.6;
        double costSubNeighbor = 0.8;
        double costSubDiag = 1.3;
        /// Above `costLimit`, so a single far-key substitution never enters the beam.
        double costSubOther = 2.6;
        double costInsertDelete = 1.1;
        double costRepeat = 0.5;
        /// One missing letter or two cheap slips fit. A far-key swap (delete + insert) does not.
        double costLimit = 2.0;
        double anchorGain = 4.0;
    };

    struct Query {
        /// Committed words before the caret, current sentence only.
        QVector<QString> sentenceWords;
        /// Normalized letters of the token under the caret. Empty at a word boundary.
        QString typed;
        bool offerPreviousCorrection = false;
        /// True when `typed` is the first word of the sentence.
        bool sentenceStart = false;
        int caret = 0;
        int activeStart = 0;
        int activeEnd = 0;
        int previousStart = -1;
        int previousEnd = -1;
    };

    struct Hit {
        QString word;
        bool replacesPrevious = false;
        bool proper = false;
    };

    struct WordSpec {
        QString text;
        double unigram = 0;
        bool proper = false;
        QVector<qint8> embedding;
    };
    struct Link {
        QString word;
        double probability = 0;
    };
    struct BigramSpec {
        QString left;
        QVector<Link> next;
    };
    struct TrigramSpec {
        QString left2;
        QString left1;
        QVector<Link> next;
    };
    struct TriggerSpec {
        QString from;
        QString to;
        double bonus = 0;
    };
    struct Fixture {
        QVector<WordSpec> words;
        QVector<BigramSpec> bigrams;
        QVector<TrigramSpec> trigrams;
        QVector<TriggerSpec> triggers;
        Weights weights;
    };

    WordPredictor();
    ~WordPredictor();
    WordPredictor(WordPredictor&&) noexcept;
    WordPredictor& operator=(WordPredictor&&) noexcept;
    WordPredictor(const WordPredictor&) = delete;
    WordPredictor& operator=(const WordPredictor&) = delete;

    /// Tokenize `text` up to `caret` into a sentence-scoped query.
    [[nodiscard]] static Query fromPhrase(const QString& text, int caret);

    bool loadFixture(const Fixture& fixture, QString* error = nullptr);
    bool loadFile(const QString& path, QString* error = nullptr);
    bool saveFile(const QString& path, QString* error = nullptr) const;
    /// One dictionary entry: a word and how often it is used.
    struct UsageCount {
        QString word;
        int count = 0;
    };

    /// Train from one sentence per line (also splits on . ? !). Replaces the lexicon.
    bool buildFromSentences(const QStringList& lines, QString* error = nullptr);
    /// Same training, plus a usage-ranked dictionary for words the sentences never contain.
    bool buildFromSentences(const QStringList& lines, const QVector<UsageCount>& usage,
                            QString* error = nullptr);

    [[nodiscard]] bool isLoaded() const;
    [[nodiscard]] bool isLexiconWord(const QString& word) const;
    [[nodiscard]] QVector<Hit> suggest(const Query& query, int limit) const;

    /// Count an accepted or exactly typed word. `left` is the sentence context before it.
    void observe(const QStringList& left, const QString& word);
    bool loadUser(const QString& path);
    bool saveUser(const QString& path) const;

private:
    std::unique_ptr<PredictModel> d;
};

} // namespace gazer
