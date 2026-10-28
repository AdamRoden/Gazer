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
    /// Physical chip buttons on the Speak row.
    static constexpr int kVisibleChips = 12;
    /// Word chips kept when the row pages. The first and last buttons are pagers.
    static constexpr int kChipWordSlots = 10;
    static constexpr int kChipPageStep = 5;

    enum class ChipSlotKind { Empty, Word, PageBack, PageForward };

    struct ChipSlot {
        ChipSlotKind kind = ChipSlotKind::Empty;
        int tokenIndex = -1;
    };

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
    /// Replace [start, end) in one undo step. Does not coalesce with letter inserts.
    void replaceRange(int start, int end, QStringView text);
    /// Insert `[tag]` with padding. `laugh` and `[laugh]` both work.
    void insertTag(QStringView tag);
    /// Delete one character before caret. Rapid backspaces coalesce.
    void backspace();
    /// Delete the selection, or the whole word before caret (Voice deleteWholeWord).
    void deleteWord();
    /// Remove the word on chip `slot`. Pager slots and empty slots do nothing.
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

    [[nodiscard]] bool hasSelection() const;
    /// Ordered selection, or -1 when there is none.
    [[nodiscard]] int selectionStart() const;
    [[nodiscard]] int selectionEnd() const;
    /// The selection only. Empty when nothing is selected.
    [[nodiscard]] QString selectedText() const;
    /// Select the whole phrase. Empty text is a no-op and does not push undo.
    void selectAll();
    void collapseSelection();

    [[nodiscard]] QVector<Token> tokens() const;
    /// Words in the chip window. Pager buttons are not included.
    [[nodiscard]] QVector<Token> visibleTokens() const;
    /// Full-token index for a word chip, or -1 for a pager or empty slot.
    [[nodiscard]] int tokenIndexForVisibleSlot(int slot) const;
    [[nodiscard]] bool chipsOverflow() const;
    [[nodiscard]] int chipWindowStart() const;
    [[nodiscard]] ChipSlot chipSlot(int slot) const;
    /// Move the word window by `delta` tokens (the chips use ±5).
    void pageChips(int delta);

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
    int m_selA = -1;
    int m_selB = -1;
    bool m_followTail = true;
    int m_windowStart = 0;
    QVector<Snapshot> m_undo;
    QVector<Snapshot> m_redo;
    Kind m_lastKind = Kind::None;
    qint64 m_lastAt = 0;
    std::optional<qint64> m_nowOverride;
};

} // namespace gazer
