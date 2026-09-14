#include "ui/BoardPaint.h"

#include "ui/KeySymbols.h"
#include "ui/PhraseLayout.h"
#include "ui/ProgressPaint.h"
#include "layout/RoundBox.h"
#include "ui/ColorField.h"
#include "ui/PoseChart.h"
#include "ui/ScrollBar.h"
#include "ui/SliderTrack.h"

#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QtMath>

namespace gazer {
namespace BoardPaint {

QString segoeFamily()
{
    return QStringLiteral("Segoe UI");
}

int fontPxToFit(const QString& family, int weight, int startPx, int minPx, const QString& text,
                const QRectF& box, int flags)
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

namespace {

void paintLabelText(QPainter& p, const QRectF& box, const QFont& font, int flags,
                    const QString& text, const QColor& color, int caretIndex)
{
    if (box.isEmpty() || !color.isValid()) {
        return;
    }
    if (caretIndex >= 0) {
        paintPhraseLayout(p, box, text, font, flags, color, caretIndex);
        return;
    }
    p.setPen(color);
    p.setFont(font);
    p.drawText(box, flags, text);
}

int fontPixelsToFill(const QString& family, int weight, const QString& text, const QRectF& box,
                     int flags, int minPx = 10)
{
    int px = qMax(minPx, qRound(qMin(box.width(), box.height()) * 0.72));
    while (px > minPx) {
        QFont f(family, -1, weight);
        f.setPixelSize(px);
        const QRectF br = QFontMetricsF(f).boundingRect(box, flags, text);
        if (br.width() <= box.width() + 0.5 && br.height() <= box.height() + 0.5) {
            break;
        }
        --px;
    }
    return qMax(minPx, px);
}

} // namespace

void fillRound(QPainter& p, const QRectF& r, double radius, const QColor& bg)
{
    fillRound(p, r, PageBox::all(radius), bg);
}

void fillRound(QPainter& p, const QRectF& r, const PageBox& radii, const QColor& bg)
{
    if (!bg.isValid() || bg.alpha() <= 0 || r.isEmpty()) {
        return;
    }
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawPath(roundedBoxPath(r, radii));
}

void strokeRound(QPainter& p, const QRectF& r, double radius, const QColor& color, double width)
{
    strokeRound(p, r, PageBox::all(radius), color, PageBox::all(width));
}

void strokeRound(QPainter& p, const QRectF& r, const PageBox& radii, const QColor& color,
                 const PageBox& width)
{
    if (!color.isValid() || color.alpha() <= 0 || r.isEmpty()) {
        return;
    }
    const double t = width.at(0);
    const double ri = width.at(1);
    const double btm = width.at(2);
    const double l = width.at(3);
    if (t <= 0.0 && ri <= 0.0 && btm <= 0.0 && l <= 0.0) {
        return;
    }
    if (width.uniform() && t > 0.0) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(color, t, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
        const double h = t / 2.0;
        p.drawPath(roundedBoxPath(r.adjusted(h, h, -h, -h), radii));
        return;
    }
    const QPainterPath outer = roundedBoxPath(r, radii);
    const QRectF innerRect = r.adjusted(l, t, -ri, -btm);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    if (innerRect.width() <= 0.5 || innerRect.height() <= 0.5) {
        p.drawPath(outer);
        return;
    }
    const PageBox fitted = fitCornerRadii(r, radii);
    const PageBox innerRadii = PageBox::of(qMax(0.0, fitted.at(0) - qMax(t, l)),
                                           qMax(0.0, fitted.at(1) - qMax(t, ri)),
                                           qMax(0.0, fitted.at(2) - qMax(btm, ri)),
                                           qMax(0.0, fitted.at(3) - qMax(btm, l)));
    QPainterPath ring = outer;
    ring.setFillRule(Qt::OddEvenFill);
    ring.addPath(roundedBoxPath(innerRect, innerRadii));
    p.drawPath(ring);
}

void paintLabel(QPainter& p, const PageTarget& t, const QRectF& r, const ThemeColors& theme,
                const QColor& canvas)
{
    const QString family = segoeFamily();
    const QString ts = t.textStyle.toLower();
    const QRectF pad = phrasePad(r);
    if (pad.isEmpty()) {
        return;
    }
    if (ts == QLatin1String("section")) {
        p.setPen(Qt::NoPen);
        p.setBrush(theme.accent);
        p.drawRoundedRect(QRectF(pad.left(), pad.center().y() - 7.0, 4.0, 14.0), 2.0, 2.0);
        p.setPen(theme.text.isValid() ? theme.text : theme.textSecondary);
        p.setFont(QFont(family, 13, QFont::DemiBold));
        p.drawText(pad.adjusted(16, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, t.label);
        return;
    }

    int titlePx = 14;
    int titleWeight = QFont::DemiBold;
    const bool valueLabel = t.role.compare(QLatin1String("value"), Qt::CaseInsensitive) == 0;
    int flags = int((valueLabel ? Qt::AlignHCenter : Qt::AlignLeft) | Qt::AlignVCenter
                    | Qt::TextWordWrap);
    if (ts == QLatin1String("caption")) {
        titlePx = 12;
        titleWeight = QFont::Normal;
    } else if (ts == QLatin1String("body")) {
        titlePx = 14;
        titleWeight = QFont::Normal;
    } else if (ts == QLatin1String("subtitle")) {
        titlePx = 20;
    } else if (ts == QLatin1String("title")) {
        titlePx = 24;
    }

    const QColor fill = theme.resolveFill(t.chrome.background.token, canvas, false, false);
    QColor titleFg = theme.readableForeground(fill, t.chrome.foreground.token, canvas);
    if (ts == QLatin1String("caption") && t.chrome.foreground.token.trimmed().isEmpty()) {
        titleFg = theme.textSecondary;
    }
    if (t.caption.isEmpty()) {
        const int px = t.caretIndex >= 0
                           ? fontPxToFitPhrase(family, titleWeight, titlePx, 11, t.label, pad, flags)
                           : fontPxToFit(family, titleWeight, titlePx, 11, t.label, pad, flags);
        paintLabelText(p, pad, QFont(family, px, titleWeight), flags, t.label, titleFg,
                       t.caretIndex);
        return;
    }

    const int capFlags =
        int((valueLabel ? Qt::AlignHCenter : Qt::AlignLeft) | Qt::AlignTop | Qt::TextWordWrap);
    QFont tFont(family, titlePx, titleWeight);
    auto titleH = [&]() { return QFontMetricsF(tFont).boundingRect(pad, flags, t.label).height(); };
    const double capLine = QFontMetricsF(QFont(family, 12)).lineSpacing();
    while (tFont.pointSize() > 11 && titleH() + capLine + 4.0 > pad.height()) {
        tFont.setPointSize(tFont.pointSize() - 1);
    }
    const double th = qBound(QFontMetricsF(tFont).lineSpacing(), titleH(),
                             qMax(QFontMetricsF(tFont).lineSpacing(), pad.height() - capLine - 2.0));
    const QRectF titleR(pad.left(), pad.top(), pad.width(), th);
    const QRectF capR(pad.left(), titleR.bottom() + 2.0, pad.width(),
                      qMax(1.0, pad.bottom() - titleR.bottom() - 2.0));
    const int capPx = fontPxToFit(family, QFont::Normal, 12, 9, t.caption, capR, capFlags);
    paintLabelText(p, titleR, tFont, flags, t.label, titleFg, t.caretIndex);
    p.setPen(theme.textSecondary);
    p.setFont(QFont(family, capPx));
    p.drawText(capR, capFlags, t.caption);
}

void paintTab(QPainter& p, const PageTarget& t, const QRectF& r, const ThemeColors& theme,
              const QColor& canvas, bool hovered, bool selected, double progress)
{
    const double radius = qMax(8.0, t.chrome.resolvedRadius().first());
    const QRectF pill = r.adjusted(4, 4, -4, 6);
    const QColor fill =
        theme.resolveFill(t.chrome.background.token, canvas, hovered && !selected, selected);
    if (selected || hovered) {
        QColor painted = fill;
        if (!selected) {
            painted.setAlpha(qBound(24, painted.alpha(), 80));
        }
        fillRound(p, pill, radius, painted);
    }
    const QColor fg = selected ? theme.readableForeground(fill, t.chrome.foreground.token, canvas)
                               : theme.textSecondary;
    const QRectF content = r.adjusted(8, 4, -8, -12);
    if (!t.icon.isEmpty()) {
        paintIconAndText(p, t, content, fg, theme);
    } else {
        const QString family = segoeFamily();
        const int flags = int(Qt::AlignCenter | Qt::TextWordWrap);
        const int px = fontPxToFit(family, selected ? QFont::DemiBold : QFont::Normal, 14, 11,
                                   t.label, content, flags);
        p.setPen(fg);
        p.setFont(QFont(family, px, selected ? QFont::DemiBold : QFont::Normal));
        p.drawText(content, flags, t.label);
    }
    const double tBar = selected ? 1.0 : progress;
    if (tBar > 0.01) {
        const double maxW = qMax(24.0, r.width() - 36.0);
        const double barW = maxW * tBar;
        const QRectF bar(r.center().x() - barW * 0.5, r.bottom() - 7.0, barW, selected ? 3.0 : 2.0);
        fillRound(p, bar, 1.5, theme.accent);
    }
}

void paintIconAndText(QPainter& p, const PageTarget& t, const QRectF& r, const QColor& fg,
                      const ThemeColors& theme)
{
    Q_UNUSED(theme);
    const QString family = segoeFamily();
    const bool hasText = !t.label.isEmpty();
    const bool hasIcon = !t.icon.isEmpty();
    const bool hasCap = !t.caption.isEmpty();
    if (!hasText && !hasIcon) {
        return;
    }
    const qreal w = r.width();
    const qreal h = r.height();
    const qreal pad = qBound(6.0, qMin(w, h) * 0.08, 14.0);
    const bool keyGlyph =
        !hasIcon && t.textStyle.compare(QLatin1String("key"), Qt::CaseInsensitive) == 0;
    const bool inlineIcon =
        hasIcon && hasText
        && t.textStyle.compare(QLatin1String("title"), Qt::CaseInsensitive) == 0;
    const int weight = keyGlyph ? QFont::Normal : QFont::DemiBold;

    QRectF iconR;
    QRectF textR = r.adjusted(pad, pad, -pad, -pad);
    QRectF capR;
    int flags = int(Qt::AlignCenter | Qt::TextWordWrap);
    int capFlags = int(Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap);
    int startPx = 14;
    bool paintedIcon = false;

    if (inlineIcon) {
        const QRectF padBox = r.adjusted(pad, pad, -pad, -pad);
        if (padBox.isEmpty()) {
            return;
        }
        int px = 24;
        QFont tFont(family, px, weight);
        flags = int(Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap);
        const qreal gap = 12.0;
        auto textSize = [&]() { return QFontMetricsF(tFont).boundingRect(t.label).size(); };
        qreal side = QFontMetricsF(tFont).height();
        while (px > 12) {
            side = QFontMetricsF(tFont).height();
            const QSizeF ts = textSize();
            if (side + gap + ts.width() <= padBox.width() + 0.5
                && qMax(side, ts.height()) <= padBox.height() + 0.5) {
                break;
            }
            --px;
            tFont.setPointSize(px);
        }
        side = QFontMetricsF(tFont).height();
        const QSizeF ts = textSize();
        const qreal totalW = side + gap + ts.width();
        const qreal totalH = qMax(side, ts.height());
        const qreal x0 = padBox.left() + qMax(0.0, (padBox.width() - totalW) * 0.5);
        const qreal y0 = padBox.top() + qMax(0.0, (padBox.height() - totalH) * 0.5);
        iconR = QRectF(x0, y0 + (totalH - side) * 0.5, side, side);
        paintedIcon = KeySymbols::paint(p, t.icon, iconR, fg);
        textR = QRectF(iconR.right() + gap, y0, ts.width(), totalH);
        paintLabelText(p, textR, tFont, flags, t.label, fg, t.caretIndex);
        return;
    }

    if (hasIcon && hasText && h >= 52.0 && w >= 40.0) {
        const qreal capBand = hasCap ? qBound(14.0, h * 0.20, 22.0) : 0.0;
        const qreal labelBand = qBound(18.0, h * 0.28, 30.0);
        const QRectF iconArea(r.left() + pad, r.top() + pad, w - 2.0 * pad,
                              qMax(8.0, h - 2.0 * pad - labelBand - capBand - 2.0));
        const qreal side = qMax(8.0, qMin(iconArea.width(), iconArea.height()));
        iconR = QRectF(iconArea.center().x() - side / 2.0, iconArea.center().y() - side / 2.0, side,
                       side);
        paintedIcon = KeySymbols::paint(p, t.icon, iconR, fg);
        if (hasCap) {
            capR = QRectF(r.left() + 6.0, r.bottom() - pad - capBand, w - 12.0, capBand);
            textR = QRectF(r.left() + 6.0, capR.top() - labelBand, w - 12.0, labelBand);
        } else {
            textR = QRectF(r.left() + 6.0, r.bottom() - pad - labelBand, w - 12.0, labelBand);
        }
        flags = int(Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextWordWrap);
        startPx = 13;
    } else if (hasIcon && hasText) {
        const qreal side = qMax(8.0, qMin(h - 2.0 * pad, w * 0.38));
        iconR = QRectF(r.left() + pad, r.center().y() - side / 2.0, side, side);
        paintedIcon = KeySymbols::paint(p, t.icon, iconR, fg);
        textR = QRectF(iconR.right() + 8.0, r.top() + pad, r.right() - pad - iconR.right() - 8.0,
                       h - 2.0 * pad);
        flags = int(Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap);
        startPx = 13;
        if (hasCap) {
            const qreal capBand = qBound(12.0, textR.height() * 0.38, 20.0);
            capR = QRectF(textR.left(), textR.bottom() - capBand, textR.width(), capBand);
            textR.setHeight(qMax(8.0, textR.height() - capBand - 2.0));
            capFlags = int(Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap);
        }
    } else if (hasIcon) {
        const qreal side = qMax(8.0, qMin(w, h) - 2.0 * pad);
        iconR = QRectF(r.center().x() - side / 2.0, r.center().y() - side / 2.0, side, side);
        paintedIcon = KeySymbols::paint(p, t.icon, iconR, fg);
    } else if (hasText && hasCap) {
        const QRectF box = phrasePad(r);
        if (box.isEmpty()) {
            return;
        }
        flags = int(Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap);
        capFlags = int(Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap);
        QFont tFont(family, startPx, weight);
        auto titleH = [&]() {
            return QFontMetricsF(tFont).boundingRect(box, flags, t.label).height();
        };
        const double capLine = QFontMetricsF(QFont(family, 12)).lineSpacing();
        while (tFont.pointSize() > 11 && titleH() + capLine + 4.0 > box.height()) {
            tFont.setPointSize(tFont.pointSize() - 1);
        }
        const double th =
            qBound(QFontMetricsF(tFont).lineSpacing(), titleH(),
                   qMax(QFontMetricsF(tFont).lineSpacing(), box.height() - capLine - 2.0));
        textR = QRectF(box.left(), box.top(), box.width(), th);
        capR = QRectF(box.left(), textR.bottom() + 2.0, box.width(),
                      qMax(1.0, box.bottom() - textR.bottom() - 2.0));
    }

    if (hasText && !(paintedIcon && textR.height() < 10.0)) {
        QFont textFont;
        if (keyGlyph) {
            textFont = QFont(family, -1, weight);
            textFont.setPixelSize(fontPixelsToFill(family, weight, t.label, textR, flags));
        } else {
            textFont = QFont(family, fontPxToFit(family, weight, startPx, 10, t.label, textR, flags),
                             weight);
        }
        paintLabelText(p, textR, textFont, flags, t.label, fg, t.caretIndex);
        if (!capR.isEmpty() && hasCap) {
            p.setPen(fg);
            p.setFont(QFont(family, fontPxToFit(family, QFont::Normal, 12, 9, t.caption, capR,
                                                capFlags)));
            p.drawText(capR, capFlags, t.caption);
        }
    } else if (!paintedIcon && hasIcon) {
        p.setPen(fg);
        const int px = fontPxToFit(family, QFont::DemiBold, 14, 10, t.icon, textR, flags);
        p.setFont(QFont(family, px, QFont::DemiBold));
        p.drawText(textR, flags, t.icon);
    }
}

void paintSurface(QPainter& p, const QRectF& r, const PageChrome& chrome, const ThemeColors& theme,
                  GlassBackdrop* glass, bool grid, bool hovered, bool active, bool interactive,
                  const QColor& canvas)
{
    if (r.isEmpty()) {
        return;
    }
    const PageBox radii = chrome.resolvedRadius();
    const std::optional<QColor> fillColor = theme.resolveToken(chrome.background.token, canvas);
    const std::optional<QColor> borderColor = theme.resolveToken(chrome.borderColor.token, canvas);
    // Authored colors keep their alpha. Unset chrome still uses an opaque theme fill
    // so a board without a background stays a solid overlay.
    const bool authoredFill =
        fillColor && fillColor->isValid() && fillColor->alpha() > 0;
    const bool authoredThickness = chrome.thickness.has_value();
    const bool hover = hovered && interactive;
    QColor bg = theme.resolveFill(chrome.background.token, canvas, hover, active);
    QColor border = borderColor.value_or(theme.border);
    PageBox thickness = chrome.resolvedThickness();

    if (active) {
        border = theme.accentHover.isValid() ? theme.accentHover : theme.accent;
        if (thickness.first() < 1.5) {
            thickness = PageBox::all(1.5);
        }
    } else if (!grid && !authoredFill && !authoredThickness && thickness.first() <= 0.0) {
        thickness = PageBox::all(1.0);
        border = theme.border;
        border.setAlpha(qBound(28, border.alpha(), 70));
    }

    if (chrome.hasBlur() && glass) {
        const QColor tint = (bg.isValid() && bg.alpha() > 0) ? bg : QColor();
        glass->paint(p, r, radii, tint);
    } else if (bg.isValid() && bg.alpha() > 0) {
        fillRound(p, r, radii, bg);
    }
    strokeRound(p, r, radii, border, thickness);
}

namespace {

void paintLockRadio(QPainter& p, const QRectF& r, const QColor& color)
{
    const double d = qBound(8.0, qMin(r.width(), r.height()) * 0.22, 16.0);
    if (d < 6.0 || r.width() < d + 6.0 || r.height() < d + 6.0) {
        return;
    }
    const QRectF outer(r.right() - d - 3.5, r.top() + 3.5, d, d);
    QColor ring = color.isValid() ? color : QColor(255, 255, 255);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(ring, qMax(1.3, d * 0.14), Qt::SolidLine, Qt::RoundCap));
    p.drawEllipse(outer);
    const double inset = d * 0.28;
    p.setPen(Qt::NoPen);
    p.setBrush(ring);
    p.drawEllipse(outer.adjusted(inset, inset, -inset, -inset));
}

double controlMarkWidth(const QRectF& r)
{
    return qBound(28.0, qMin(r.width() * 0.22, r.height() * 0.85), 56.0);
}

void paintChoiceRadio(QPainter& p, const QRectF& r, const ThemeColors& theme, bool on)
{
    const double d = qBound(12.0, qMin(r.width(), r.height()) * 0.28, 22.0);
    if (d < 8.0 || r.width() < d + 16.0 || r.height() < d + 8.0) {
        return;
    }
    const QRectF outer(r.right() - d - 10.0, r.center().y() - d * 0.5, d, d);
    const QColor ring = on ? (theme.accent.isValid() ? theme.accent : theme.text)
                           : (theme.textSecondary.isValid() ? theme.textSecondary : theme.text);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(ring, qMax(1.6, d * 0.12), Qt::SolidLine, Qt::RoundCap));
    p.drawEllipse(outer);
    if (on) {
        const double inset = d * 0.28;
        p.setPen(Qt::NoPen);
        p.setBrush(ring);
        p.drawEllipse(outer.adjusted(inset, inset, -inset, -inset));
    }
}

void paintThemeCard(QPainter& p, const PageTarget& t, const QRectF& r, const ThemeColors& theme,
                    GlassBackdrop* glass, const QColor& canvas, bool hovered, double progress,
                    bool active)
{
    PageChrome chrome = t.chrome;
    const QColor window = theme.resolveToken(chrome.background.token, canvas).value_or(theme.bgMain);
    const QColor surface = theme.resolveToken(chrome.borderColor.token, canvas)
                               .value_or(ThemeColors::mix(window, theme.text, 0.08));
    const QColor accent = theme.resolveToken(chrome.foreground.token, canvas)
                              .value_or(theme.accent.isValid() ? theme.accent : theme.text);
    const QColor progressCol = theme.resolveToken(chrome.progressColor.token, canvas).value_or(accent);
    if (active) {
        chrome.borderColor = accent;
        chrome.thickness = PageBox::all(qMax(2.4, chrome.resolvedThickness().first()));
    }
    paintSurface(p, r, chrome, theme, glass, false, hovered, active, true, canvas);

    const double pad = qBound(6.0, qMin(r.width(), r.height()) * 0.06, 12.0);
    const double labelH = t.label.isEmpty() ? 0.0 : qBound(18.0, r.height() * 0.22, 28.0);
    const QRectF mock(r.left() + pad, r.top() + pad, r.width() - 2.0 * pad,
                      qMax(16.0, r.height() - 2.0 * pad - labelH - (labelH > 0.0 ? 4.0 : 0.0)));
    if (mock.height() >= 24.0 && mock.width() >= 40.0) {
        const double rad = qBound(6.0, mock.height() * 0.10, 10.0);
        fillRound(p, mock, rad, window);
        const QRectF panel = mock.adjusted(mock.width() * 0.08, mock.height() * 0.12,
                                           -mock.width() * 0.08, -mock.height() * 0.10);
        fillRound(p, panel, qMax(4.0, rad - 2.0), surface);
        const double gap = qBound(4.0, panel.width() * 0.06, 8.0);
        const double inset = qBound(4.0, panel.width() * 0.06, 6.0);
        const double cellH = qBound(18.0, panel.height() * 0.72, panel.height() - 8.0);
        const double cellW = qMax(12.0, (panel.width() - inset * 2.0 - gap) * 0.5);
        const double cellY = panel.center().y() - cellH * 0.5;
        const QRectF kb(panel.left() + inset, cellY, cellW, cellH);
        const QRectF mouse(kb.right() + gap, cellY, cellW, cellH);
        const double cellRad = qBound(4.0, cellH * 0.16, 8.0);
        const double borderW = qBound(1.6, cellH * 0.08, 2.8);
        const double iconSide = qMax(10.0, qMin(cellW, cellH) * 0.56);
        auto iconAt = [&](const QRectF& cell) {
            return QRectF(cell.center().x() - iconSide * 0.5, cell.center().y() - iconSide * 0.5,
                          iconSide, iconSide);
        };

        const QColor tintedBg = ThemeColors::mix(surface, accent, 0.38);
        fillRound(p, kb, cellRad, tintedBg);
        strokeRound(p, kb, cellRad, accent, borderW);
        KeySymbols::paint(p, QStringLiteral("keyboardKeys"), iconAt(kb),
                          ThemeColors::contrastOn(tintedBg));

        fillRound(p, mouse, cellRad, surface);
        strokeRound(p, mouse, cellRad, progressCol, borderW);
        KeySymbols::paint(p, QStringLiteral("mouse"), iconAt(mouse), accent);
    }

    if (labelH > 0.0) {
        const QRectF labelR(r.left() + 8.0, r.bottom() - pad - labelH, r.width() - 16.0, labelH);
        const QString family = segoeFamily();
        const int flags = int(Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextWordWrap);
        const int px = fontPxToFit(family, active ? QFont::DemiBold : QFont::Normal, 14, 10,
                                   t.label, labelR, flags);
        p.setPen(ThemeColors::contrastOn(window));
        p.setFont(QFont(family, px, active ? QFont::DemiBold : QFont::Normal));
        p.drawText(labelR, flags, t.label);
    }

    if (progress > 0.0 && t.interactive) {
        const PageBox radii = t.chrome.resolvedRadius();
        ProgressVisuals vis;
        vis.progressColor = accent;
        paintProgress(p, r, progress, vis, ProgressShape::RoundedRect, radii);
    }
}

void paintColorSwatch(QPainter& p, const PageTarget& t, const QRectF& r, const ThemeColors& theme,
                      const QColor& canvas, bool hovered, double progress, bool active,
                      bool roundedRect)
{
    const QColor fill = theme.resolveToken(t.chrome.background.token, canvas).value_or(theme.accent);
    if (!fill.isValid() || r.isEmpty()) {
        return;
    }
    QRectF well;
    double rad = 0.0;
    ProgressShape shape = ProgressShape::Ellipse;
    if (roundedRect) {
        const double pad = qBound(1.5, qMin(r.width(), r.height()) * 0.06, 5.0);
        well = r.adjusted(pad, pad, -pad, -pad);
        rad = qBound(3.0, qMin(well.width(), well.height()) * 0.28, 10.0);
        shape = ProgressShape::RoundedRect;
    } else {
        const double pad = qBound(4.0, qMin(r.width(), r.height()) * 0.14, 16.0);
        const double d = qMax(8.0, qMin(r.width(), r.height()) - 2.0 * pad);
        well = QRectF(r.center().x() - d * 0.5, r.center().y() - d * 0.5, d, d);
        rad = d * 0.5;
    }
    if (well.isEmpty()) {
        return;
    }
    const QColor ink = theme.text.isValid() ? theme.text : ThemeColors::contrastOn(fill);
    const double ringW = active ? qBound(2.0, rad * 0.35, 3.2) : (hovered ? 1.5 : 1.0);
    const QColor ring = active ? ink : ThemeColors::mix(fill, ink, 0.22);
    fillRound(p, well, rad, fill);
    strokeRound(p, well, rad, ring, ringW);
    if (progress > 0.0 && t.interactive) {
        ProgressVisuals vis;
        vis.progressColor = theme.accent.isValid() ? theme.accent : ink;
        paintProgress(p, well.adjusted(-3.0, -3.0, 3.0, 3.0), progress, vis, shape,
                      PageBox::all(rad));
    }
}

void paintToggleSwitch(QPainter& p, const QRectF& r, const ThemeColors& theme, const QColor& canvas,
                       bool on)
{
    const double h = qBound(16.0, qMin(r.height() * 0.42, 26.0), r.width() * 0.22);
    const double w = h * 1.72;
    if (w + 16.0 > r.width() || h + 8.0 > r.height()) {
        return;
    }
    const QRectF track(r.right() - w - 10.0, r.center().y() - h * 0.5, w, h);
    const QColor seed = canvas.isValid() ? canvas : theme.bgMain;
    QColor fill = on ? theme.accent : ThemeColors::mixTone(seed, 80);
    if (!fill.isValid()) {
        fill = ThemeColors::mixTone(seed, 80);
    }
    fillRound(p, track, h * 0.5, fill);
    const double d = qMax(10.0, h - 6.0);
    const double x = on ? track.right() - d - 3.0 : track.left() + 3.0;
    fillRound(p, QRectF(x, track.center().y() - d * 0.5, d, d), d * 0.5,
              ThemeColors::contrastOn(fill));
}

} // namespace

void paintTarget(QPainter& p, const PageTarget& t, const QRectF& r, const ThemeColors& theme,
                 const QColor& canvas, GlassBackdrop* glass, bool hovered, double progress,
                 bool flashing, bool active, const ProgressVisuals& pv, const Live& live,
                 bool locked)
{
    if (r.isEmpty()) {
        return;
    }
    const PageBox radii = t.chrome.resolvedRadius();
    const double radius = radii.first();
    ProgressVisuals vis = pv;
    if (t.chrome.progressStyle) {
        vis.applyStyle(*t.chrome.progressStyle);
    }
    if (const std::optional<QColor> pc = theme.resolveToken(t.chrome.progressColor.token, canvas)) {
        if (pc->isValid()) {
            vis.progressColor = *pc;
        }
    }
    const QColor fillForFg =
        theme.resolveFill(t.chrome.background.token, canvas, hovered, active);
    QColor fg = theme.readableForeground(fillForFg, t.chrome.foreground.token, canvas);
    if (active && !t.activeState.isEmpty()) {
        const QColor accent = theme.accent.isValid() ? theme.accent : fg;
        fg = ThemeColors::contrastRatio(accent, fillForFg) >= ThemeColors::kReadableContrast
                 ? accent
                 : ThemeColors::contrastOn(fillForFg);
    }
    const QString role = t.role.toLower();
    if (role == QLatin1String("slider")) {
        paintSurface(p, r, t.chrome, theme, glass, false, false, false, false, canvas);
        const QString channel = t.caption.isEmpty() ? t.id : t.caption;
        const bool scrubbing =
            !live.sliderScrubId.isEmpty()
            && (sessionKey(t) == live.sliderScrubId || t.id == live.sliderScrubId
                || t.id.endsWith(QLatin1Char('/') + live.sliderScrubId));
        SliderTrack::paint(p, r, theme, vis, live.previewColor, channel, t.label, hovered, progress,
                           scrubbing, live.sliderScrubT, live.sliderScrubValue,
                           live.sliderScrubProgress);
    } else if (role == QLatin1String("colorfield")) {
        ColorField::paint(p, r, live.previewColor);
    } else if (role == QLatin1String("scrollbar")) {
        paintSurface(p, r, t.chrome, theme, glass, false, false, false, false, canvas);
        ScrollBar::paint(p, r, theme, ScrollBar::parseSpec(t.caption));
    } else if (role == QLatin1String("preview")) {
        SliderTrack::paintPreview(p, r, radius, live.previewColor);
        paintLabel(p, t, r, theme, canvas);
    } else if (role == QLatin1String("headpreview")) {
        if (!live.headPreview.isNull()) {
            p.setRenderHint(QPainter::SmoothPixmapTransform, true);
            p.drawImage(r, live.headPreview);
        } else {
            paintSurface(p, r, t.chrome, theme, glass, false, false, false, false, canvas);
            paintLabel(p, t, r, theme, canvas);
        }
    } else if (role == QLatin1String("curvefield")) {
        const QVector<HeadPoseCurvePoint> pts =
            live.curvePoints.isEmpty() ? PoseChart::parseCaption(t.caption) : live.curvePoints;
        PoseChart::paintCurve(p, r, theme, pts, live.curveSelected, live.curveLiveIn,
                              live.curveLiveOn);
    } else if (role == QLatin1String("tab")) {
        paintTab(p, t, r, theme, canvas, hovered, active || t.actions.isEmpty(), progress);
    } else if (role == QLatin1String("label") || role == QLatin1String("value")
               || role == QLatin1String("display")) {
        paintSurface(p, r, t.chrome, theme, glass, false, false, false, false, canvas);
        paintLabel(p, t, r, theme, canvas);
    } else if (role == QLatin1String("swatch") || role == QLatin1String("swatchrect")) {
        const bool on = active && !t.activeState.isEmpty();
        paintColorSwatch(p, t, r, theme, canvas, hovered, progress, on,
                         role == QLatin1String("swatchrect"));
    } else {
        const bool on = active && !t.activeState.isEmpty();
        const bool choice = role == QLatin1String("choice");
        const bool toggle = role == QLatin1String("toggle");
        const bool schemeChoice = choice && theme.resolveToken(t.chrome.progressColor.token, canvas)
                                  && theme.resolveToken(t.chrome.background.token, canvas);
        if (schemeChoice) {
            paintThemeCard(p, t, r, theme, glass, canvas, hovered, progress, on);
        } else {
            paintSurface(p, r, t.chrome, theme, glass, false, hovered, on, t.interactive, canvas);
            if (progress > 0.0 && t.interactive) {
                paintProgress(p, r, progress, vis.withItemFlash(fg), ProgressShape::RoundedRect,
                              radii);
            }
            QRectF content = r;
            if (choice || toggle) {
                content.adjust(0.0, 0.0, -controlMarkWidth(r), 0.0);
            }
            paintIconAndText(p, t, content, fg, theme);
            if (t.phaseIndex >= 0 && t.phases.size() > 1) {
                const int n = t.phases.size();
                const double d = 7.0;
                const double gap = 5.0;
                const double w = n * d + (n - 1) * gap;
                double x = r.center().x() - w * 0.5;
                const double y = r.bottom() - 10.0;
                for (int i = 0; i < n; ++i) {
                    const QRectF pip(x, y, d, d);
                    const bool on = i == t.phaseIndex;
                    fillRound(p, pip, d * 0.5, on ? fg : ThemeColors::mix(fg, QColor(0, 0, 0), 0.55));
                    x += d + gap;
                }
            }
            if (choice) {
                paintChoiceRadio(p, r, theme, on);
            } else if (toggle) {
                paintToggleSwitch(p, r, theme, canvas, on);
            }
        }
    }
    if (hovered && t.interactive && pv.hoverBorderWidth > 0.0 && pv.hoverBorder.isValid()) {
        strokeRound(p, r, radii, pv.hoverBorder, PageBox::all(pv.hoverBorderWidth));
    }
    if (flashing) {
        const QColor fc = vis.resolvedFlashColor(fg);
        fillRound(p, r, radii, fc);
        strokeRound(p, r, radii, fc, PageBox::all(3.5));
    }
    if (locked) {
        paintLockRadio(p, r, theme.accent.isValid() ? theme.accent : fg);
    }
}

} // namespace BoardPaint
} // namespace gazer
