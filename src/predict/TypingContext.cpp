#include "predict/TypingContext.h"

#include "predict/PredictInternal.h"
#include "predict/WordPredictor.h"

#include <algorithm>

namespace gazer {
namespace {

constexpr int kUndoCap = 32;
constexpr int kIdleMs = 2000;

bool isCaretMove(const QString& command)
{
    return command == QLatin1String("delete") || command == QLatin1String("tab")
           || command == QLatin1String("escape") || command == QLatin1String("home")
           || command == QLatin1String("end") || command == QLatin1String("pageup")
           || command == QLatin1String("pagedown") || command == QLatin1String("arrowup")
           || command == QLatin1String("arrowdown") || command == QLatin1String("arrowleft")
           || command == QLatin1String("arrowright");
}

} // namespace

TypingContext::TypingContext(WordPredictor* words, QObject* parent)
    : QObject(parent)
    , m_words(words)
{
    m_idle.setSingleShot(true);
    m_idle.setInterval(kIdleMs);
    connect(&m_idle, &QTimer::timeout, this, [this]() { forgetContext(); });
}

TypingContext::~TypingContext()
{
    saveUsers();
}

void TypingContext::setListener(std::function<void(const QHash<QChar, double>&)> fn)
{
    m_listener = std::move(fn);
}

bool TypingContext::load(const QString& shippedPath, const QString& userCharPath,
                         const QString& userWordPath, QString* error)
{
    m_userCharPath = userCharPath;
    m_userWordPath = userWordPath;
    QString err;
    if (!m_prior.loadFile(shippedPath, &err)) {
        if (error) {
            *error = err;
        }
    }
    m_prior.loadUser(userCharPath);
    if (m_words && !userWordPath.isEmpty()) {
        m_words->loadUser(userWordPath);
    }
    publish();
    return err.isEmpty();
}

bool TypingContext::isComposePage(const QString& pageId)
{
    return pageId == QLatin1String("compose") || pageId.startsWith(QLatin1String("compose_"));
}

QString TypingContext::normalizePrefix(const QString& text)
{
    QString out;
    for (const QChar ch : text) {
        if (ch.isSpace()) {
            if (!out.isEmpty() && out.back() != QLatin1Char(' ')) {
                out.append(QLatin1Char(' '));
            }
            continue;
        }
        const QChar n = CharPrior::canonicalize(ch);
        if (!n.isNull()) {
            out.append(n);
        }
    }
    return out;
}

void TypingContext::noteKey(const QString& pageId, QChar symbol)
{
    if (isComposePage(pageId)) {
        return;
    }
    const QChar ch = CharPrior::canonicalize(symbol);
    if (ch.isNull()) {
        return;
    }
    m_stale = false;
    if (m_source == Source::Phrase) {
        m_context.clear();
        m_undo.clear();
        m_source = Source::Keys;
    }
    appendSymbol(ch, true);
    publish();
    armIdle();
}

void TypingContext::noteBackspace(const QString& pageId)
{
    if (isComposePage(pageId) || m_source == Source::Phrase || m_context.isEmpty()) {
        return;
    }
    m_stale = false;
    chopLearned();
    publish();
    armIdle();
}

void TypingContext::noteLineBreak()
{
    closeTokenEndingAt(m_context.size());
    m_context.clear();
    m_undo.clear();
    m_source = Source::Keys;
    m_stale = false;
    publish();
    armIdle();
}

void TypingContext::noteNamedKey(const QString& pageId, const QString& name, bool isSend)
{
    if (isComposePage(pageId)) {
        return;
    }
    const QString trimmed = name.trimmed();
    if (isSend && trimmed.size() == 1) {
        noteKey(pageId, trimmed.at(0));
        return;
    }
    const QString lower = trimmed.toLower();
    if (lower == QLatin1String("space")) {
        noteKey(pageId, QLatin1Char(' '));
    } else if (lower == QLatin1String("backspace")) {
        noteBackspace(pageId);
    } else if (lower == QLatin1String("enter") || (isSend && lower == QLatin1String("return"))) {
        noteLineBreak();
    } else if (isCaretMove(lower)) {
        resetContext();
    }
}

void TypingContext::noteSend(const QString& pageId, const QString& key, const QString& edge)
{
    if (!edge.trimmed().isEmpty()) {
        return;
    }
    noteNamedKey(pageId, key, true);
}

void TypingContext::noteCommand(const QString& pageId, const QString& command)
{
    noteNamedKey(pageId, command, false);
}

void TypingContext::syncPhrase(const QString& text, int caret)
{
    const int n = qBound(0, caret, text.size());
    const QString next = normalizePrefix(text.left(n));
    if (m_source != Source::Phrase) {
        if (!m_context.isEmpty()) {
            m_context = next;
            m_undo.clear();
            m_source = Source::Phrase;
            m_stale = false;
            publish();
            armIdle();
            return;
        }
        m_source = Source::Phrase;
    }
    if (next == m_context) {
        return;
    }
    m_stale = false;
    applyDiff(next);
    publish();
    armIdle();
}

void TypingContext::adoptPhrase(const QString& text, int caret)
{
    const int n = qBound(0, caret, text.size());
    const QString next = normalizePrefix(text.left(n));
    if (m_source == Source::Phrase && next == m_context) {
        if (!m_stale) {
            return;
        }
        m_stale = false;
        publish();
        return;
    }
    if (m_source == Source::Phrase && next.startsWith(m_context)) {
        m_stale = false;
        for (int i = m_context.size(); i < next.size(); ++i) {
            appendSymbol(next.at(i), false);
        }
        publish();
        armIdle();
        return;
    }
    m_context = next;
    m_undo.clear();
    m_source = Source::Phrase;
    m_stale = false;
    publish();
    armIdle();
}

void TypingContext::noteAcceptedWord(const QStringList& left, const QString& word)
{
    if (!m_words || word.isEmpty()) {
        return;
    }
    m_words->observe(left, word);
    m_wordDirty = true;
    saveUsers();
}

void TypingContext::resetContext()
{
    m_context.clear();
    m_undo.clear();
    m_source = Source::Keys;
    m_stale = false;
    publish();
}

void TypingContext::forgetContext()
{
    if (m_stale) {
        return;
    }
    m_stale = true;
    publish();
}

void TypingContext::appendSymbol(QChar symbol, bool learn)
{
    const QChar ch = CharPrior::canonicalize(symbol);
    if (ch.isNull()) {
        return;
    }
    ushort prev2 = 0;
    ushort prev1 = 0;
    if (!m_context.isEmpty()) {
        prev1 = m_context.back().unicode();
        if (m_context.size() >= 2) {
            prev2 = m_context.at(m_context.size() - 2).unicode();
        }
    }
    if (learn) {
        const QChar p2 = prev2 == 0 ? QChar() : QChar(prev2);
        const QChar p1 = prev1 == 0 ? QChar() : QChar(prev1);
        m_prior.observe(p2, p1, ch);
        m_undo.push_back(Undo{prev2, prev1, ch.unicode()});
        if (m_undo.size() > kUndoCap) {
            m_undo.removeFirst();
        }
        m_charDirty = true;
    }
    m_context.append(ch);
    if (m_source == Source::Keys && m_context.size() > 64) {
        m_context.remove(0, m_context.size() - 64);
    }
    if (m_source == Source::Keys && learn && !predict_detail::isWordChar(ch)) {
        closeTokenEndingAt(m_context.size() - 1);
    }
}

void TypingContext::chopLearned()
{
    if (m_context.isEmpty()) {
        return;
    }
    const QChar ch = m_context.back();
    m_context.chop(1);
    if (m_undo.isEmpty() || m_undo.back().next != ch.unicode()) {
        return;
    }
    const Undo u = m_undo.back();
    m_undo.removeLast();
    const QChar p2 = u.prev2 == 0 ? QChar() : QChar(u.prev2);
    const QChar p1 = u.prev1 == 0 ? QChar() : QChar(u.prev1);
    if (m_prior.undo(p2, p1, ch)) {
        m_charDirty = true;
    }
}

void TypingContext::closeTokenEndingAt(int endExclusive)
{
    if (m_source != Source::Keys || !m_words || endExclusive <= 0 || endExclusive > m_context.size()) {
        return;
    }
    int start = endExclusive;
    while (start > 0 && predict_detail::isWordChar(m_context.at(start - 1))) {
        --start;
    }
    if (endExclusive - start < 2) {
        return;
    }
    const QString word = m_context.mid(start, endExclusive - start);
    QStringList left;
    int i = start - 1;
    while (i >= 0 && !predict_detail::isWordChar(m_context.at(i))) {
        --i;
    }
    const int prevEnd = i + 1;
    while (i >= 0 && predict_detail::isWordChar(m_context.at(i))) {
        --i;
    }
    if (prevEnd - (i + 1) >= 2) {
        left.push_back(m_context.mid(i + 1, prevEnd - (i + 1)));
    }
    m_words->observe(left, word);
    m_wordDirty = true;
    saveUsers();
}

void TypingContext::applyDiff(const QString& next)
{
    int keep = 0;
    const int nmax = std::min(m_context.size(), next.size());
    while (keep < nmax && m_context.at(keep) == next.at(keep)) {
        ++keep;
    }
    while (m_context.size() > keep) {
        chopLearned();
    }
    for (int i = keep; i < next.size(); ++i) {
        appendSymbol(next.at(i), true);
    }
}

void TypingContext::saveUsers()
{
    if (m_charDirty && !m_userCharPath.isEmpty()) {
        QString err;
        if (m_prior.saveUser(m_userCharPath, &err)) {
            m_charDirty = false;
        }
    }
    if (m_wordDirty && m_words && !m_userWordPath.isEmpty()) {
        if (m_words->saveUser(m_userWordPath)) {
            m_wordDirty = false;
        }
    }
}

QHash<QChar, double> TypingContext::distribution() const
{
    QChar prev2;
    QChar prev1;
    QString prefix;
    QStringList words;
    if (!m_stale && !m_context.isEmpty()) {
        prev1 = m_context.back();
        if (m_context.size() >= 2) {
            prev2 = m_context.at(m_context.size() - 2);
        }
        const QStringList parts = m_context.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (!parts.isEmpty()) {
            const bool open = !m_context.endsWith(QLatin1Char(' '));
            if (open) {
                prefix = parts.last();
                for (int i = 0; i < parts.size() - 1; ++i) {
                    words.push_back(parts.at(i));
                }
            } else {
                words = parts;
            }
        }
    }
    QHash<QChar, double> chars;
    double charSum = 0;
    const QVector<CharPrior::Mass> rows = (m_stale || m_context.isEmpty())
                                              ? m_prior.unigram()
                                              : m_prior.distribution(prev2, prev1);
    for (const CharPrior::Mass& row : rows) {
        chars.insert(row.symbol, row.mass);
        charSum += row.mass;
    }
    QHash<QChar, double> dict;
    double dictSum = 0;
    if (m_words) {
        WordPredictor::Query query;
        query.typed = prefix;
        for (const QString& w : words) {
            query.sentenceWords.push_back(w);
        }
        for (const WordPredictor::NextChar& row : m_words->nextCharMass(query)) {
            dict[row.symbol] += row.mass;
            dictSum += row.mass;
        }
    }
    const double dictWeight = dictSum > 0 ? 0.55 : 0;
    const double charWeight = 1.0 - dictWeight;
    QHash<QChar, double> mixed;
    if (charSum > 0 && charWeight > 0) {
        for (auto it = chars.cbegin(); it != chars.cend(); ++it) {
            mixed[it.key()] += charWeight * (it.value() / charSum);
        }
    }
    if (dictSum > 0 && dictWeight > 0) {
        for (auto it = dict.cbegin(); it != dict.cend(); ++it) {
            mixed[it.key()] += dictWeight * (it.value() / dictSum);
        }
    }
    return mixed;
}

void TypingContext::publish()
{
    if (m_listener) {
        m_listener(distribution());
    }
}

void TypingContext::armIdle()
{
    m_idle.start();
}

} // namespace gazer
