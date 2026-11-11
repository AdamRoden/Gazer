#pragma once

#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QTextLayout>
#include <QTextLine>
#include <QTextOption>
#include <QtMath>
#include <utility>

namespace gazer {

inline constexpr int kPhraseCaretBlinkMs = 530;
inline constexpr double kPhrasePadX = 12.0;
inline constexpr double kPhrasePadY = 6.0;

[[nodiscard]] inline QRectF phrasePad(const QRectF& cell)
{
    return cell.adjusted(kPhrasePadX, kPhrasePadY, -kPhrasePadX, -kPhrasePadY);
}

enum class PhraseWrap { Words, None };

template <typename Fn>
void withPhraseLayout(const QRectF& pad, const QString& text, const QFont& font, int flags,
                      PhraseWrap wrap, Fn&& fn)
{
    QTextLayout layout(text, font);
    layout.setCacheEnabled(true);
    QTextOption opt;
    opt.setWrapMode(wrap == PhraseWrap::None ? QTextOption::NoWrap
                                            : QTextOption::WrapAtWordBoundaryOrAnywhere);
    Qt::Alignment align = Qt::AlignLeft;
    if (flags & Qt::AlignHCenter) {
        align = Qt::AlignHCenter;
    } else if (flags & Qt::AlignRight) {
        align = Qt::AlignRight;
    }
    if (flags & Qt::AlignVCenter) {
        align |= Qt::AlignVCenter;
    } else if (flags & Qt::AlignBottom) {
        align |= Qt::AlignBottom;
    } else {
        align |= Qt::AlignTop;
    }
    opt.setAlignment(align);
    layout.setTextOption(opt);

    const qreal width = qMax(1.0, pad.width());
    layout.beginLayout();
    qreal y = 0.0;
    for (;;) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLineWidth(width);
        line.setPosition(QPointF(0.0, y));
        y += line.height();
    }
    layout.endLayout();

    qreal y0 = pad.top();
    const qreal blockH = layout.boundingRect().height();
    if (flags & Qt::AlignVCenter) {
        y0 = pad.top() + qMax(0.0, (pad.height() - blockH) * 0.5);
    } else if (flags & Qt::AlignBottom) {
        y0 = pad.bottom() - blockH;
    }
    fn(layout, QPointF(pad.left(), y0), pad);
}

template <typename Fn>
void withPhraseLayout(const QRectF& pad, const QString& text, const QFont& font, int flags, Fn&& fn)
{
    withPhraseLayout(pad, text, font, flags, PhraseWrap::Words, std::forward<Fn>(fn));
}

[[nodiscard]] inline int fontPxToFitPhrase(const QString& family, int weight, int startPx,
                                           int minPx, const QString& text, const QRectF& pad,
                                           int flags)
{
    int px = startPx;
    while (px > minPx) {
        bool fits = false;
        withPhraseLayout(pad, text, QFont(family, px, weight), flags,
                         [&](const QTextLayout& layout, const QPointF&, const QRectF& box) {
                             const QRectF br = layout.boundingRect();
                             fits = br.width() <= box.width() + 0.5
                                    && br.height() <= box.height() + 0.5;
                         });
        if (fits) {
            break;
        }
        --px;
    }
    return qMax(minPx, px);
}

inline void paintPhraseLayout(QPainter& p, const QRectF& pad, const QString& text, const QFont& font,
                              int flags, const QColor& color, int caretIndex,
                              int selectionStart = -1, int selectionEnd = -1,
                              PhraseWrap wrap = PhraseWrap::Words)
{
    withPhraseLayout(pad, text, font, flags, wrap, [&](QTextLayout& layout, const QPointF& origin,
                                                 const QRectF& box) {
        if (selectionStart >= 0 && selectionEnd > selectionStart && layout.lineCount() > 0) {
            const int n = layout.text().size();
            const int fromSel = qBound(0, selectionStart, n);
            const int toSel = qBound(0, selectionEnd, n);
            QColor hi = color;
            hi.setAlpha(80);
            p.setPen(Qt::NoPen);
            for (int lineIndex = 0; lineIndex < layout.lineCount(); ++lineIndex) {
                const QTextLine line = layout.lineAt(lineIndex);
                if (!line.isValid()) {
                    continue;
                }
                const int lineStart = line.textStart();
                const int lineEnd = lineStart + line.textLength();
                const int from = qMax(fromSel, lineStart);
                const int to = qMin(toSel, lineEnd);
                if (from >= to) {
                    continue;
                }
                const qreal x1 = line.cursorToX(from);
                const qreal x2 = line.cursorToX(to);
                p.fillRect(QRectF(origin.x() + qMin(x1, x2), origin.y() + line.y(), qAbs(x2 - x1),
                                  qMax(1.0, line.height())),
                           hi);
            }
        }
        p.setPen(color);
        p.setFont(font);
        layout.draw(&p, origin);
        if (caretIndex < 0) {
            return;
        }
        const int i = qBound(0, caretIndex, layout.text().size());
        const QFontMetricsF fm(font);
        qreal cx = origin.x();
        qreal cy = origin.y();
        qreal ch = qMax(10.0, fm.ascent());
        if (layout.lineCount() > 0) {
            const QTextLine line = layout.lineForTextPosition(i);
            if (line.isValid()) {
                cx = origin.x() + line.cursorToX(i);
                cy = origin.y() + line.y();
                ch = qMax(10.0, line.height());
            }
        } else {
            cy = box.center().y() - ch * 0.5;
        }
        QColor bar = color;
        bar.setAlpha(qMax(180, bar.alpha()));
        p.fillRect(QRectF(cx, cy, 2.0, ch), bar);
    });
}

/// Body size of the export file-name line. It does not shrink to fit.
inline constexpr int kFileNameFontPx = 18;

/// Horizontal shift that keeps `caretAdvance` in view. A line wider than the
/// field pins the caret at the center until an end is reached. A short line
/// stays at scroll 0, so the caret keeps its natural x.
[[nodiscard]] inline double pinnedFileScroll(double caretAdvance, double textWidth, double viewWidth)
{
    const double view = qMax(1.0, viewWidth);
    const double maxScroll = qMax(0.0, textWidth - view);
    return qBound(0.0, caretAdvance - view * 0.5, maxScroll);
}

/// `prefix[i]` is the width of the first `i` characters. `prefix[0]` is 0.
[[nodiscard]] inline int caretIndexAtAdvances(const double* prefix, int count, double docX)
{
    if (prefix == nullptr || count <= 1 || docX <= 0.0) {
        return 0;
    }
    int best = 0;
    for (int i = 1; i < count; ++i) {
        if (docX < (prefix[i - 1] + prefix[i]) * 0.5) {
            break;
        }
        best = i;
    }
    return qMin(best, count - 1);
}

struct FileNameScroll {
    QRectF line;
    double scroll = 0;
    double caretX = 0;
    double caretAdvance = 0;
    double textWidth = 0;
    double viewWidth = 0;
};

/// Single-line file name. `caption` reserves a line under the text.
[[nodiscard]] inline FileNameScroll fileNameScroll(const QFont& font, const QString& text, int caret,
                                                   const QRectF& pad, bool caption)
{
    FileNameScroll s;
    const QFontMetricsF fm(font);
    const double lineH = qMax(1.0, fm.lineSpacing());
    s.line = pad;
    if (caption && pad.height() > lineH + 8.0) {
        const double cap = QFontMetricsF(QFont(font.family(), 12)).lineSpacing() + 2.0;
        s.line.setHeight(qMax(lineH, pad.height() - cap));
    }
    s.viewWidth = qMax(1.0, s.line.width());
    const int i = qBound(0, caret, text.size());
    s.caretAdvance = fm.horizontalAdvance(text.left(i));
    s.textWidth = fm.horizontalAdvance(text);
    s.scroll = pinnedFileScroll(s.caretAdvance, s.textWidth, s.viewWidth);
    s.caretX = s.caretAdvance - s.scroll;
    return s;
}

[[nodiscard]] inline int fileNameCaretAt(const QFont& font, const QString& text, double docX)
{
    if (text.isEmpty() || docX <= 0.0) {
        return 0;
    }
    const QFontMetricsF fm(font);
    int best = 0;
    double prev = 0.0;
    for (int i = 1; i <= text.size(); ++i) {
        const double adv = fm.horizontalAdvance(text.left(i));
        if (docX < (prev + adv) * 0.5) {
            break;
        }
        best = i;
        prev = adv;
    }
    return best;
}

/// Draw one no-wrap line shifted by `scrollPx`, clipped to `line`.
inline void paintFileNameLine(QPainter& p, const QRectF& line, const QString& text, const QFont& font,
                              const QColor& color, int caretIndex, int selectionStart, int selectionEnd,
                              double scrollPx)
{
    if (line.isEmpty()) {
        return;
    }
    const QFontMetricsF fm(font);
    const double width = qMax(line.width(), fm.horizontalAdvance(text) + 8.0);
    const QRectF wide(line.left() - scrollPx, line.top(), width, line.height());
    p.save();
    p.setClipRect(line);
    paintPhraseLayout(p, wide, text, font, int(Qt::AlignLeft | Qt::AlignVCenter), color, caretIndex,
                      selectionStart, selectionEnd, PhraseWrap::None);
    p.restore();
}

} // namespace gazer
