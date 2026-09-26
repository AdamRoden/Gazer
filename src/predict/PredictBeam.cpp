#include "predict/PredictInternal.h"

#include <cmath>
#include <queue>

namespace gazer {
namespace predict_detail {
namespace {

struct KeyPos {
    float x = 0;
    float y = 0;
    bool ok = false;
};

KeyPos keyPos(QChar ch)
{
    ch = ch.toLower();
    if (ch.unicode() < 'a' || ch.unicode() > 'z') {
        return {};
    }
    static const KeyPos kPos[26] = {
        {1.8f, 1.f, true},  {7.5f, 2.f, true},  {4.1f, 2.f, true},  {3.8f, 1.f, true},
        {3.5f, 0.f, true},  {4.8f, 1.f, true},  {5.8f, 1.f, true},  {6.8f, 1.f, true},
        {8.5f, 0.f, true},  {7.8f, 1.f, true},  {8.8f, 1.f, true},  {9.8f, 1.f, true},
        {9.5f, 2.f, true},  {8.5f, 2.f, true},  {9.5f, 0.f, true},  {10.5f, 0.f, true},
        {1.5f, 0.f, true},  {4.5f, 0.f, true},  {2.8f, 1.f, true},  {5.5f, 0.f, true},
        {7.5f, 0.f, true},  {5.1f, 2.f, true},  {2.5f, 0.f, true},  {3.15f, 2.f, true},
        {6.5f, 0.f, true},  {2.25f, 2.f, true},
    };
    return kPos[ch.unicode() - 'a'];
}

float subCost(QChar a, QChar b, const WordPredictor::Weights& w)
{
    if (a.toLower() == b.toLower()) {
        return 0.f;
    }
    const KeyPos pa = keyPos(a);
    const KeyPos pb = keyPos(b);
    if (!pa.ok || !pb.ok) {
        return float(w.costSubOther);
    }
    const float d = std::hypot(pa.x - pb.x, pa.y - pb.y);
    if (d <= 1.15f) {
        return float(w.costSubNeighbor);
    }
    if (d <= 1.75f) {
        return float(w.costSubDiag);
    }
    return float(w.costSubOther);
}

int edgeTo(const PredictModel& m, int node, ushort ch)
{
    if (node < 0 || node >= m.nodes.size()) {
        return -1;
    }
    const TrieNode& n = m.nodes[node];
    int lo = 0;
    int hi = n.edgeCount;
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        const ushort c = m.edges[n.edgeStart + mid].ch;
        if (c < ch) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if (lo < n.edgeCount && m.edges[n.edgeStart + lo].ch == ch) {
        return m.edges[n.edgeStart + lo].to;
    }
    return -1;
}

void recordWord(QHash<int, float>& found, int wordId, float cost, int& budget)
{
    if (wordId < 0 || budget <= 0) {
        return;
    }
    const auto it = found.constFind(wordId);
    if (it == found.cend()) {
        found.insert(wordId, cost);
        --budget;
        return;
    }
    if (cost < it.value()) {
        found.insert(wordId, cost);
    }
}

void completeFrom(const PredictModel& m, int start, float cost, QHash<int, float>& found, int& budget)
{
    struct NodePri {
        float uni = 0;
        int node = 0;
        int extra = 0;
        bool operator<(const NodePri& o) const { return uni < o.uni; }
    };
    if (start < 0 || start >= m.nodes.size() || budget <= 0) {
        return;
    }
    std::priority_queue<NodePri> pq;
    const int best = m.nodes[start].bestWord;
    const float uni = best >= 0 ? m.words[best].uni : 0.f;
    pq.push({uni, start, 0});
    QHash<int, char> seen;
    int guard = 0;
    while (!pq.empty() && budget > 0 && guard++ < 500) {
        const NodePri cur = pq.top();
        pq.pop();
        if (seen.contains(cur.node)) {
            continue;
        }
        seen.insert(cur.node, 1);
        const TrieNode& n = m.nodes[cur.node];
        if (n.wordId >= 0) {
            recordWord(found, n.wordId, cost, budget);
        }
        if (cur.extra >= 18) {
            continue;
        }
        for (int i = 0; i < n.edgeCount; ++i) {
            const TrieEdge& e = m.edges[n.edgeStart + i];
            if (seen.contains(e.to)) {
                continue;
            }
            float childUni = 0.f;
            const int bw = m.nodes[e.to].bestWord;
            if (bw >= 0) {
                childUni = m.words[bw].uni;
            }
            pq.push({childUni, e.to, cur.extra + 1});
        }
    }
}

QString phoneticKey(const QString& word)
{
    QString s;
    s.reserve(word.size());
    for (QChar c : word) {
        if (c.isLetter()) {
            s.append(c.toLower());
        }
    }
    if (s.isEmpty()) {
        return {};
    }
    QString out;
    const auto at = [&](int i) { return i < s.size() ? s[i] : QChar(); };
    for (int i = 0; i < s.size();) {
        const QChar c = s[i];
        const QChar n = at(i + 1);
        QChar put;
        int step = 1;
        if (c == QLatin1Char('p') && n == QLatin1Char('h')) {
            put = QLatin1Char('f');
            step = 2;
        } else if (c == QLatin1Char('k') && n == QLatin1Char('n')) {
            put = QLatin1Char('n');
            step = 2;
        } else if (c == QLatin1Char('w') && n == QLatin1Char('r')) {
            put = QLatin1Char('r');
            step = 2;
        } else if (c == QLatin1Char('c')
                   && (n == QLatin1Char('e') || n == QLatin1Char('i') || n == QLatin1Char('y'))) {
            put = QLatin1Char('s');
        } else if (c == QLatin1Char('c') || c == QLatin1Char('q')) {
            put = QLatin1Char('k');
        } else if (c == QLatin1Char('x')) {
            put = QLatin1Char('s');
        } else if (QStringLiteral("aeiou").contains(c)) {
            if (out.isEmpty()) {
                put = c;
            }
        } else {
            put = c;
        }
        if (!put.isNull() && (out.isEmpty() || out.back() != put)) {
            out.append(put);
        }
        i += step;
    }
    return out;
}

} // namespace

QHash<int, float> beamEdits(const PredictModel& m, const QString& typed)
{
    QHash<int, float> found;
    if (m.nodes.isEmpty() || typed.isEmpty() || typed.size() > 32) {
        return found;
    }
    const WordPredictor::Weights& w = m.w;
    struct Item {
        float cost = 0;
        int node = 0;
        int pos = 0;
        bool operator>(const Item& o) const { return cost > o.cost; }
    };
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> pq;
    QHash<quint64, float> best;
    int budget = 80;
    const auto push = [&](int node, int pos, float cost) {
        if (node < 0 || !(cost <= float(w.costLimit)) || pos < 0 || pos > typed.size()) {
            return;
        }
        const quint64 key = (quint64(uint(node)) << 16) | quint64(uint(pos));
        const auto it = best.constFind(key);
        if (it != best.cend() && it.value() <= cost + 1e-5f) {
            return;
        }
        best.insert(key, cost);
        pq.push({cost, node, pos});
    };
    push(0, 0, 0.f);
    int expanded = 0;
    while (!pq.empty() && expanded < 6000) {
        const Item it = pq.top();
        pq.pop();
        const quint64 key = (quint64(uint(it.node)) << 16) | quint64(uint(it.pos));
        const auto seen = best.constFind(key);
        if (seen == best.cend() || seen.value() + 1e-5f < it.cost) {
            continue;
        }
        ++expanded;
        const TrieNode& node = m.nodes[it.node];
        if (it.pos == typed.size()) {
            completeFrom(m, it.node, it.cost, found, budget);
            continue;
        }
        const QChar cur = typed[it.pos];
        const float del = (it.pos > 0 && typed[it.pos] == typed[it.pos - 1])
                              ? float(w.costRepeat)
                              : float(w.costInsertDelete);
        push(it.node, it.pos + 1, it.cost + del);
        if (it.pos + 1 < typed.size() && typed[it.pos] != typed[it.pos + 1]) {
            const int mid = edgeTo(m, it.node, typed[it.pos + 1].unicode());
            if (mid >= 0) {
                const int fin = edgeTo(m, mid, typed[it.pos].unicode());
                if (fin >= 0) {
                    push(fin, it.pos + 2, it.cost + float(w.costTranspose));
                }
            }
        }
        for (int i = 0; i < node.edgeCount; ++i) {
            const TrieEdge& e = m.edges[node.edgeStart + i];
            const QChar letter(e.ch);
            float step = it.cost;
            if (letter.toLower() == cur.toLower()) {
                step += 0.f;
            } else {
                step += subCost(cur, letter, w);
            }
            push(e.to, it.pos + 1, step);
            const bool repeatIns = it.pos > 0 && typed[it.pos - 1].unicode() == e.ch;
            const float ins = it.cost + (repeatIns ? float(w.costRepeat) : float(w.costInsertDelete));
            push(e.to, it.pos, ins);
        }
    }
    return found;
}

void addPhonetic(const PredictModel& m, const QString& typed, QHash<int, float>& edits)
{
    if (typed.size() < 4) {
        return;
    }
    const QString key = phoneticKey(typed);
    const auto it = m.phonetic.constFind(key);
    if (it == m.phonetic.cend()) {
        return;
    }
    const float cost = float(m.w.costLimit);
    int added = 0;
    for (int id : it.value()) {
        if (added >= 8) {
            break;
        }
        if (!edits.contains(id)) {
            edits.insert(id, cost);
            ++added;
        }
    }
}

void rebuildPhonetic(PredictModel& m)
{
    m.phonetic.clear();
    for (int i = 0; i < m.words.size(); ++i) {
        const QString key = phoneticKey(m.words[i].text);
        if (key.size() < 2) {
            continue;
        }
        m.phonetic[key].push_back(i);
    }
}

} // namespace predict_detail
} // namespace gazer
