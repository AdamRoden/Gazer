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
#include <QVector>
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

/// Shrink `startPx` until `text` fits in `box`, or stop at `minPx`.
[[nodiscard]] inline int fitFontPx(const QString& family, int weight, int startPx, int minPx,
                                   const QString& text, const QRectF& box, int flags)
{
    int px = startPx;
    while (px > minPx) {
        const QRectF br = QFontMetricsF(QFont(family, px, weight)).boundingRect(box, flags, text);
        if (br.width() <= box.width() + 0.5 && br.height() <= box.height() + 0.5) {
            break;
        }
        --px;
    }
    return qMax(minPx, px);
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

/// Point size and alignment for a wrapped label style.
struct PhraseStyle {
    int px = 14;
    int weight = QFont::DemiBold;
    int flags = int(Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap);
};

[[nodiscard]] inline PhraseStyle phraseStyle(const QString& textStyle, const QString& role)
{
    PhraseStyle face;
    const QString ts = textStyle.toLower();
    const bool valueLabel = role.compare(QLatin1String("value"), Qt::CaseInsensitive) == 0;
    face.flags = int((valueLabel ? Qt::AlignHCenter : Qt::AlignLeft) | Qt::AlignVCenter
                     | Qt::TextWordWrap);
    if (ts == QLatin1String("caption")) {
        face.px = 12;
        face.weight = QFont::Normal;
    } else if (ts == QLatin1String("body")) {
        face.px = 18;
        face.weight = QFont::Normal;
    } else if (ts == QLatin1String("subtitle")) {
        face.px = 20;
    } else if (ts == QLatin1String("title")) {
        face.px = 24;
    }
    return face;
}

/// Wrapped block height at `font`. Empty text is one line.
[[nodiscard]] inline double phraseBlockHeight(const QString& text, const QFont& font, double width,
                                              int flags)
{
    if (text.isEmpty()) {
        return qMax(1.0, QFontMetricsF(font).lineSpacing());
    }
    const QRectF wide(0, 0, qMax(1.0, width), 1.0e6);
    double h = 0.0;
    withPhraseLayout(wide, text, font, flags, [&](const QTextLayout& layout, const QPointF&,
                                                   const QRectF&) {
        h = qMax(h, layout.boundingRect().height());
        if (layout.lineCount() > 0) {
            const QTextLine last = layout.lineAt(layout.lineCount() - 1);
            h = qMax(h, last.y() + last.height());
        }
    });
    if (h < 1.0) {
        h = QFontMetricsF(font).lineSpacing();
    }
    return h;
}

/// Authored row until the block needs more room, then the height that fits it.
[[nodiscard]] inline double phraseRowPx(double baseRowPx, double blockPx, double chromePx)
{
    const double need = qMax(0.0, blockPx) + qMax(0.0, chromePx);
    if (need <= baseRowPx + 0.5) {
        return baseRowPx;
    }
    return qCeil(need - 1e-6);
}

/// Text block `paintLabel` draws for a wrapped label. `textRect` is in `cell` coordinates.
/// A caption keeps the label in the upper band and leaves the caption below it.
/// `shrinkToFit` is off for the composer phrase: the row grows instead.
struct PhraseTextFrame {
    QRectF textRect;
    QFont font;
    int flags = 0;
    bool valid = false;
};

[[nodiscard]] inline PhraseTextFrame phraseTextFrame(const QString& family, const QRectF& cell,
                                                     const QString& label, const QString& caption,
                                                     const QString& textStyle, const QString& role,
                                                     bool phraseInk, bool shrinkToFit = true)
{
    PhraseTextFrame out;
    const QString ts = textStyle.toLower();
    if (ts == QLatin1String("row") || ts == QLatin1String("section")
        || ts == QLatin1String("filename")) {
        return out;
    }
    const QRectF pad = phrasePad(cell);
    if (pad.isEmpty()) {
        return out;
    }
    const PhraseStyle face = phraseStyle(textStyle, role);
    const int titlePx = face.px;
    const int titleWeight = face.weight;
    const int flags = face.flags;
    if (caption.isEmpty()) {
        int px = titlePx;
        if (shrinkToFit) {
            px = phraseInk ? fontPxToFitPhrase(family, titleWeight, titlePx, 11, label, pad, flags)
                           : fitFontPx(family, titleWeight, titlePx, 11, label, pad, flags);
        }
        out.textRect = pad;
        out.font = QFont(family, px, titleWeight);
        out.flags = flags;
        out.valid = true;
        return out;
    }
    QFont tFont(family, titlePx, titleWeight);
    auto titleH = [&]() { return QFontMetricsF(tFont).boundingRect(pad, flags, label).height(); };
    const double capLine = QFontMetricsF(QFont(family, 12)).lineSpacing();
    if (shrinkToFit) {
        while (tFont.pointSize() > 11 && titleH() + capLine + 4.0 > pad.height()) {
            tFont.setPointSize(tFont.pointSize() - 1);
        }
    }
    const double natural = shrinkToFit ? titleH() : phraseBlockHeight(label, tFont, pad.width(), flags);
    const double th = shrinkToFit
                          ? qBound(QFontMetricsF(tFont).lineSpacing(), natural,
                                   qMax(QFontMetricsF(tFont).lineSpacing(), pad.height() - capLine - 2.0))
                          : qMax(QFontMetricsF(tFont).lineSpacing(), natural);
    out.textRect = QRectF(pad.left(), pad.top(), pad.width(), th);
    out.font = tFont;
    out.flags = flags;
    out.valid = true;
    return out;
}

/// One wrapped line. `cursorX[i]` is the screen X of caret `start + i` (through `end`).
struct PhraseLineGeom {
    int start = 0;
    int end = 0;
    QRectF rect;
    QVector<double> cursorX;
};

/// Lines of `text` in the same coordinates as `textRect`. An empty string is one line.
[[nodiscard]] inline QVector<PhraseLineGeom> layoutPhraseLines(const QRectF& textRect,
                                                               const QString& text, const QFont& font,
                                                               int flags,
                                                               PhraseWrap wrap = PhraseWrap::Words)
{
    QVector<PhraseLineGeom> lines;
    if (textRect.isEmpty()) {
        return lines;
    }
    withPhraseLayout(textRect, text, font, flags, wrap,
                     [&](const QTextLayout& layout, const QPointF& origin, const QRectF& box) {
        if (layout.lineCount() <= 0) {
            PhraseLineGeom g;
            const QFontMetricsF fm(font);
            const double h = qMax(10.0, fm.ascent());
            const double y = box.center().y() - h * 0.5;
            g.rect = QRectF(box.left(), y, qMax(1.0, box.width()), h);
            g.cursorX.push_back(origin.x());
            lines.push_back(std::move(g));
            return;
        }
        lines.reserve(layout.lineCount());
        for (int i = 0; i < layout.lineCount(); ++i) {
            const QTextLine line = layout.lineAt(i);
            if (!line.isValid()) {
                continue;
            }
            PhraseLineGeom g;
            g.start = line.textStart();
            g.end = line.textStart() + line.textLength();
            const double h = qMax(1.0, line.height());
            g.rect = QRectF(box.left(), origin.y() + line.y(), qMax(1.0, box.width()), h);
            const int n = g.end - g.start;
            g.cursorX.reserve(n + 1);
            for (int k = 0; k <= n; ++k) {
                g.cursorX.push_back(origin.x() + line.cursorToX(g.start + k));
            }
            lines.push_back(std::move(g));
        }
    });
    return lines;
}

/// Cover the plain glyphs, then draw the same characters bold and scaled about their center.
inline void paintGrowingPhraseWord(QPainter& p, const QTextLayout& layout, const QPointF& origin,
                                   const QFont& font, const QColor& color, int growStart,
                                   int growEnd, double scale, const QColor& cover)
{
    if (growEnd <= growStart || layout.lineCount() <= 0) {
        return;
    }
    const QString text = layout.text();
    const int n = text.size();
    const int fromSel = qBound(0, growStart, n);
    const int toSel = qBound(0, growEnd, n);
    if (fromSel >= toSel) {
        return;
    }
    const double s = qBound(1.0, scale, 1.5);
    QFont bold = font;
    bold.setBold(true);
    const QFontMetricsF bfm(bold);
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
        const QRectF word(origin.x() + qMin(x1, x2), origin.y() + line.y(),
                          qMax(1.0, qAbs(x2 - x1)), qMax(1.0, line.height()));
        if (cover.isValid()) {
            p.fillRect(word.adjusted(-1.0, 0.0, 1.0, 0.0), cover);
        }
        const QString frag = text.mid(from, to - from);
        const double adv = bfm.horizontalAdvance(frag);
        const QPointF anchor(word.center().x() - adv * 0.5, origin.y() + line.y() + line.ascent());
        p.save();
        p.translate(word.center());
        p.scale(s, s);
        p.translate(-word.center());
        p.setFont(bold);
        p.setPen(color);
        p.drawText(anchor, frag);
        p.restore();
    }
}

inline void paintPhraseCaret(QPainter& p, const QTextLayout& layout, const QPointF& origin,
                             const QRectF& box, const QFont& font, const QColor& color,
                             int caretIndex, int caretLine);

inline void paintPhraseLayout(QPainter& p, const QRectF& pad, const QString& text, const QFont& font,
                              int flags, const QColor& color, int caretIndex,
                              int selectionStart = -1, int selectionEnd = -1,
                              PhraseWrap wrap = PhraseWrap::Words, int caretLine = -1,
                              int growStart = -1, int growEnd = -1, double growScale = 1.0,
                              const QColor& growCover = {})
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
        if (caretIndex >= 0) {
            paintPhraseCaret(p, layout, origin, box, font, color, caretIndex, caretLine);
        }
        paintGrowingPhraseWord(p, layout, origin, font, color, growStart, growEnd, growScale,
                               growCover);
    });
}

inline void paintPhraseCaret(QPainter& p, const QTextLayout& layout, const QPointF& origin,
                             const QRectF& box, const QFont& font, const QColor& color,
                             int caretIndex, int caretLine)
{
        const int i = qBound(0, caretIndex, layout.text().size());
        const QFontMetricsF fm(font);
        qreal cx = origin.x();
        qreal cy = origin.y();
        qreal ch = qMax(10.0, fm.ascent());
        QTextLine line;
        if (caretLine >= 0 && caretLine < layout.lineCount()) {
            line = layout.lineAt(caretLine);
        } else if (layout.lineCount() > 0) {
            line = layout.lineForTextPosition(i);
        }
        if (line.isValid()) {
            cx = origin.x() + line.cursorToX(i);
            cy = origin.y() + line.y();
            ch = qMax(10.0, line.height());
        } else {
            cy = box.center().y() - ch * 0.5;
        }
        QColor bar = color;
        bar.setAlpha(qMax(180, bar.alpha()));
        p.fillRect(QRectF(cx, cy, 2.0, ch), bar);
}

/// Body size of the export file-name line. It does not shrink to fit.
inline constexpr int kFileNameFontPx = 18;

/// Single-line file name. `caption` reserves a line under the text.
[[nodiscard]] inline QRectF fileNameLineRect(const QFont& font, const QRectF& pad, bool caption)
{
    QRectF line = pad;
    const QFontMetricsF fm(font);
    const double lineH = qMax(1.0, fm.lineSpacing());
    if (caption && pad.height() > lineH + 8.0) {
        const double cap = QFontMetricsF(QFont(font.family(), 12)).lineSpacing() + 2.0;
        line.setHeight(qMax(lineH, pad.height() - cap));
    }
    return line;
}

/// Draw one no-wrap line, clipped to `line`.
inline void paintFileNameLine(QPainter& p, const QRectF& line, const QString& text, const QFont& font,
                              const QColor& color, int caretIndex, int selectionStart, int selectionEnd,
                              int growStart = -1, int growEnd = -1, double growScale = 1.0,
                              const QColor& growCover = {})
{
    if (line.isEmpty()) {
        return;
    }
    const QFontMetricsF fm(font);
    const double width = qMax(line.width(), fm.horizontalAdvance(text) + 8.0);
    const QRectF wide(line.left(), line.top(), width, line.height());
    p.save();
    p.setClipRect(line);
    paintPhraseLayout(p, wide, text, font, int(Qt::AlignLeft | Qt::AlignVCenter), color, caretIndex,
                      selectionStart, selectionEnd, PhraseWrap::None, -1, growStart, growEnd,
                      growScale, growCover);
    p.restore();
}

} // namespace gazer
