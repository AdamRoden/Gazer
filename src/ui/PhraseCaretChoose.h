#pragma once

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>
#include <QtGlobal>

#include <cmath>

namespace gazer {

/// One run of letters and digits, in buffer indexes. Other characters split words.
struct PhraseCaretToken {
    int start = 0;
    int end = 0;
};

/// One wrapped line. `cursorX[i]` is the screen X of caret `start + i`.
struct PhraseCaretLine {
    int start = 0;
    int end = 0;
    QRectF rect;
    QVector<double> cursorX;
};

struct PhraseWordSpan {
    int start = 0;
    int end = 0;
    /// One rect per line. A wrap is not the bounding box of the pieces.
    QVector<QRectF> rects;
};

enum class PhraseCaretChoice { Left, Right, Home, End };

struct PhraseCaretButton {
    QPointF center;
};

struct PhraseCaretEdit {
    int caret = 0;
    int selStart = -1;
    int selEnd = -1;
    bool keepChooser = false;
    bool changed = false;
};

/// Look time before the word turns bold. Completing it does not start the dwell.
inline constexpr double kPhraseWordScanMs = 200.0;
/// Bold grow after the scan. A hit shorter than this does not select.
inline constexpr double kPhraseWordDwellMs = 600.0;
/// Green check after a successful copy, before the button hides.
inline constexpr double kPhraseCopyAckMs = 1000.0;
/// Matches the Speak `round` style stroke.
inline constexpr double kPhraseCopyBorderPx = 2.0;
/// Once a word is bold, its dwell band is this many line-heights tall.
inline constexpr int kPhraseWordDwellLines = 3;
/// The looked-at word grows by this fraction of its size over the dwell.
inline constexpr double kPhraseWordGrow = 0.5;

/// 1 at the start of a look, 1.5 when the dwell completes.
[[nodiscard]] inline double phraseWordGrowScale(double elapsedMs, double dwellMs)
{
    if (dwellMs <= 0.0 || elapsedMs <= 0.0) {
        return 1.0;
    }
    const double t = qBound(0.0, elapsedMs / dwellMs, 1.0);
    return 1.0 + kPhraseWordGrow * t;
}

struct PhraseLook {
    int id = -1;
    double elapsedMs = 0;
    bool latched = false;
};

struct PhraseLookStep {
    PhraseLook look;
    bool fired = false;
};

/// Advance a one-shot look. Leaving the target clears the latch so it can fire again.
[[nodiscard]] inline PhraseLookStep stepPhraseLook(PhraseLook look, int hitId, double dtMs,
                                                    double dwellMs)
{
    PhraseLookStep out;
    if (hitId < 0) {
        return out;
    }
    out.look.id = hitId;
    if (hitId == look.id && look.latched) {
        out.look.elapsedMs = look.elapsedMs;
        out.look.latched = true;
        return out;
    }
    const double carried = hitId == look.id ? look.elapsedMs : 0.0;
    out.look.elapsedMs = qMin(dwellMs, carried + qMax(0.0, dtMs));
    if (out.look.elapsedMs >= dwellMs) {
        out.look.latched = true;
        out.fired = true;
    }
    return out;
}

/// One-shot word look. Scan grace, then a single dwell. The word is bold only during the dwell.
struct PhraseWordLook {
    int id = -1;
    double elapsedMs = 0;
    bool scanning = true;
    bool latched = false;
};

struct PhraseWordStep {
    PhraseWordLook look;
    bool fired = false;
};

[[nodiscard]] inline PhraseWordStep stepPhraseWord(PhraseWordLook look, int hitId, double dtMs,
                                                   double scanMs, double dwellMs)
{
    PhraseWordStep out;
    if (hitId < 0) {
        return out;
    }
    if (hitId != look.id) {
        look = {};
        look.id = hitId;
    }
    out.look = look;
    if (out.look.latched) {
        return out;
    }
    out.look.elapsedMs += qMax(0.0, dtMs);
    if (out.look.scanning) {
        if (out.look.elapsedMs < qMax(0.0, scanMs)) {
            return out;
        }
        out.look.scanning = false;
        out.look.elapsedMs = 0.0;
        if (scanMs > 0.0) {
            return out;
        }
    }
    const double need = qMax(1.0, dwellMs);
    if (out.look.elapsedMs >= need) {
        out.fired = true;
        out.look.latched = true;
        out.look.elapsedMs = need;
        return out;
    }
    return out;
}

/// Arrow-key dwell. Scan grace, then the rapid sequence. The last step repeats.
struct PhraseKeyLook {
    int id = -1;
    int step = 0;
    double elapsedMs = 0;
    bool scanDone = false;
    double progress = 0;
};

struct PhraseKeyStep {
    PhraseKeyLook look;
    bool fired = false;
};

[[nodiscard]] inline PhraseKeyStep stepPhraseKey(PhraseKeyLook look, int hitId, double dtMs,
                                                 const QVector<int>& sequence, int scanGraceMs)
{
    PhraseKeyStep out;
    if (hitId < 0) {
        return out;
    }
    if (hitId != look.id) {
        look = {};
        look.id = hitId;
    }
    out.look = look;
    out.look.elapsedMs += qMax(0.0, dtMs);
    if (!out.look.scanDone) {
        if (out.look.elapsedMs < qMax(0, scanGraceMs)) {
            out.look.progress = 0.0;
            return out;
        }
        out.look.scanDone = true;
        out.look.elapsedMs = 0.0;
        out.look.progress = 0.0;
        if (scanGraceMs > 0) {
            return out;
        }
    }
    const QVector<int> fallback{400};
    const QVector<int>& seq = sequence.isEmpty() ? fallback : sequence;
    const int idx = qBound(0, out.look.step, seq.size() - 1);
    const int need = qMax(1, seq.at(idx));
    if (out.look.elapsedMs >= need) {
        out.fired = true;
        out.look.elapsedMs = 0.0;
        out.look.progress = 0.0;
        if (out.look.step + 1 < seq.size()) {
            ++out.look.step;
        }
        return out;
    }
    out.look.progress = out.look.elapsedMs / static_cast<double>(need);
    return out;
}

inline constexpr double kPhraseCaretButtonRadius = 28.0;
/// Larger than the painted circle. A word under the gaze wins over this disc.
inline constexpr double kPhraseCaretButtonHit = 40.0;

/// Selectable words. Punctuation, spaces, hyphens, and dots are gaps, not words.
[[nodiscard]] inline QVector<PhraseCaretToken> alphanumericCaretTokens(const QString& text)
{
    QVector<PhraseCaretToken> out;
    const int n = text.size();
    int i = 0;
    while (i < n) {
        if (!text.at(i).isLetterOrNumber()) {
            ++i;
            continue;
        }
        const int start = i;
        ++i;
        while (i < n && text.at(i).isLetterOrNumber()) {
            ++i;
        }
        out.push_back(PhraseCaretToken{start, i});
    }
    return out;
}

[[nodiscard]] inline QVector<PhraseWordSpan> phraseWordSpans(const QVector<PhraseCaretLine>& lines,
                                                             const QVector<PhraseCaretToken>& tokens)
{
    QVector<PhraseWordSpan> out;
    out.reserve(tokens.size());
    for (const PhraseCaretToken& token : tokens) {
        if (token.end <= token.start) {
            continue;
        }
        PhraseWordSpan span;
        span.start = token.start;
        span.end = token.end;
        for (const PhraseCaretLine& line : lines) {
            const int a = qMax(token.start, line.start);
            const int b = qMin(token.end, line.end);
            if (a >= b) {
                continue;
            }
            const int n = line.cursorX.size();
            const int i1 = a - line.start;
            const int i2 = b - line.start;
            if (i1 < 0 || i2 < 0 || i1 >= n || i2 >= n) {
                continue;
            }
            const double x1 = line.cursorX.at(i1);
            const double x2 = line.cursorX.at(i2);
            QRectF part(qMin(x1, x2), line.rect.top(), qMax(1.0, qAbs(x2 - x1)),
                        qMax(1.0, line.rect.height()));
            part = part.intersected(line.rect);
            if (part.isEmpty()) {
                continue;
            }
            span.rects.push_back(part);
        }
        if (!span.rects.isEmpty()) {
            out.push_back(span);
        }
    }
    return out;
}

/// A soft wrap shares this index with the next line. Pin the bar to the earlier line.
[[nodiscard]] inline int phraseBoundaryCaretLine(const QVector<PhraseCaretLine>& lines, int caret)
{
    for (int i = 0; i + 1 < lines.size(); ++i) {
        if (caret == lines.at(i).end && lines.at(i + 1).start == caret) {
            return i;
        }
    }
    return -1;
}

/// Glyph rect, or that rect grown to `kPhraseWordDwellLines` line-heights while the word is bold.
[[nodiscard]] inline QRectF phraseWordDwellRect(const QRectF& glyph, bool bold)
{
    if (!bold) {
        return glyph;
    }
    const double line = glyph.height();
    if (line <= 0.0) {
        return glyph;
    }
    const double extra = line * ((kPhraseWordDwellLines - 1) / 2.0);
    return glyph.adjusted(0.0, -extra, 0.0, extra);
}

/// Tight glyph hits win. `boldIndex` also accepts that word's taller dwell band.
[[nodiscard]] inline int phraseWordIndexAt(const QVector<PhraseWordSpan>& words, const QPointF& p,
                                           int boldIndex = -1)
{
    int best = -1;
    double bestArea = 0.0;
    for (int i = 0; i < words.size(); ++i) {
        for (const QRectF& rect : words.at(i).rects) {
            if (!rect.contains(p)) {
                continue;
            }
            const double area = rect.width() * rect.height();
            if (best < 0 || area < bestArea) {
                best = i;
                bestArea = area;
            }
        }
    }
    if (best >= 0 || boldIndex < 0 || boldIndex >= words.size()) {
        return best;
    }
    for (const QRectF& rect : words.at(boldIndex).rects) {
        if (phraseWordDwellRect(rect, true).contains(p)) {
            return boldIndex;
        }
    }
    return -1;
}

/// Copy, bottom edge on the top of `anchor`, clamped into the screen.
inline void layoutPhraseCopyButton(const QRectF& anchor, const QRectF& screen, PhraseCaretButton& out)
{
    const double r = kPhraseCaretButtonRadius;
    double midX = anchor.center().x();
    const double minX = screen.left() + 8.0 + r;
    const double maxX = screen.right() - 8.0 - r;
    if (minX <= maxX) {
        midX = qBound(minX, midX, maxX);
    }
    double cy = anchor.top() - r;
    const double minY = screen.top() + 8.0 + r;
    const double maxY = screen.bottom() - 8.0 - r;
    if (minY <= maxY) {
        cy = qBound(minY, cy, maxY);
    }
    out.center = QPointF(midX, cy);
}

[[nodiscard]] inline bool phraseCaretButtonAt(const PhraseCaretButton& button, const QPointF& p)
{
    return std::hypot(p.x() - button.center.x(), p.y() - button.center.y()) <= kPhraseCaretButtonHit;
}

/// Speak shift layers are 2 and 4. Arrows, Home, and End use this to extend the selection.
[[nodiscard]] inline bool phraseShiftSelects(bool shiftHeld, const QVector<int>& shownLayers)
{
    return shiftHeld || shownLayers.contains(2) || shownLayers.contains(4);
}

/// Without Shift, left and right move one character and clear any selection.
/// With Shift, they move the caret and extend or shrink the selection.
/// Home and End jump to that end. `length` is the buffer size.
[[nodiscard]] inline PhraseCaretEdit applyPhraseCaretChoice(int caret, int selStart, int selEnd,
                                                            PhraseCaretChoice choice, bool shift,
                                                            int length)
{
    PhraseCaretEdit out;
    out.caret = caret;
    out.selStart = selStart;
    out.selEnd = selEnd;
    if (choice == PhraseCaretChoice::Home || choice == PhraseCaretChoice::End) {
        const int n = qMax(0, length);
        const int dest = choice == PhraseCaretChoice::Home ? 0 : n;
        const bool hasSel = selStart >= 0 && selEnd > selStart;
        if (!shift) {
            out.caret = dest;
            out.selStart = -1;
            out.selEnd = -1;
            out.changed = caret != dest || hasSel;
            return out;
        }
        int a = hasSel ? selStart : caret;
        int b = hasSel ? selEnd : caret;
        if (choice == PhraseCaretChoice::Home) {
            a = 0;
        } else {
            b = n;
        }
        out.caret = dest;
        if (a >= b) {
            out.selStart = -1;
            out.selEnd = -1;
        } else {
            out.selStart = a;
            out.selEnd = b;
            out.keepChooser = true;
        }
        out.changed = out.caret != caret || out.selStart != selStart || out.selEnd != selEnd;
        return out;
    }
    const bool hasSel = selStart >= 0 && selEnd > selStart;
    const int dir = choice == PhraseCaretChoice::Left ? -1 : 1;
    const int n = qMax(0, length);
    if (!shift) {
        const int dest = qBound(0, caret + dir, n);
        out.caret = dest;
        out.selStart = -1;
        out.selEnd = -1;
        out.changed = caret != dest || hasSel;
        return out;
    }
    int anchor = caret;
    int focus = caret;
    if (hasSel) {
        anchor = caret <= selStart ? selEnd : selStart;
        focus = caret;
    }
    const int next = qBound(0, focus + dir, n);
    if (next == anchor) {
        out.caret = next;
        out.selStart = -1;
        out.selEnd = -1;
        out.changed = caret != next || hasSel;
        return out;
    }
    out.caret = next;
    out.selStart = qMin(anchor, next);
    out.selEnd = qMax(anchor, next);
    out.keepChooser = true;
    out.changed = out.caret != caret || out.selStart != selStart || out.selEnd != selEnd;
    return out;
}

/// A word fully inside the selection cannot be looked at.
[[nodiscard]] inline bool phraseWordInsideSelection(int wordStart, int wordEnd, int selStart,
                                                    int selEnd)
{
    return selStart >= 0 && selEnd > selStart && wordStart >= selStart && wordEnd <= selEnd;
}

/// Shift extends the selection across the new word and the text between them.
/// The caret lands on the outer edge of that word. Otherwise the word replaces the selection.
[[nodiscard]] inline PhraseCaretEdit selectPhraseWordRange(int caret, int selStart, int selEnd,
                                                           int wordStart, int wordEnd, bool shift)
{
    PhraseCaretEdit out;
    out.caret = caret;
    out.selStart = selStart;
    out.selEnd = selEnd;
    if (phraseWordInsideSelection(wordStart, wordEnd, selStart, selEnd)) {
        return out;
    }
    out.caret = wordEnd;
    out.selStart = wordStart;
    out.selEnd = wordEnd;
    const bool hasSel = selStart >= 0 && selEnd > selStart;
    if (!shift || !hasSel) {
        out.changed = caret != out.caret || selStart != out.selStart || selEnd != out.selEnd;
        return out;
    }
    out.selStart = qMin(selStart, wordStart);
    out.selEnd = qMax(selEnd, wordEnd);
    out.caret = wordStart < selStart ? out.selStart : out.selEnd;
    out.keepChooser = true;
    out.changed = caret != out.caret || selStart != out.selStart || selEnd != out.selEnd;
    return out;
}

} // namespace gazer
