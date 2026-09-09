#include "assist/ComposeBuffer.h"

#include <QDateTime>
#include <QRegularExpression>
#include <algorithm>

namespace gazer {
namespace {

const QRegularExpression& tokenRe()
{
    static const QRegularExpression re(QStringLiteral(R"(\S+)"));
    return re;
}

int clampCaret(int caret, int len)
{
    return std::clamp(caret, 0, len);
}

} // namespace

qint64 ComposeBuffer::nowMs() const
{
    return m_nowOverride ? *m_nowOverride : QDateTime::currentMSecsSinceEpoch();
}

void ComposeBuffer::pushUndo()
{
    if (!m_undo.isEmpty()) {
        const Snapshot& last = m_undo.last();
        if (last.text == m_text && last.caret == m_caret) {
            return;
        }
    }
    m_undo.push_back(Snapshot{m_text, m_caret});
    if (m_undo.size() > kUndoLimit) {
        m_undo.removeFirst();
    }
    m_redo.clear();
}

void ComposeBuffer::apply(const QString& text, int caret, Kind kind, bool coalesce)
{
    if (!coalesce) {
        pushUndo();
    }
    m_text = text;
    m_caret = clampCaret(caret, m_text.size());
    m_lastKind = kind;
    m_lastAt = nowMs();
}

QString ComposeBuffer::padAgainstNeighbors(QStringView text, int start, int end, QStringView insert)
{
    QString piece(insert);
    if (piece.isEmpty()) {
        return piece;
    }
    const QChar before = start > 0 ? text[start - 1] : QChar();
    const QChar after = end < text.size() ? text[end] : QChar();
    if (!before.isNull() && !before.isSpace() && !piece.front().isSpace()) {
        piece.prepend(QLatin1Char(' '));
    }
    if (!after.isNull() && !after.isSpace() && !piece.back().isSpace()) {
        piece.append(QLatin1Char(' '));
    }
    return piece;
}

void ComposeBuffer::insert(QStringView chars)
{
    if (chars.isEmpty()) {
        return;
    }
    const qint64 t = nowMs();
    const bool coalesce = m_lastKind == Kind::Insert && (t - m_lastAt) < kCoalesceMs;
    QString next = m_text;
    next.insert(m_caret, chars);
    apply(next, m_caret + int(chars.size()), Kind::Insert, coalesce);
}

void ComposeBuffer::insertPadded(QStringView chunk)
{
    if (chunk.isEmpty()) {
        return;
    }
    const int start = m_caret;
    const int end = m_caret;
    const QString padded = padAgainstNeighbors(m_text, start, end, chunk);
    QString next = m_text;
    next.insert(start, padded);
    apply(next, start + padded.size(), Kind::Other, false);
}

void ComposeBuffer::insertTag(QStringView tag)
{
    QString t = tag.toString().trimmed();
    if (t.isEmpty()) {
        return;
    }
    if (!(t.startsWith(QLatin1Char('[')) && t.endsWith(QLatin1Char(']')))) {
        t = QLatin1Char('[') + t + QLatin1Char(']');
    }
    insertPadded(t);
}

void ComposeBuffer::backspace()
{
    if (m_caret <= 0) {
        return;
    }
    const qint64 t = nowMs();
    const bool coalesce = m_lastKind == Kind::Delete && (t - m_lastAt) < kCoalesceMs;
    QString next = m_text;
    next.remove(m_caret - 1, 1);
    apply(next, m_caret - 1, Kind::Delete, coalesce);
}

void ComposeBuffer::deleteWord()
{
    const int start = m_caret;
    const int end = m_caret;
    if (start != end) {
        QString next = m_text;
        next.remove(start, end - start);
        apply(next, start, Kind::Other, false);
        return;
    }
    if (start <= 0) {
        return;
    }
    int i = start;
    while (i > 0 && m_text[i - 1].isSpace()) {
        --i;
    }
    while (i > 0 && !m_text[i - 1].isSpace()) {
        --i;
    }
    QString next = m_text;
    next.remove(i, start - i);
    apply(next, i, Kind::Other, false);
}

void ComposeBuffer::removeTokenAt(int index)
{
    const QVector<Token> toks = tokens();
    if (index < 0 || index >= toks.size()) {
        return;
    }
    const Token& t = toks[index];
    int start = t.start;
    int end = t.end;
    if (end < m_text.size() && m_text[end].isSpace()) {
        ++end;
    } else if (start > 0 && m_text[start - 1].isSpace()) {
        --start;
    }
    QString next = m_text;
    next.remove(start, end - start);
    apply(next, std::min(start, int(next.size())), Kind::Other, false);
}

void ComposeBuffer::removeVisibleWord(int slot)
{
    const int index = tokenIndexForVisibleSlot(slot);
    if (index < 0) {
        return;
    }
    removeTokenAt(index);
}

void ComposeBuffer::clear()
{
    if (m_text.isEmpty() && m_caret == 0) {
        return;
    }
    apply(QString(), 0, Kind::Other, false);
}

void ComposeBuffer::load(const QString& text)
{
    m_text = text;
    m_caret = text.size();
    resetHistory();
}

void ComposeBuffer::setCaret(int pos)
{
    m_caret = clampCaret(pos, m_text.size());
}

void ComposeBuffer::moveCaretToTokenEdge(int index, bool atStart)
{
    const QVector<Token> toks = tokens();
    if (index < 0 || index >= toks.size()) {
        return;
    }
    setCaret(atStart ? toks[index].start : toks[index].end);
}

void ComposeBuffer::moveCaretToVisibleWordEdge(int slot, bool atStart)
{
    moveCaretToTokenEdge(tokenIndexForVisibleSlot(slot), atStart);
}

bool ComposeBuffer::undo()
{
    if (m_undo.isEmpty()) {
        return false;
    }
    const Snapshot cur{m_text, m_caret};
    const Snapshot prev = m_undo.takeLast();
    m_redo.push_back(cur);
    m_text = prev.text;
    m_caret = clampCaret(prev.caret, m_text.size());
    m_lastKind = Kind::None;
    return true;
}

bool ComposeBuffer::redo()
{
    if (m_redo.isEmpty()) {
        return false;
    }
    const Snapshot cur{m_text, m_caret};
    const Snapshot next = m_redo.takeLast();
    m_undo.push_back(cur);
    if (m_undo.size() > kUndoLimit) {
        m_undo.removeFirst();
    }
    m_text = next.text;
    m_caret = clampCaret(next.caret, m_text.size());
    m_lastKind = Kind::None;
    return true;
}

void ComposeBuffer::resetHistory()
{
    m_undo.clear();
    m_redo.clear();
    m_lastKind = Kind::None;
}

QVector<ComposeBuffer::Token> ComposeBuffer::tokens() const
{
    QVector<Token> out;
    QRegularExpressionMatchIterator it = tokenRe().globalMatch(m_text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        Token t;
        t.text = m.captured();
        t.start = int(m.capturedStart());
        t.end = int(m.capturedEnd());
        out.push_back(t);
    }
    return out;
}

QVector<ComposeBuffer::Token> ComposeBuffer::visibleTokens() const
{
    const QVector<Token> all = tokens();
    if (all.size() <= kVisibleChips) {
        return all;
    }
    return all.mid(all.size() - kVisibleChips);
}

int ComposeBuffer::tokenIndexForVisibleSlot(int slot) const
{
    const int n = tokens().size();
    if (n == 0) {
        return -1;
    }
    const int window = std::min(kVisibleChips, n);
    if (slot < 0 || slot >= window) {
        return -1;
    }
    return n - window + slot;
}

} // namespace gazer
