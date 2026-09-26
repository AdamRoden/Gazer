#include "predict/PredictInternal.h"

#include <QMap>
#include <algorithm>
#include <cmath>

namespace gazer {
namespace {

void sortCont(QVector<PredictCont>& rows)
{
    std::sort(rows.begin(), rows.end(), [](const PredictCont& a, const PredictCont& b) {
        return a.word < b.word;
    });
}

} // namespace

WordPredictor::WordPredictor()
    : d(std::make_unique<PredictModel>())
{
}

WordPredictor::~WordPredictor() = default;
WordPredictor::WordPredictor(WordPredictor&&) noexcept = default;
WordPredictor& WordPredictor::operator=(WordPredictor&&) noexcept = default;

namespace predict_detail {

void rebuildTrie(PredictModel& m)
{
    struct BNode {
        int wordId = -1;
        QMap<ushort, int> next;
    };
    QVector<BNode> built;
    built.push_back(BNode{});
    for (int id = 0; id < m.words.size(); ++id) {
        int node = 0;
        for (const QChar ch : m.words[id].text) {
            const ushort u = ch.unicode();
            const auto it = built[node].next.constFind(u);
            if (it == built[node].next.cend()) {
                const int to = built.size();
                built.push_back(BNode{});
                built[node].next.insert(u, to);
                node = to;
            } else {
                node = it.value();
            }
        }
        built[node].wordId = id;
    }
    m.nodes.resize(built.size());
    m.edges.clear();
    m.edges.reserve(built.size());
    for (int i = 0; i < built.size(); ++i) {
        m.nodes[i].wordId = built[i].wordId;
        m.nodes[i].bestWord = -1;
        m.nodes[i].edgeStart = m.edges.size();
        m.nodes[i].edgeCount = built[i].next.size();
        for (auto it = built[i].next.cbegin(); it != built[i].next.cend(); ++it) {
            TrieEdge e;
            e.ch = it.key();
            e.to = it.value();
            m.edges.push_back(e);
        }
    }
    const auto walk = [&](auto&& self, int i) -> int {
        int best = m.nodes[i].wordId;
        float bestP = best >= 0 ? m.words[best].uni : -1.f;
        for (int k = 0; k < m.nodes[i].edgeCount; ++k) {
            const int child = self(self, m.edges[m.nodes[i].edgeStart + k].to);
            if (child >= 0 && m.words[child].uni > bestP) {
                best = child;
                bestP = m.words[child].uni;
            }
        }
        m.nodes[i].bestWord = best;
        return best;
    };
    if (!m.nodes.isEmpty()) {
        walk(walk, 0);
    }
}

void rebuildTopUni(PredictModel& m)
{
    QVector<int> ids;
    ids.reserve(m.words.size());
    for (int i = 0; i < m.words.size(); ++i) {
        if (m.words[i].inSentences) {
            ids.push_back(i);
        }
    }
    std::sort(ids.begin(), ids.end(), [&](int a, int b) {
        if (m.words[a].uni != m.words[b].uni) {
            return m.words[a].uni > m.words[b].uni;
        }
        return a < b;
    });
    if (ids.size() > 48) {
        ids.resize(48);
    }
    m.topUni = ids;
}

} // namespace predict_detail

bool WordPredictor::isLoaded() const
{
    return d && !d->words.isEmpty();
}

bool WordPredictor::isLexiconWord(const QString& word) const
{
    return d && predict_detail::lexiconId(*d, predict_detail::normWord(word)) >= 0;
}

bool WordPredictor::loadFixture(const Fixture& fixture, QString* error)
{
    auto fail = [&](const QString& msg) {
        if (error) {
            *error = msg;
        }
        return false;
    };
    PredictModel next;
    next.w = fixture.weights;
    QHash<QString, int> idOf;
    for (const WordSpec& spec : fixture.words) {
        const QString text = predict_detail::normWord(spec.text);
        if (text.isEmpty()) {
            return fail(QStringLiteral("Empty fixture word"));
        }
        if (idOf.contains(text)) {
            return fail(QStringLiteral("Duplicate fixture word %1").arg(text));
        }
        if (!spec.embedding.isEmpty()) {
            if (next.embDim == 0) {
                next.embDim = spec.embedding.size();
            } else if (spec.embedding.size() != next.embDim) {
                return fail(QStringLiteral("Embedding width mismatch for %1").arg(text));
            }
        }
        LexWord w;
        w.text = text;
        w.uni = float(std::max(0.0, spec.unigram));
        w.proper = spec.proper;
        w.inSentences = true;
        w.emb = spec.embedding;
        idOf.insert(text, next.words.size());
        next.words.push_back(std::move(w));
    }
    if (next.words.isEmpty()) {
        return fail(QStringLiteral("Fixture has no words"));
    }
    next.idOf = idOf;
    const auto resolve = [&](const QVector<Link>& links, QVector<PredictCont>& out) {
        for (const Link& link : links) {
            const int id = idOf.value(predict_detail::normWord(link.word), -1);
            if (id < 0 || !(link.probability > 0.0)) {
                continue;
            }
            PredictCont c;
            c.word = id;
            c.p = float(link.probability);
            out.push_back(c);
        }
        sortCont(out);
    };
    for (const BigramSpec& spec : fixture.bigrams) {
        const int left = idOf.value(predict_detail::normWord(spec.left), -1);
        if (left < 0) {
            continue;
        }
        QVector<PredictCont> rows;
        resolve(spec.next, rows);
        if (!rows.isEmpty()) {
            next.bigram.insert(left, rows);
        }
    }
    for (const TrigramSpec& spec : fixture.trigrams) {
        const int a = idOf.value(predict_detail::normWord(spec.left2), -1);
        const int b = idOf.value(predict_detail::normWord(spec.left1), -1);
        if (a < 0 || b < 0) {
            continue;
        }
        QVector<PredictCont> rows;
        resolve(spec.next, rows);
        if (!rows.isEmpty()) {
            next.trigram.insert(predict_detail::triKey(a, b), rows);
        }
    }
    for (const TriggerSpec& spec : fixture.triggers) {
        const int from = idOf.value(predict_detail::normWord(spec.from), -1);
        const int to = idOf.value(predict_detail::normWord(spec.to), -1);
        if (from < 0 || to < 0 || !(spec.bonus > 0.0)) {
            continue;
        }
        PredictCont c;
        c.word = to;
        c.p = float(spec.bonus);
        next.triggers[from].push_back(c);
    }
    for (auto it = next.triggers.begin(); it != next.triggers.end(); ++it) {
        sortCont(it.value());
    }
    predict_detail::rebuildTrie(next);
    predict_detail::rebuildPhonetic(next);
    predict_detail::rebuildTopUni(next);
    *d = std::move(next);
    return true;
}

WordPredictor::Query WordPredictor::fromPhrase(const QString& text, int caret)
{
    Query q;
    const int n = text.size();
    const int c = std::clamp(caret, 0, n);
    q.caret = c;
    struct Tok {
        int start = 0;
        int end = 0;
    };
    QVector<Tok> toks;
    for (int i = 0; i < n;) {
        if (text[i].isSpace()) {
            ++i;
            continue;
        }
        int j = i;
        while (j < n && !text[j].isSpace()) {
            ++j;
        }
        toks.push_back({i, j});
        i = j;
    }
    int active = -1;
    for (int i = 0; i < toks.size(); ++i) {
        if ((c > toks[i].start && c < toks[i].end) || c == toks[i].end) {
            active = i;
            break;
        }
    }
    int contextEnd = c;
    if (active >= 0) {
        q.activeStart = toks[active].start;
        q.activeEnd = toks[active].end;
        q.typed = predict_detail::normWord(text.mid(q.activeStart, q.activeEnd - q.activeStart));
        contextEnd = q.activeStart;
    } else {
        q.activeStart = c;
        q.activeEnd = c;
        int prev = -1;
        for (int i = 0; i < toks.size(); ++i) {
            if (toks[i].end <= c) {
                prev = i;
            }
        }
        if (prev >= 0) {
            q.previousStart = toks[prev].start;
            q.previousEnd = toks[prev].end;
        }
    }
    int sent = 0;
    for (int i = 0; i < contextEnd; ++i) {
        const QChar ch = text[i];
        if (ch == QLatin1Char('.') || ch == QLatin1Char('?') || ch == QLatin1Char('!')) {
            sent = i + 1;
        }
    }
    if (q.previousStart >= 0 && q.previousStart < sent) {
        q.previousStart = -1;
        q.previousEnd = -1;
    }
    if (active < 0 && q.previousStart >= 0) {
        q.offerPreviousCorrection = true;
    }
    for (const Tok& t : toks) {
        if (t.end > contextEnd || t.start < sent) {
            continue;
        }
        if (active >= 0 && t.start == q.activeStart) {
            continue;
        }
        const QString w = predict_detail::normWord(text.mid(t.start, t.end - t.start));
        if (!w.isEmpty()) {
            q.sentenceWords.push_back(w);
        }
    }
    q.sentenceStart = q.sentenceWords.isEmpty();
    return q;
}

void WordPredictor::observe(const QStringList& left, const QString& word)
{
    if (!d) {
        return;
    }
    const QString w = predict_detail::normWord(word);
    if (w.isEmpty()) {
        return;
    }
    if (!d->userUni.contains(w) && d->userUni.size() >= 4000) {
        return;
    }
    d->userUni[w] += 1;
    d->userTotal += 1;
    predict_detail::ensureOov(*d, w);
    if (left.isEmpty()) {
        return;
    }
    const QString prev = predict_detail::normWord(left.last());
    if (prev.isEmpty()) {
        return;
    }
    const QString key = prev + QLatin1Char('\t') + w;
    if (!d->userBi.contains(key) && d->userBi.size() >= 8000) {
        return;
    }
    d->userBi[key] += 1;
    predict_detail::ensureOov(*d, prev);
}

namespace {

struct WordContext {
    QVector<int> ctx;
    QHash<QString, int> cache;
    int cacheN = 0;
    AnchorState anchors;
};

WordContext packWords(const PredictModel& m, const QVector<QString>& words)
{
    WordContext packed;
    packed.ctx = predict_detail::contextIds(m, words);
    for (const QString& w : words) {
        const QString n = predict_detail::normWord(w);
        if (n.isEmpty()) {
            continue;
        }
        packed.cache[n] += 1;
        ++packed.cacheN;
    }
    packed.anchors = predict_detail::makeAnchors(m, packed.ctx);
    return packed;
}

QVector<predict_detail::Scored> rankTyped(const PredictModel& m, const QString& typed,
                                          const WordContext& packed, bool keepCompletions)
{
    QHash<int, float> edits = predict_detail::beamEdits(m, typed);
    const int exact = predict_detail::lexiconId(m, typed);
    if (exact >= 0) {
        const float have = edits.value(exact, 1.f);
        edits.insert(exact, std::min(have, 0.f));
    }
    if (edits.isEmpty()) {
        predict_detail::addPhonetic(m, typed, edits);
    }
    if (exact >= 0 && edits.contains(exact)) {
        const double base = std::max(
            predict_detail::mixP(m, exact, packed.ctx, packed.cache, packed.cacheN, packed.anchors),
            1e-15);
        QHash<int, float> kept;
        for (auto it = edits.cbegin(); it != edits.cend(); ++it) {
            // A zero-cost extension continues the caret token. A finished word still clears the margin.
            const bool completion = keepCompletions && it.value() == 0.f;
            if (it.key() == exact || completion) {
                kept.insert(it.key(), it.value());
                continue;
            }
            const double mix = predict_detail::mixP(m, it.key(), packed.ctx, packed.cache, packed.cacheN,
                                                    packed.anchors);
            const double ratio = (mix / base) * std::exp(-m.w.beta * double(it.value()));
            if (ratio >= m.w.margin) {
                kept.insert(it.key(), it.value());
            }
        }
        edits = kept;
    }
    return predict_detail::rankEdits(m, packed.ctx, packed.cache, packed.cacheN, edits, packed.anchors);
}

} // namespace

QVector<WordPredictor::Hit> WordPredictor::suggest(const Query& query, int limit) const
{
    QVector<Hit> hits;
    if (!d || d->words.isEmpty()) {
        return hits;
    }
    limit = std::clamp(limit, 1, kMaxSuggestions);
    const WordContext here = packWords(*d, query.sentenceWords);

    const auto appendRanked = [&](const QVector<predict_detail::Scored>& ranked) {
        for (const predict_detail::Scored& s : ranked) {
            if (hits.size() >= limit) {
                return;
            }
            Hit h;
            h.word = predict_detail::wordText(*d, s.id);
            h.proper = s.id >= 0 && s.id < d->words.size() && d->words[s.id].proper;
            if (h.word.isEmpty()) {
                continue;
            }
            bool dup = false;
            for (const Hit& have : hits) {
                if (have.word == h.word) {
                    dup = true;
                    break;
                }
            }
            if (!dup) {
                hits.push_back(h);
            }
        }
    };

    const QString typed = predict_detail::normWord(query.typed);
    if (!typed.isEmpty()) {
        appendRanked(rankTyped(*d, typed, here, true));
        return hits;
    }

    if (query.offerPreviousCorrection && !query.sentenceWords.isEmpty()) {
        const QString prev = predict_detail::normWord(query.sentenceWords.last());
        const QVector<QString> left = query.sentenceWords.mid(0, query.sentenceWords.size() - 1);
        const QVector<predict_detail::Scored> ranked = rankTyped(*d, prev, packWords(*d, left), false);
        if (!ranked.isEmpty()) {
            const QString word = predict_detail::wordText(*d, ranked[0].id);
            if (!word.isEmpty() && word != prev) {
                Hit h;
                h.word = word;
                h.replacesPrevious = true;
                h.proper = ranked[0].id >= 0 && ranked[0].id < d->words.size()
                           && d->words[ranked[0].id].proper;
                hits.push_back(h);
            }
        }
    }

    QHash<int, float> edits = predict_detail::nextEdits(*d, here.ctx, here.anchors, here.cache);
    if (!hits.isEmpty()) {
        QHash<int, float> filtered;
        for (auto it = edits.cbegin(); it != edits.cend(); ++it) {
            if (predict_detail::wordText(*d, it.key()) != hits[0].word) {
                filtered.insert(it.key(), 0.f);
            }
        }
        edits = filtered;
    }
    appendRanked(predict_detail::rankEdits(*d, here.ctx, here.cache, here.cacheN, edits, here.anchors));
    return hits;
}

} // namespace gazer
