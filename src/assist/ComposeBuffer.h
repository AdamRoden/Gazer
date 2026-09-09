#pragma once

#include <QString>
#include <QStringView>
#include <QVector>
#include <optional>

namespace gazer {

/// Internal composer phrase: caret, chips, undo. No UI, no network.
class ComposeBuffer {
public:
    static constexpr int kUndoLimit = 80;
    static constexpr int kCoalesceMs = 800;
    static constexpr int kVisibleChips = 12;

    struct Token {
        QString text;
        int start = 0;
        int end = 0;
    };

    [[nodiscard]] const QString& text() const { return m_text; }
    [[nodiscard]] int caret() const { return m_caret; }
    [[nodiscard]] bool isEmpty() const { return m_text.isEmpty(); }

    /// Keyboard / unpadded insert at caret. Rapid inserts within 800 ms coalesce.
    void insert(QStringView chars);
    /// Word/tag insert: pad spaces against non-space neighbors. One undo step.
    void insertPadded(QStringView chunk);
    /// Insert `[tag]` with padding. `laugh` and `[laugh]` both work.
    void insertTag(QStringView tag);
    /// Delete one character before caret. Rapid backspaces coalesce.
    void backspace();
    /// Delete the selection, or the whole word before caret (Voice deleteWholeWord).
    void deleteWord();
    /// Remove visible chip `slot` (0..11 = last 12 tokens, not the full list).
    void removeVisibleWord(int slot);
    void clear();
    /// Replace the phrase and caret, and drop undo/redo. Used when the composer
    /// is borrowed as a name field.
    void load(const QString& text);
    /// Move the caret without an undo step.
    void setCaret(int pos);
    /// Place the caret at the start (`true`) or end of token `index`.
    void moveCaretToTokenEdge(int index, bool atStart);
    void moveCaretToVisibleWordEdge(int slot, bool atStart);

    bool undo();
    bool redo();
    [[nodiscard]] bool canUndo() const { return !m_undo.isEmpty(); }
    [[nodiscard]] bool canRedo() const { return !m_redo.isEmpty(); }
    void resetHistory();

    [[nodiscard]] QVector<Token> tokens() const;
    [[nodiscard]] QVector<Token> visibleTokens() const;
    /// Full-token index for a visible slot, or -1.
    [[nodiscard]] int tokenIndexForVisibleSlot(int slot) const;

    /// Test hook: freeze the coalesce clock. nullopt = wall clock.
    void setNowMsForTest(std::optional<qint64> ms) { m_nowOverride = ms; }

private:
    enum class Kind { None, Insert, Delete, Other };

    struct Snapshot {
        QString text;
        int caret = 0;
    };

    [[nodiscard]] qint64 nowMs() const;
    void pushUndo();
    void apply(const QString& text, int caret, Kind kind, bool coalesce);
    static QString padAgainstNeighbors(QStringView text, int start, int end, QStringView insert);
    void removeTokenAt(int index);

    QString m_text;
    int m_caret = 0;
    QVector<Snapshot> m_undo;
    QVector<Snapshot> m_redo;
    Kind m_lastKind = Kind::None;
    qint64 m_lastAt = 0;
    std::optional<qint64> m_nowOverride;
};

} // namespace gazer
