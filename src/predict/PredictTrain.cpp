#include "predict/PredictInternal.h"

#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <utility>

namespace gazer {
namespace {

struct Counted {
    QString text;
    int count = 0;
};

QStringList splitSentences(const QStringList& lines)
{
    QStringList out;
    for (const QString& line : lines) {
        QString cur;
        for (const QChar ch : line) {
            cur.append(ch);
            if (ch == QLatin1Char('.') || ch == QLatin1Char('?') || ch == QLatin1Char('!')) {
                if (!cur.trimmed().isEmpty()) {
                    out.push_back(cur);
                }
                cur.clear();
            }
        }
        if (!cur.trimmed().isEmpty()) {
            out.push_back(cur);
        }
    }
    return out;
}

struct Tok {
    QString norm;
    bool capital = false;
    bool sentenceInitial = false;
};

QVector<Tok> tokenizeSentence(const QString& sentence)
{
    QVector<Tok> toks;
    const QStringList parts = sentence.split(QRegularExpression(QStringLiteral("\\s+")),
                                             Qt::SkipEmptyParts);
    bool first = true;
    for (const QString& part : parts) {
        Tok t;
        t.norm = predict_detail::normWord(part);
        t.sentenceInitial = first;
        first = false;
        if (t.norm.isEmpty()) {
            continue;
        }
        const QChar c = part.trimmed().front();
        t.capital = c.isLetter() && c.isUpper();
        toks.push_back(t);
    }
    return toks;
}

QVector<PredictCont> discounted(const QHash<int, int>& counts, int ctx, int cap)
{
    QVector<PredictCont> rows;
    if (ctx <= 0) {
        return rows;
    }
    for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
        const float p = std::max(0.f, float(it.value()) - 0.5f) / float(ctx);
        if (p < 0.001f) {
            continue;
        }
        PredictCont c;
        c.word = it.key();
        c.p = p;
        rows.push_back(c);
    }
    std::sort(rows.begin(), rows.end(), [](const PredictCont& a, const PredictCont& b) {
        if (a.p != b.p) {
            return a.p > b.p;
        }
        return a.word < b.word;
    });
    if (rows.size() > cap) {
        rows.resize(cap);
    }
    std::sort(rows.begin(), rows.end(), [](const PredictCont& a, const PredictCont& b) {
        return a.word < b.word;
    });
    return rows;
}

} // namespace

bool WordPredictor::buildFromSentences(const QStringList& lines, QString* error)
{
    return buildFromSentences(lines, {}, error);
}

bool WordPredictor::buildFromSentences(const QStringList& lines, const QVector<UsageCount>& usage,
                                       QString* error)
{
    auto fail = [&](const QString& msg) {
        if (error) {
            *error = msg;
        }
        return false;
    };
    QHash<QString, int> uni;
    QHash<QString, int> bi;
    QHash<QString, int> tri;
    QHash<QString, int> biCtx;
    QHash<QString, int> triCtx;
    QHash<QString, int> pairs;
    QHash<QString, int> docFreq;
    QHash<QString, int> midCap;
    QHash<QString, int> midLow;
    int sentences = 0;
    int tokens = 0;
    for (const QString& sentence : splitSentences(lines)) {
        const QVector<Tok> toks = tokenizeSentence(sentence);
        if (toks.isEmpty()) {
            continue;
        }
        ++sentences;
        QSet<QString> seenContent;
        QVector<QString> content;
        for (int i = 0; i < toks.size(); ++i) {
            const QString& w = toks[i].norm;
            uni[w] += 1;
            ++tokens;
            if (!toks[i].sentenceInitial) {
                if (toks[i].capital) {
                    midCap[w] += 1;
                } else {
                    midLow[w] += 1;
                }
            }
            if (i > 0) {
                const QString left = toks[i - 1].norm;
                const QString bk = left + QLatin1Char('\t') + w;
                bi[bk] += 1;
                biCtx[left] += 1;
            }
            if (i > 1) {
                const QString a = toks[i - 2].norm;
                const QString b = toks[i - 1].norm;
                const QString tk = a + QLatin1Char('\t') + b + QLatin1Char('\t') + w;
                tri[tk] += 1;
                triCtx[a + QLatin1Char('\t') + b] += 1;
            }
            if (!predict_detail::isClosedClass(w)) {
                content.push_back(w);
                seenContent.insert(w);
            }
        }
        for (const QString& w : seenContent) {
            docFreq[w] += 1;
        }
        for (int i = 0; i < content.size(); ++i) {
            for (int j = i + 1; j < content.size() && j <= i + 5; ++j) {
                QString a = content[i];
                QString b = content[j];
                if (a == b) {
                    continue;
                }
                if (b < a) {
                    std::swap(a, b);
                }
                pairs[a + QLatin1Char('\t') + b] += 1;
            }
        }
    }
    if (uni.isEmpty() || tokens <= 0) {
        return fail(QStringLiteral("No words in the training text"));
    }
    QHash<QString, int> dict;
    long long dictTotal = 0;
    for (const UsageCount& row : usage) {
        const QString w = predict_detail::normWord(row.word);
        if (w.size() < 2 || w.size() > 24 || row.count <= 0 || !w.front().isLetter()
            || !w.back().isLetter()) {
            continue;
        }
        bool ok = true;
        for (const QChar ch : w) {
            const bool letter = ch.isLetter() && ch.isLower();
            if (!letter && ch != QLatin1Char('\'') && ch != QLatin1Char('-')) {
                ok = false;
                break;
            }
        }
        if (!ok) {
            continue;
        }
        dict[w] += row.count;
    }
    for (auto it = dict.cbegin(); it != dict.cend(); ++it) {
        dictTotal += it.value();
    }
    const auto blended = [&](const QString& text, int sentCount) {
        double p = 0;
        if (sentCount > 0) {
            const double mass = dictTotal > 0 ? 0.55 : 1.0;
            p += mass * double(sentCount) / double(tokens);
        }
        if (dictTotal > 0) {
            const int dc = dict.value(text);
            if (dc > 0) {
                p += 0.45 * double(dc) / double(dictTotal);
            }
        }
        return float(p);
    };
    QVector<Counted> ranked;
    ranked.reserve(uni.size());
    for (auto it = uni.cbegin(); it != uni.cend(); ++it) {
        ranked.push_back({it.key(), it.value()});
    }
    std::sort(ranked.begin(), ranked.end(), [](const Counted& a, const Counted& b) {
        if (a.count != b.count) {
            return a.count > b.count;
        }
        return a.text < b.text;
    });
    PredictModel next;
    QHash<QString, int> idOf;
    constexpr int kDim = 48;
    next.embDim = kDim;
    for (int i = 0; i < ranked.size(); ++i) {
        LexWord w;
        w.text = ranked[i].text;
        w.uni = blended(ranked[i].text, ranked[i].count);
        const int caps = midCap.value(w.text);
        const int lows = midLow.value(w.text);
        w.proper = caps >= 2 && caps > lows && !predict_detail::isClosedClass(w.text);
        w.inSentences = true;
        idOf.insert(w.text, i);
        next.words.push_back(std::move(w));
    }
    constexpr int kMaxLexicon = 50000;
    QVector<Counted> extra;
    extra.reserve(dict.size());
    for (auto it = dict.cbegin(); it != dict.cend(); ++it) {
        if (!uni.contains(it.key())) {
            extra.push_back({it.key(), it.value()});
        }
    }
    std::sort(extra.begin(), extra.end(), [](const Counted& a, const Counted& b) {
        if (a.count != b.count) {
            return a.count > b.count;
        }
        return a.text < b.text;
    });
    const int room = std::max(0, kMaxLexicon - int(next.words.size()));
    if (int(extra.size()) > room) {
        extra.resize(room);
    }
    for (const Counted& row : extra) {
        LexWord w;
        w.text = row.text;
        w.uni = blended(row.text, 0);
        if (!(w.uni > 0.f)) {
            continue;
        }
        idOf.insert(w.text, next.words.size());
        next.words.push_back(std::move(w));
    }
    next.idOf = idOf;

    QHash<int, QHash<int, int>> biByLeft;
    for (auto it = bi.cbegin(); it != bi.cend(); ++it) {
        const int tab = it.key().indexOf(QLatin1Char('\t'));
        const int left = idOf.value(it.key().left(tab), -1);
        const int right = idOf.value(it.key().mid(tab + 1), -1);
        if (left < 0 || right < 0) {
            continue;
        }
        biByLeft[left].insert(right, it.value());
    }
    for (auto it = biByLeft.cbegin(); it != biByLeft.cend(); ++it) {
        const int ctx = biCtx.value(next.words[it.key()].text);
        QVector<PredictCont> rows = discounted(it.value(), ctx, 24);
        if (!rows.isEmpty()) {
            next.bigram.insert(it.key(), rows);
        }
    }
    QHash<quint64, QHash<int, int>> triByCtx;
    for (auto it = tri.cbegin(); it != tri.cend(); ++it) {
        const QStringList parts = it.key().split(QLatin1Char('\t'));
        if (parts.size() != 3) {
            continue;
        }
        const int a = idOf.value(parts[0], -1);
        const int b = idOf.value(parts[1], -1);
        const int c = idOf.value(parts[2], -1);
        if (a < 0 || b < 0 || c < 0) {
            continue;
        }
        triByCtx[predict_detail::triKey(a, b)].insert(c, it.value());
    }
    for (auto it = triByCtx.cbegin(); it != triByCtx.cend(); ++it) {
        const int a = int(quint32(it.key() >> 32));
        const int b = int(quint32(it.key()));
        const int ctx = triCtx.value(next.words[a].text + QLatin1Char('\t') + next.words[b].text);
        QVector<PredictCont> rows = discounted(it.value(), ctx, 24);
        if (!rows.isEmpty()) {
            next.trigram.insert(it.key(), rows);
        }
    }

    QHash<int, QVector<double>> acc;
    const auto addCooccur = [&](int id, int axis) {
        QVector<double>& row = acc[id];
        if (row.size() != kDim) {
            row = QVector<double>(kDim, 0.0);
        }
        row[axis] += 1.0;
    };
    for (const QString& sentence : splitSentences(lines)) {
        QVector<int> content;
        for (const Tok& t : tokenizeSentence(sentence)) {
            if (predict_detail::isClosedClass(t.norm)) {
                continue;
            }
            const int id = idOf.value(t.norm, -1);
            if (id >= 0 && next.words[id].inSentences) {
                content.push_back(id);
            }
        }
        for (int i = 0; i < content.size(); ++i) {
            for (int j = i + 1; j < content.size() && j <= i + 5; ++j) {
                const int bucket = int(qHash(next.words[content[j]].text) % uint(kDim));
                const int back = int(qHash(next.words[content[i]].text) % uint(kDim));
                addCooccur(content[i], bucket);
                addCooccur(content[j], back);
            }
        }
    }
    for (auto it = acc.begin(); it != acc.end(); ++it) {
        const QVector<double>& row = it.value();
        double norm = 0;
        for (double v : row) {
            norm += v * v;
        }
        norm = std::sqrt(norm);
        if (norm < 1e-6) {
            continue;
        }
        LexWord& w = next.words[it.key()];
        w.emb.resize(kDim);
        for (int axis = 0; axis < kDim; ++axis) {
            const int q = int(std::lround(row[axis] / norm * 100.0));
            w.emb[axis] = qint8(std::clamp(q, -127, 127));
        }
    }

    struct Bonus {
        int to = -1;
        float pmi = 0;
    };
    QHash<int, QVector<Bonus>> rawTrig;
    for (auto it = pairs.cbegin(); it != pairs.cend(); ++it) {
        if (it.value() < 2 || sentences <= 0) {
            continue;
        }
        const int tab = it.key().indexOf(QLatin1Char('\t'));
        const QString a = it.key().left(tab);
        const QString b = it.key().mid(tab + 1);
        const int ia = idOf.value(a, -1);
        const int ib = idOf.value(b, -1);
        const int da = docFreq.value(a);
        const int db = docFreq.value(b);
        if (ia < 0 || ib < 0 || da <= 0 || db <= 0) {
            continue;
        }
        const double pmi = std::log((double(it.value()) * double(sentences)) / (double(da) * double(db)));
        if (pmi <= 0.4) {
            continue;
        }
        const float bonus = float(std::min(pmi, 3.0));
        rawTrig[ia].push_back({ib, bonus});
        rawTrig[ib].push_back({ia, bonus});
    }
    for (auto it = rawTrig.begin(); it != rawTrig.end(); ++it) {
        QVector<Bonus>& rows = it.value();
        std::sort(rows.begin(), rows.end(), [](const Bonus& a, const Bonus& b) {
            return a.pmi > b.pmi;
        });
        if (rows.size() > 16) {
            rows.resize(16);
        }
        QVector<PredictCont> stored;
        for (const Bonus& b : rows) {
            PredictCont c;
            c.word = b.to;
            c.p = b.pmi;
            stored.push_back(c);
        }
        std::sort(stored.begin(), stored.end(), [](const PredictCont& a, const PredictCont& b) {
            return a.word < b.word;
        });
        next.triggers.insert(it.key(), stored);
    }

    predict_detail::rebuildTrie(next);
    predict_detail::rebuildPhonetic(next);
    predict_detail::rebuildTopUni(next);
    *d = std::move(next);
    return true;
}

} // namespace gazer
