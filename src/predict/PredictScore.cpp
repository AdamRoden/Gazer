#include "predict/PredictInternal.h"

#include <QSet>
#include <algorithm>
#include <cmath>

namespace gazer {
namespace predict_detail {
namespace {

const QSet<QString>& closedClass()
{
    static const QSet<QString> k = {
        QStringLiteral("a"),       QStringLiteral("an"),      QStringLiteral("the"),
        QStringLiteral("of"),      QStringLiteral("to"),      QStringLiteral("and"),
        QStringLiteral("in"),      QStringLiteral("on"),      QStringLiteral("for"),
        QStringLiteral("with"),    QStringLiteral("at"),      QStringLiteral("by"),
        QStringLiteral("from"),    QStringLiteral("as"),      QStringLiteral("is"),
        QStringLiteral("are"),     QStringLiteral("was"),     QStringLiteral("were"),
        QStringLiteral("be"),      QStringLiteral("been"),    QStringLiteral("being"),
        QStringLiteral("it"),      QStringLiteral("its"),     QStringLiteral("this"),
        QStringLiteral("that"),    QStringLiteral("these"),   QStringLiteral("those"),
        QStringLiteral("i"),       QStringLiteral("you"),     QStringLiteral("he"),
        QStringLiteral("she"),     QStringLiteral("we"),      QStringLiteral("they"),
        QStringLiteral("me"),      QStringLiteral("him"),     QStringLiteral("her"),
        QStringLiteral("us"),      QStringLiteral("them"),    QStringLiteral("my"),
        QStringLiteral("your"),    QStringLiteral("his"),     QStringLiteral("our"),
        QStringLiteral("their"),   QStringLiteral("mine"),    QStringLiteral("yours"),
        QStringLiteral("ours"),    QStringLiteral("theirs"),  QStringLiteral("not"),
        QStringLiteral("no"),      QStringLiteral("nor"),     QStringLiteral("so"),
        QStringLiteral("if"),      QStringLiteral("or"),      QStringLiteral("but"),
        QStringLiteral("because"), QStringLiteral("while"),   QStringLiteral("although"),
        QStringLiteral("though"),  QStringLiteral("than"),    QStringLiteral("then"),
        QStringLiteral("too"),     QStringLiteral("very"),    QStringLiteral("can"),
        QStringLiteral("could"),   QStringLiteral("should"),  QStringLiteral("would"),
        QStringLiteral("may"),     QStringLiteral("might"),   QStringLiteral("must"),
        QStringLiteral("will"),    QStringLiteral("shall"),   QStringLiteral("do"),
        QStringLiteral("does"),    QStringLiteral("did"),     QStringLiteral("doing"),
        QStringLiteral("done"),    QStringLiteral("have"),    QStringLiteral("has"),
        QStringLiteral("had"),     QStringLiteral("having"),  QStringLiteral("about"),
        QStringLiteral("into"),    QStringLiteral("over"),    QStringLiteral("after"),
        QStringLiteral("before"),  QStringLiteral("under"),   QStringLiteral("again"),
        QStringLiteral("further"), QStringLiteral("once"),    QStringLiteral("here"),
        QStringLiteral("there"),   QStringLiteral("when"),    QStringLiteral("where"),
        QStringLiteral("why"),     QStringLiteral("how"),     QStringLiteral("all"),
        QStringLiteral("each"),    QStringLiteral("few"),     QStringLiteral("more"),
        QStringLiteral("most"),    QStringLiteral("other"),   QStringLiteral("some"),
        QStringLiteral("such"),    QStringLiteral("only"),    QStringLiteral("own"),
        QStringLiteral("same"),    QStringLiteral("just"),    QStringLiteral("also"),
        QStringLiteral("up"),      QStringLiteral("down"),    QStringLiteral("out"),
        QStringLiteral("off"),     QStringLiteral("above"),   QStringLiteral("below"),
        QStringLiteral("between"), QStringLiteral("against"), QStringLiteral("during"),
        QStringLiteral("without"), QStringLiteral("within"),  QStringLiteral("along"),
        QStringLiteral("across"),  QStringLiteral("around"),  QStringLiteral("through"),
        QStringLiteral("per"),     QStringLiteral("via"),     QStringLiteral("what"),
        QStringLiteral("which"),   QStringLiteral("who"),     QStringLiteral("whom"),
        QStringLiteral("whose"),   QStringLiteral("am"),      QStringLiteral("now"),
        QStringLiteral("don't"),   QStringLiteral("isn't"),   QStringLiteral("aren't"),
        QStringLiteral("wasn't"),  QStringLiteral("weren't"), QStringLiteral("can't"),
        QStringLiteral("couldn't"), QStringLiteral("wouldn't"), QStringLiteral("shouldn't"),
        QStringLiteral("i'm"),     QStringLiteral("i've"),    QStringLiteral("i'll"),
        QStringLiteral("i'd"),     QStringLiteral("you're"),  QStringLiteral("you've"),
        QStringLiteral("you'll"),  QStringLiteral("you'd"),   QStringLiteral("we're"),
        QStringLiteral("we've"),   QStringLiteral("we'll"),   QStringLiteral("that's"),
        QStringLiteral("it's"),    QStringLiteral("he's"),    QStringLiteral("she's"),
        QStringLiteral("there's"), QStringLiteral("here's"),  QStringLiteral("what's"),
        QStringLiteral("who's"),   QStringLiteral("let's"),
    };
    return k;
}

void lastContext(const QVector<int>& ctx, int& prev2, int& prev1)
{
    prev2 = -1;
    prev1 = -1;
    for (int id : ctx) {
        if (id < 0) {
            continue;
        }
        prev2 = prev1;
        prev1 = id;
    }
}

double cosine(const QVector<double>& pooled, const QVector<qint8>& emb)
{
    if (pooled.size() != emb.size() || pooled.isEmpty()) {
        return 0;
    }
    double dot = 0;
    double na = 0;
    double nb = 0;
    for (int i = 0; i < pooled.size(); ++i) {
        const double b = double(emb[i]);
        dot += pooled[i] * b;
        na += pooled[i] * pooled[i];
        nb += b * b;
    }
    if (na < 1e-9 || nb < 1e-9) {
        return 0;
    }
    return dot / std::sqrt(na * nb);
}

float triggerOf(const PredictModel& m, int src, int dst)
{
    const auto it = m.triggers.constFind(src);
    if (it == m.triggers.cend()) {
        return 0.f;
    }
    return lookupCont(it.value(), dst);
}

void pushUnique(QVector<int>& ids, QSet<int>& seen, int id, int cap)
{
    if (id < 0 || seen.contains(id) || ids.size() >= cap) {
        return;
    }
    seen.insert(id);
    ids.push_back(id);
}

} // namespace

bool isWordChar(QChar c)
{
    return c.isLetterOrNumber() || c == QLatin1Char('\'') || c == QLatin1Char('-');
}

QString normWord(QStringView word)
{
    const QString raw = word.toString().toLower();
    int a = 0;
    int b = raw.size();
    while (a < b && !isWordChar(raw[a])) {
        ++a;
    }
    while (b > a && !isWordChar(raw[b - 1])) {
        --b;
    }
    return raw.mid(a, b - a);
}

bool isClosedClass(const QString& norm)
{
    return closedClass().contains(norm);
}

int lexiconId(const PredictModel& m, const QString& norm)
{
    const auto it = m.idOf.constFind(norm);
    return it == m.idOf.cend() ? -1 : it.value();
}

const QString& wordText(const PredictModel& m, int id)
{
    static const QString kEmpty;
    if (id >= 0 && id < m.words.size()) {
        return m.words[id].text;
    }
    const int oov = id - m.words.size();
    if (oov >= 0 && oov < m.oovText.size()) {
        return m.oovText[oov];
    }
    return kEmpty;
}

float uniOf(const PredictModel& m, int id)
{
    if (id >= 0 && id < m.words.size()) {
        return m.words[id].uni;
    }
    return 0.f;
}

void ensureOov(PredictModel& m, const QString& norm)
{
    if (norm.isEmpty() || lexiconId(m, norm) >= 0 || m.oovId.contains(norm)) {
        return;
    }
    const int id = m.words.size() + m.oovText.size();
    m.oovText.push_back(norm);
    m.oovId.insert(norm, id);
}

void clearUser(PredictModel& m)
{
    m.userUni.clear();
    m.userBi.clear();
    m.userTotal = 0;
    m.oovId.clear();
    m.oovText.clear();
}

quint64 triKey(int left2, int left1)
{
    return (quint64(quint32(left2)) << 32) | quint32(left1);
}

float lookupCont(const QVector<PredictCont>& rows, int word)
{
    int lo = 0;
    int hi = rows.size();
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        if (rows[mid].word < word) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if (lo < rows.size() && rows[lo].word == word) {
        return rows[lo].p;
    }
    return 0.f;
}

QVector<int> contextIds(const PredictModel& m, const QVector<QString>& words)
{
    QVector<int> ids;
    ids.reserve(words.size());
    for (const QString& w : words) {
        const QString n = normWord(w);
        int id = lexiconId(m, n);
        if (id < 0) {
            const auto oov = m.oovId.constFind(n);
            id = oov == m.oovId.cend() ? -1 : oov.value();
        }
        ids.push_back(id);
    }
    return ids;
}

AnchorState makeAnchors(const PredictModel& m, const QVector<int>& ctx)
{
    AnchorState state;
    const int n = ctx.size();
    if (n <= 0) {
        return state;
    }
    struct Row {
        int id;
        double weight;
        int index;
    };
    QVector<Row> rows;
    for (int i = 0; i < n; ++i) {
        const int id = ctx[i];
        if (id < 0 || id >= m.words.size()) {
            continue;
        }
        if (isClosedClass(m.words[id].text)) {
            continue;
        }
        const double uni = std::max(double(m.words[id].uni), 1e-6);
        double idf = -std::log(uni);
        if (idf < 1.2) {
            continue;
        }
        idf = std::min(idf, 8.0);
        const double lead = 1.0 + 0.5 * (1.0 - double(i) / double(std::max(n, 1)));
        const double recency = std::exp(-0.15 * double(n - 1 - i));
        rows.push_back({id, idf * lead * recency, i});
    }
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
        return a.weight > b.weight;
    });
    if (rows.size() > 4) {
        rows.resize(4);
    }
    if (rows.isEmpty()) {
        return state;
    }
    state.active = true;
    state.vec = QVector<double>(std::max(0, m.embDim), 0.0);
    for (const Row& row : rows) {
        state.items.push_back({row.id, row.weight});
        const QVector<qint8>& emb = m.words[row.id].emb;
        if (emb.size() == state.vec.size()) {
            for (int d = 0; d < emb.size(); ++d) {
                state.vec[d] += row.weight * double(emb[d]);
            }
        }
    }
    return state;
}

double mixP(const PredictModel& m, int id, const QVector<int>& ctx, const QHash<QString, int>& cache,
            int cacheN, const AnchorState& anchors)
{
    if (id < 0) {
        return 0;
    }
    int prev2 = -1;
    int prev1 = -1;
    lastContext(ctx, prev2, prev1);
    const float p1 = uniOf(m, id);
    float p2 = 0.f;
    float p3 = 0.f;
    if (prev1 >= 0 && prev1 < m.words.size()) {
        const auto bi = m.bigram.constFind(prev1);
        if (bi != m.bigram.cend()) {
            p2 = lookupCont(bi.value(), id);
        }
    }
    if (prev1 >= 0 && prev2 >= 0 && prev1 < m.words.size() && prev2 < m.words.size()) {
        const auto tri = m.trigram.constFind(triKey(prev2, prev1));
        if (tri != m.trigram.cend()) {
            p3 = lookupCont(tri.value(), id);
        }
    }
    const QString& text = wordText(m, id);
    double pc = 0;
    if (cacheN > 0 && !text.isEmpty()) {
        pc = double(cache.value(text)) / double(cacheN);
    }
    double pu = 0;
    if (m.userTotal > 0 && !text.isEmpty()) {
        const double uni = double(m.userUni.value(text)) / double(m.userTotal);
        double bi = uni;
        if (prev1 >= 0) {
            const QString& prev = wordText(m, prev1);
            const int prevCount = m.userUni.value(prev);
            if (prevCount > 0 && !prev.isEmpty()) {
                const int both = m.userBi.value(prev + QLatin1Char('\t') + text);
                bi = 0.65 * (double(both) / double(prevCount)) + 0.35 * uni;
            }
        }
        pu = bi;
    }
    double pa = 0;
    if (anchors.active && p1 > 0.f && id < m.words.size()) {
        const double cos = cosine(anchors.vec, m.words[id].emb);
        double bonus = 0;
        double wsum = 0;
        for (const AnchorState::Item& a : anchors.items) {
            wsum += a.weight;
        }
        if (wsum > 0) {
            for (const AnchorState::Item& a : anchors.items) {
                bonus += (a.weight / wsum) * double(triggerOf(m, a.id, id));
            }
        }
        if (cos > 0.0 || bonus > 0.0) {
            pa = double(p1) * std::exp(m.w.anchorGain * std::max(0.0, cos) + bonus);
        }
    }
    const double userMix = m.w.lambdaUser * std::min(1.0, double(m.userTotal) / 200.0);
    return m.w.lambda3 * double(p3) + m.w.lambda2 * double(p2) + m.w.lambda1 * double(p1)
           + m.w.lambdaAnchor * pa + m.w.lambdaCache * pc + userMix * pu;
}

QHash<int, float> nextEdits(const PredictModel& m, const QVector<int>& ctx, const AnchorState& anchors,
                            const QHash<QString, int>& cache)
{
    QVector<int> ids;
    QSet<int> seen;
    constexpr int kCap = 200;
    for (int id : m.topUni) {
        pushUnique(ids, seen, id, kCap);
    }
    int prev2 = -1;
    int prev1 = -1;
    lastContext(ctx, prev2, prev1);
    const auto take = [&](const QVector<PredictCont>& rows) {
        for (const PredictCont& c : rows) {
            pushUnique(ids, seen, c.word, kCap);
        }
    };
    if (prev1 >= 0 && prev2 >= 0) {
        const auto tri = m.trigram.constFind(triKey(prev2, prev1));
        if (tri != m.trigram.cend()) {
            take(tri.value());
        }
    }
    if (prev1 >= 0) {
        const auto bi = m.bigram.constFind(prev1);
        if (bi != m.bigram.cend()) {
            take(bi.value());
        }
    }
    for (const AnchorState::Item& a : anchors.items) {
        const auto tr = m.triggers.constFind(a.id);
        if (tr != m.triggers.cend()) {
            take(tr.value());
        }
    }
    if (anchors.active && m.embDim > 0) {
        QVector<QPair<double, int>> sims;
        for (int i = 0; i < m.words.size(); ++i) {
            if (m.words[i].emb.size() != m.embDim) {
                continue;
            }
            const double c = cosine(anchors.vec, m.words[i].emb);
            if (c > 0.05) {
                sims.push_back({c, i});
            }
        }
        const int n = std::min(32, int(sims.size()));
        std::partial_sort(sims.begin(), sims.begin() + n, sims.end(),
                          [](const QPair<double, int>& a, const QPair<double, int>& b) {
                              return a.first > b.first;
                          });
        for (int i = 0; i < n; ++i) {
            pushUnique(ids, seen, sims[i].second, kCap);
        }
    }
    for (auto it = cache.cbegin(); it != cache.cend(); ++it) {
        int id = lexiconId(m, it.key());
        if (id < 0) {
            const auto oov = m.oovId.constFind(it.key());
            id = oov == m.oovId.cend() ? -1 : oov.value();
        }
        pushUnique(ids, seen, id, kCap);
    }
    for (auto it = m.userUni.cbegin(); it != m.userUni.cend(); ++it) {
        if (it.value() <= 0) {
            continue;
        }
        int id = lexiconId(m, it.key());
        if (id < 0) {
            const auto oov = m.oovId.constFind(it.key());
            id = oov == m.oovId.cend() ? -1 : oov.value();
        }
        pushUnique(ids, seen, id, kCap);
    }
    QHash<int, float> edits;
    for (int id : ids) {
        edits.insert(id, 0.f);
    }
    return edits;
}

QVector<Scored> rankEdits(const PredictModel& m, const QVector<int>& ctx,
                          const QHash<QString, int>& cache, int cacheN, const QHash<int, float>& edits,
                          const AnchorState& anchors)
{
    QVector<Scored> out;
    out.reserve(edits.size());
    for (auto it = edits.cbegin(); it != edits.cend(); ++it) {
        const double p = mixP(m, it.key(), ctx, cache, cacheN, anchors);
        if (!(p > 0.0)) {
            continue;
        }
        Scored s;
        s.id = it.key();
        s.edit = it.value();
        s.score = std::log(p) - m.w.beta * double(it.value());
        out.push_back(s);
    }
    std::sort(out.begin(), out.end(), [](const Scored& a, const Scored& b) {
        if (a.score != b.score) {
            return a.score > b.score;
        }
        return a.id < b.id;
    });
    return out;
}

} // namespace predict_detail
} // namespace gazer
