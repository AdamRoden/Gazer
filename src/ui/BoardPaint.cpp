#include "ui/BoardPaint.h"

#include "ui/KeySymbols.h"
#include "ui/ProgressPaint.h"
#include "layout/RoundBox.h"
#include "ui/SliderTrack.h"

#include <QFont>
#include <QFontInfo>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QtMath>

namespace gazer {
namespace BoardPaint {

QString segoeFamily()
{
    const QFont variable(QStringLiteral("Segoe UI Variable Text"));
    if (QFontInfo(variable).family().contains(QLatin1String("Segoe UI Variable"),
                                              Qt::CaseInsensitive)) {
        return QStringLiteral("Segoe UI Variable Text");
    }
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

void paintLabel(QPainter& p, const PageTarget& t, const QRectF& r, const ThemeColors& theme)
{
    const QString family = segoeFamily();
    const QString ts = t.textStyle.toLower();
    const QRectF pad = r.adjusted(12, 6, -12, -6);
    if (pad.isEmpty()) {
        return;
    }
    if (ts == QLatin1String("section")) {
        p.setPen(Qt::NoPen);
        p.setBrush(theme.accent);
        p.drawRoundedRect(QRectF(pad.left(), pad.center().y() - 7.0, 4.0, 14.0), 2.0, 2.0);
        p.setPen(theme.textSecondary);
        p.setFont(QFont(family, 12, QFont::DemiBold));
        p.drawText(pad.adjusted(16, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, t.label.toUpper());
        return;
    }

    int titlePx = 14;
    int titleWeight = QFont::DemiBold;
    const bool valueLabel = t.clusterSlot == QLatin1String("value");
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

    QColor titleFg = t.chrome.foreground.value_or(theme.text);
    if (ts == QLatin1String("caption") && !t.chrome.foreground) {
        titleFg = theme.textSecondary;
    }
    if (t.caption.isEmpty()) {
        const int px = fontPxToFit(family, titleWeight, titlePx, 11, t.label, pad, flags);
        p.setPen(titleFg);
        p.setFont(QFont(family, px, titleWeight));
        p.drawText(pad, flags, t.label);
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
    p.setPen(titleFg);
    p.setFont(tFont);
    p.drawText(titleR, flags, t.label);
    p.setPen(theme.textSecondary);
    p.setFont(QFont(family, capPx));
    p.drawText(capR, capFlags, t.caption);
}

void paintTab(QPainter& p, const PageTarget& t, const QRectF& r, const ThemeColors& theme,
              bool hovered, bool selected, double progress)
{
    const double radius = t.chrome.resolvedRadius().first();
    if (hovered && !selected) {
        QColor fill = theme.bgSurfaceHover.isValid() ? theme.bgSurfaceHover : theme.cellHover;
        fill.setAlpha(qBound(24, fill.alpha(), 80));
        fillRound(p, r.adjusted(4, 6, -4, 8), radius, fill);
    }
    const QString family = segoeFamily();
    const QRectF textR = r.adjusted(8, 4, -8, -12);
    const int flags = int(Qt::AlignCenter | Qt::TextWordWrap);
    const int px = fontPxToFit(family, selected ? QFont::DemiBold : QFont::Normal, 14, 11, t.label,
                               textR, flags);
    p.setPen(selected ? theme.text : theme.textSecondary);
    p.setFont(QFont(family, px, selected ? QFont::DemiBold : QFont::Normal));
    p.drawText(textR, flags, t.label);
    const double tBar = selected ? 1.0 : (hovered ? progress : 0.0);
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
    const QString family = segoeFamily();
    const bool hasText = !t.label.isEmpty();
    const bool hasIcon = !t.icon.isEmpty();
    const qreal w = r.width();
    const qreal h = r.height();
    const qreal pad = qBound(6.0, qMin(w, h) * 0.08, 14.0);

    bool paintedIcon = false;
    QRectF iconR;
    QRectF textR = r.adjusted(pad, pad, -pad, -pad);
    int flags = int(Qt::AlignCenter | Qt::TextWordWrap);
    int startPx = 14;
    const int weight = QFont::DemiBold;

    if (t.clusterSlot == QLatin1String("value")) {
        p.setPen(theme.accent);
        const int px = fontPxToFit(family, QFont::DemiBold, 18, 12, t.label, textR, flags);
        p.setFont(QFont(family, px, QFont::DemiBold));
        p.drawText(textR, flags, t.label);
        return;
    }

    if (hasIcon && hasText && h >= 52.0 && w >= 40.0) {
        const qreal labelBand = qBound(18.0, h * 0.28, 30.0);
        const QRectF iconArea(r.left() + pad, r.top() + pad, w - 2.0 * pad,
                              qMax(8.0, h - 2.0 * pad - labelBand - 2.0));
        const qreal side = qMax(8.0, qMin(iconArea.width(), iconArea.height()));
        iconR = QRectF(iconArea.center().x() - side / 2.0, iconArea.center().y() - side / 2.0, side,
                       side);
        paintedIcon = KeySymbols::paint(p, t.icon, iconR, fg);
        textR = QRectF(r.left() + 6.0, r.bottom() - pad - labelBand, w - 12.0, labelBand);
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
    } else if (hasIcon) {
        const qreal side = qMax(8.0, qMin(w, h) - 2.0 * pad);
        iconR = QRectF(r.center().x() - side / 2.0, r.center().y() - side / 2.0, side, side);
        paintedIcon = KeySymbols::paint(p, t.icon, iconR, fg);
    }

    if (hasText && !(paintedIcon && textR.height() < 10.0)) {
        p.setPen(fg);
        const int px = fontPxToFit(family, weight, startPx, 10, t.label, textR, flags);
        p.setFont(QFont(family, px, weight));
        p.drawText(textR, flags, t.label);
    } else if (!paintedIcon && hasIcon) {
        p.setPen(fg);
        const int px = fontPxToFit(family, QFont::DemiBold, 14, 10, t.icon, textR, flags);
        p.setFont(QFont(family, px, QFont::DemiBold));
        p.drawText(textR, flags, t.icon);
    }
}

QColor opaqueFill(const QColor& c, const QColor& fallback)
{
    QColor out = (c.isValid() && c.alpha() > 0) ? c : fallback;
    if (!out.isValid() || out.alpha() <= 0) {
        out = QColor(10, 10, 11);
    }
    out.setAlpha(255);
    return out;
}

void paintSurface(QPainter& p, const QRectF& r, const PageChrome& chrome, const ThemeColors& theme,
                  GlassBackdrop* glass, bool grid, bool hovered, bool active, bool interactive,
                  bool clustered)
{
    if (r.isEmpty()) {
        return;
    }
    const PageBox radii = chrome.resolvedRadius(clustered);
    const QColor themeBase = opaqueFill(grid ? theme.bgMain : theme.cellBg,
                                        grid ? QColor(10, 10, 11) : QColor(26, 27, 28));
    // Authored colors keep their alpha. Unset chrome still uses an opaque theme fill
    // so a board without a background stays a solid overlay.
    const bool authoredFill =
        chrome.background && chrome.background->isValid() && chrome.background->alpha() > 0;
    const bool authoredThickness = chrome.thickness.has_value();
    QColor bg = chrome.background.value_or(themeBase);
    QColor border = chrome.borderColor.value_or(theme.border);
    PageBox thickness = chrome.resolvedThickness();

    if (active) {
        bg = theme.cellActive;
        border = theme.accentHover;
        if (thickness.first() < 1.5) {
            thickness = PageBox::all(1.5);
        }
    } else if (hovered && interactive) {
        if (authoredFill) {
            bg = bg.lighter(114);
        } else {
            bg = theme.cellHover.isValid() ? theme.cellHover : themeBase.lighter(118);
        }
        if (thickness.first() <= 0.0) {
            thickness = PageBox::all(1.0);
            if (!chrome.borderColor) {
                border = theme.border;
                border.setAlpha(qBound(40, border.alpha(), 120));
            }
        }
    } else if (!grid && !chrome.background && !authoredThickness && thickness.first() <= 0.0) {
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

void paintTarget(QPainter& p, const PageTarget& t, const QRectF& r, const ThemeColors& theme,
                 GlassBackdrop* glass, bool hovered, double progress, bool flashing, bool active,
                 const ProgressVisuals& pv, const QColor& previewColor, const QString& sliderScrubId,
                 double sliderScrubT, const QString& sliderScrubValue, double sliderScrubProgress,
                 bool locked)
{
    if (r.isEmpty()) {
        return;
    }
    const bool clustered = !t.cluster.isEmpty();
    const PageBox radii = t.chrome.resolvedRadius(clustered);
    const double radius = radii.first();
    ProgressVisuals vis = pv;
    if (t.chrome.progressStyle) {
        vis.applyStyle(*t.chrome.progressStyle);
    }
    if (t.chrome.progressColor && t.chrome.progressColor->isValid()) {
        vis.progressColor = *t.chrome.progressColor;
        vis.borderColor = *t.chrome.progressColor;
    }
    const QColor fg = (active && !t.activeState.isEmpty())
                          ? t.chrome.foreground.value_or(theme.accent)
                          : t.chrome.foreground.value_or(theme.text);
    if (t.role == QLatin1String("slider")) {
        paintSurface(p, r, t.chrome, theme, glass, false, false, false, false, clustered);
        const QString channel = t.caption.isEmpty() ? t.id : t.caption;
        const bool scrubbing =
            !sliderScrubId.isEmpty()
            && (sessionKey(t) == sliderScrubId || t.id == sliderScrubId
                || t.id.endsWith(QLatin1Char('/') + sliderScrubId));
        SliderTrack::paint(p, r, theme, vis, previewColor, channel, t.label, hovered, progress,
                           scrubbing, sliderScrubT, sliderScrubValue, sliderScrubProgress);
    } else if (t.role == QLatin1String("preview")) {
        SliderTrack::paintPreview(p, r, radius, previewColor);
        paintLabel(p, t, r, theme);
    } else if (t.role == QLatin1String("tab")) {
        paintTab(p, t, r, theme, hovered, active || !t.interactive, progress);
    } else if (t.role == QLatin1String("label")) {
        paintSurface(p, r, t.chrome, theme, glass, false, false, false, false, clustered);
        paintLabel(p, t, r, theme);
    } else {
        paintSurface(p, r, t.chrome, theme, glass, false, hovered,
                     active && !t.activeState.isEmpty(), t.interactive, clustered);
        paintIconAndText(p, t, r, fg, theme);
        if (hovered && progress > 0.0 && t.interactive) {
            paintProgress(p, r, progress, vis.withItemFlash(fg), ProgressShape::RoundedRect, radii);
        }
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
