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

template <typename Fn>
void withPhraseLayout(const QRectF& pad, const QString& text, const QFont& font, int flags, Fn&& fn)
{
    QTextLayout layout(text, font);
    layout.setCacheEnabled(true);
    QTextOption opt;
    opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
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
                              int flags, const QColor& color, int caretIndex)
{
    withPhraseLayout(pad, text, font, flags, [&](QTextLayout& layout, const QPointF& origin,
                                                 const QRectF& box) {
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

} // namespace gazer
