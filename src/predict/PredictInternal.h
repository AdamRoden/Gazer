#pragma once

#include "predict/WordPredictor.h"

#include <QHash>
#include <QMap>
#include <QString>
#include <QVector>

namespace gazer {

struct LexWord {
    QString text;
    float uni = 0;
    bool proper = false;
    /// True when the word occurred in the sentence corpus. Next-word
    /// suggestions are drawn from these words.
    bool inSentences = false;
    QVector<qint8> emb;
};

struct PredictCont {
    int word = -1;
    float p = 0;
};

struct TrieNode {
    int wordId = -1;
    int bestWord = -1;
    int edgeStart = 0;
    int edgeCount = 0;
};

struct TrieEdge {
    ushort ch = 0;
    int to = -1;
};

struct AnchorState {
    struct Item {
        int id = -1;
        double weight = 0;
    };
    QVector<Item> items;
    QVector<double> vec;
    bool active = false;
};

struct PredictModel {
    WordPredictor::Weights w;
    QVector<LexWord> words;
    QHash<QString, int> idOf;
    QVector<TrieNode> nodes;
    QVector<TrieEdge> edges;
    QHash<int, QVector<PredictCont>> bigram;
    QHash<quint64, QVector<PredictCont>> trigram;
    QHash<int, QVector<PredictCont>> triggers;
    QVector<int> topUni;
    QHash<QString, QVector<int>> phonetic;
    int embDim = 0;

    QHash<QString, int> userUni;
    QHash<QString, int> userBi;
    int userTotal = 0;
    QHash<QString, int> oovId;
    QVector<QString> oovText;
};

namespace predict_detail {

[[nodiscard]] QString normWord(QStringView word);
[[nodiscard]] bool isClosedClass(const QString& norm);
[[nodiscard]] int lexiconId(const PredictModel& m, const QString& norm);
[[nodiscard]] const QString& wordText(const PredictModel& m, int id);
[[nodiscard]] float uniOf(const PredictModel& m, int id);
void ensureOov(PredictModel& m, const QString& norm);
void clearUser(PredictModel& m);
void rebuildTrie(PredictModel& m);
void rebuildPhonetic(PredictModel& m);
void rebuildTopUni(PredictModel& m);

[[nodiscard]] QVector<int> contextIds(const PredictModel& m, const QVector<QString>& words);
[[nodiscard]] AnchorState makeAnchors(const PredictModel& m, const QVector<int>& ctx);
[[nodiscard]] double mixP(const PredictModel& m, int id, const QVector<int>& ctx,
                          const QHash<QString, int>& cache, int cacheN, const AnchorState& anchors);

struct Scored {
    int id = -1;
    double score = 0;
    float edit = 0;
};

[[nodiscard]] QHash<int, float> beamEdits(const PredictModel& m, const QString& typed);
void addPhonetic(const PredictModel& m, const QString& typed, QHash<int, float>& edits);
[[nodiscard]] QHash<int, float> nextEdits(const PredictModel& m, const QVector<int>& ctx,
                                          const AnchorState& anchors,
                                          const QHash<QString, int>& cache);
[[nodiscard]] QVector<Scored> rankEdits(const PredictModel& m, const QVector<int>& ctx,
                                        const QHash<QString, int>& cache, int cacheN,
                                        const QHash<int, float>& edits, const AnchorState& anchors);

[[nodiscard]] quint64 triKey(int left2, int left1);
[[nodiscard]] float lookupCont(const QVector<PredictCont>& rows, int word);

} // namespace predict_detail
} // namespace gazer
