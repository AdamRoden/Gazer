#include "ui/LayoutBoardPainter.h"

#include "ui/LayoutQuickWindow.h"
#include "ui/GlassBackdrop.h"

#include "layout/LayoutGeometry.h"
#include "layout/LayoutVisibility.h"
#include "ui/MouseIcons.h"

#include <QFont>
#include <QFontMetricsF>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QSet>

namespace gazer {

namespace {

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

} // namespace


LayoutBoardPainter::LayoutBoardPainter(LayoutQuickWindow& host)
    : m_layout(host.m_layout)
    , m_itemLocalRects(host.m_itemLocalRects)
    , m_hoverId(host.m_hoverId)
    , m_hoverProgress(host.m_hoverProgress)
    , m_progressVisuals(host.m_progressVisuals)
    , m_theme(host.m_theme)
    , m_activeItemIds(host.m_activeItemIds)
    , m_props(host.m_props)
    , m_flashId(host.m_flashId)
    , m_boardOpacity(host.m_boardOpacity)
    , m_previewColor(host.m_previewColor)
    , m_sliderScrubId(host.m_sliderScrubId)
    , m_sliderScrubT(host.m_sliderScrubT)
    , m_sliderScrubValue(host.m_sliderScrubValue)
    , m_sliderScrubProgress(host.m_sliderScrubProgress)
    , m_sliderReadoutT(host.m_sliderReadoutT)
    , m_sliderReadoutValue(host.m_sliderReadoutValue)
    , m_glass(host.m_glass)
    , m_width(host.width())
    , m_height(host.height())
{
}

bool LayoutBoardPainter::itemShown(const LayoutItem& item) const
{
    return itemIsShown(item, m_props);
}

LayoutItemStyle LayoutBoardPainter::resolvedItemStyle(const LayoutItem& item) const
{
    return m_layout.style.withOverrides(item.style);
}

ProgressVisuals LayoutBoardPainter::visualsForItem(const LayoutItem* item) const
{
    ProgressVisuals v = m_progressVisuals;
    if (m_layout.dwell.sectionPresent) {
        v = v.mergedWith(m_layout.dwell);
    }
    if (item && item->dwell.sectionPresent) {
        v = v.mergedWith(item->dwell);
    }
    if (item) {
        const QColor fg = resolvedItemStyle(*item).foreground.value_or(m_theme.text);
        return v.withItemFlash(fg);
    }
    return v;
}

void LayoutBoardPainter::fillChrome(QPainter& p, const QRectF& r, double radius, const QColor& bg,
                                   const LayoutChromeStyle& st, const QColor& bgBot)
{
    if (st.hasBlur()) {
        m_glass.paint(p, r, radius, st.background ? bg : QColor());
        return;
    }
    if (bgBot.isValid() && (bg.alpha() > 0 || bgBot.alpha() > 0)) {
        QLinearGradient g(r.topLeft(), r.bottomLeft());
        g.setColorAt(0.0, bg);
        g.setColorAt(1.0, bgBot);
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawRoundedRect(r, radius, radius);
        return;
    }
    if (bg.alpha() <= 0) {
        return;
    }
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(r, radius, radius);
}

void LayoutBoardPainter::paintProgressChrome(QPainter& p, const QRectF& r, bool hovered,
                                            double progress, const ProgressVisuals& visuals,
                                            double radius)
{
    const bool flashing = !m_flashId.isEmpty() && m_itemLocalRects.contains(m_flashId)
                          && m_itemLocalRects.value(m_flashId) == r;

    if (flashing) {
        p.setPen(QPen(visuals.flashColor, 3.5));
        p.setBrush(visuals.flashColor);
        p.drawRoundedRect(r, radius, radius);
    }

    if (!hovered || progress <= 0.0) {
        return;
    }
    paintProgress(p, r, progress, visuals, ProgressShape::RoundedRect, radius);
}

void LayoutBoardPainter::paintPreviewSwatch(QPainter& p, const QRectF& r, double radius)
{
    QPainterPath clip;
    clip.addRoundedRect(r, radius, radius);
    p.save();
    p.setClipPath(clip);
    const int cell = 10;
    for (int y = int(r.top()); y < int(r.bottom()); y += cell) {
        for (int x = int(r.left()); x < int(r.right()); x += cell) {
            const bool lite = ((x / cell) + (y / cell)) % 2 == 0;
            p.fillRect(x, y, cell, cell, lite ? QColor(200, 200, 200) : QColor(140, 140, 140));
        }
    }
    p.setPen(Qt::NoPen);
    p.setBrush(m_previewColor);
    p.drawRoundedRect(r, radius, radius);
    p.restore();
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 80), 1.5));
    p.drawRoundedRect(r.adjusted(1, 1, -1, -1), radius, radius);
}

void LayoutBoardPainter::paintRadioButton(QPainter& p, const QRectF& cell, bool on)
{
    const qreal d = 18.0;
    const qreal m = 8.0;
    const QRectF outer(cell.right() - m - d, cell.top() + m, d, d);
    p.setPen(QPen(on ? m_theme.accent : m_theme.border, 2.0));
    p.setBrush(on ? QColor(m_theme.accent.red(), m_theme.accent.green(), m_theme.accent.blue(), 40)
                  : m_theme.bgMain);
    p.drawEllipse(outer);
    if (on) {
        p.setPen(Qt::NoPen);
        p.setBrush(m_theme.accent);
        p.drawEllipse(outer.adjusted(4.5, 4.5, -4.5, -4.5));
    }
}

namespace {

struct SliderChannelInfo {
    QString name;
    QString value;
    double t = 0.0;
};

SliderChannelInfo sliderChannelInfo(const QString& channel, const QColor& color)
{
    QColor c = color.isValid() ? color : ThemeColors::defaultProgressColor();
    int h = 0, s = 0, v = 0, a = 255;
    c.getHsv(&h, &s, &v, &a);
    if (h < 0) {
        h = 0;
    }
    const QString ch = channel.toLower();
    SliderChannelInfo info;
    if (ch == QLatin1String("h") || ch == QLatin1String("hue")) {
        info.name = QStringLiteral("Hue");
        info.value = QString::number(h);
        info.t = h / 359.0;
    } else if (ch == QLatin1String("s") || ch == QLatin1String("sat")) {
        const int pct = qBound(0, qRound(s / 2.55), 100);
        info.name = QStringLiteral("Saturation");
        info.value = QStringLiteral("%1%").arg(pct);
        info.t = s / 255.0;
    } else if (ch == QLatin1String("v") || ch == QLatin1String("val")) {
        const int pct = qBound(0, qRound(v / 2.55), 100);
        info.name = QStringLiteral("Value");
        info.value = QStringLiteral("%1%").arg(pct);
        info.t = v / 255.0;
    } else if (ch == QLatin1String("r") || ch == QLatin1String("red")) {
        info.name = QStringLiteral("Red");
        info.value = QString::number(c.red());
        info.t = c.red() / 255.0;
    } else if (ch == QLatin1String("g") || ch == QLatin1String("green")) {
        info.name = QStringLiteral("Green");
        info.value = QString::number(c.green());
        info.t = c.green() / 255.0;
    } else if (ch == QLatin1String("b") || ch == QLatin1String("blue")) {
        info.name = QStringLiteral("Blue");
        info.value = QString::number(c.blue());
        info.t = c.blue() / 255.0;
    } else {
        const int pct = qBound(0, qRound(c.alpha() / 2.55), 100);
        info.name = QStringLiteral("Alpha");
        info.value = QStringLiteral("%1%").arg(pct);
        info.t = c.alpha() / 255.0;
    }
    info.t = qBound(0.0, info.t, 1.0);
    return info;
}

} // namespace

void LayoutBoardPainter::paintSliderTrack(QPainter& p, const QRectF& r, const LayoutItem& item,
                                         bool hovered, double progress)
{
    const QString channel = item.caption.isEmpty() ? item.id : item.caption;
    const bool scrubbing = (item.id == m_sliderScrubId);
    const LayoutQuickWindow::SliderVisual geom = LayoutQuickWindow::sliderVisual(r, scrubbing);
    QColor base = m_previewColor.isValid() ? m_previewColor : ThemeColors::defaultProgressColor();
    int h = 0, s = 0, v = 0, a = 255;
    base.getHsv(&h, &s, &v, &a);
    if (h < 0) {
        h = 0;
    }
    const int cr = base.red();
    const int cg = base.green();
    const int cb = base.blue();
    const SliderChannelInfo info = sliderChannelInfo(channel, base);
    double t = scrubbing ? m_sliderScrubT : info.t;
    QString valueText = scrubbing && !m_sliderScrubValue.isEmpty() ? m_sliderScrubValue
                                                                  : info.value;
    if (!scrubbing && m_sliderReadoutT.contains(item.id)) {
        t = m_sliderReadoutT.value(item.id);
        valueText = m_sliderReadoutValue.value(item.id, valueText);
    }
    const QString nameText = item.label.isEmpty() ? info.name : item.label;

    const QString ch = channel.toLower();
    const QRectF track = geom.track;
    const double radius = track.height() * 0.5;

    if (ch == QLatin1String("a") || ch == QLatin1String("alpha")) {
        QPainterPath clip;
        clip.addRoundedRect(track, radius, radius);
        p.save();
        p.setClipPath(clip);
        const int cell = 6;
        for (int y = int(track.top()); y < int(track.bottom()); y += cell) {
            for (int x = int(track.left()); x < int(track.right()); x += cell) {
                const bool lite = ((x / cell) + (y / cell)) % 2 == 0;
                p.fillRect(x, y, cell, cell, lite ? QColor(210, 210, 210) : QColor(150, 150, 150));
            }
        }
        p.restore();
    }

    QLinearGradient g(track.left(), track.center().y(), track.right(), track.center().y());
    if (ch == QLatin1String("h") || ch == QLatin1String("hue")) {
        for (int i = 0; i <= 6; ++i) {
            g.setColorAt(i / 6.0, QColor::fromHsv(qMin(359, i * 60), 255, 255));
        }
    } else if (ch == QLatin1String("s") || ch == QLatin1String("sat")) {
        g.setColorAt(0.0, QColor::fromHsv(h, 0, v));
        g.setColorAt(1.0, QColor::fromHsv(h, 255, v));
    } else if (ch == QLatin1String("v") || ch == QLatin1String("val")) {
        g.setColorAt(0.0, QColor::fromHsv(h, s, 0));
        g.setColorAt(1.0, QColor::fromHsv(h, s, 255));
    } else if (ch == QLatin1String("r") || ch == QLatin1String("red")) {
        g.setColorAt(0.0, QColor(0, cg, cb));
        g.setColorAt(1.0, QColor(255, cg, cb));
    } else if (ch == QLatin1String("g") || ch == QLatin1String("green")) {
        g.setColorAt(0.0, QColor(cr, 0, cb));
        g.setColorAt(1.0, QColor(cr, 255, cb));
    } else if (ch == QLatin1String("b") || ch == QLatin1String("blue")) {
        g.setColorAt(0.0, QColor(cr, cg, 0));
        g.setColorAt(1.0, QColor(cr, cg, 255));
    } else if (ch == QLatin1String("contrast")) {
        g.setColorAt(0.0, m_theme.bgSurface);
        g.setColorAt(0.5, m_theme.cellHover);
        g.setColorAt(1.0, m_theme.accent);
    } else if (ch == QLatin1String("brightness")) {
        g.setColorAt(0.0, QColor(20, 20, 22));
        g.setColorAt(1.0, QColor(240, 240, 242));
    } else {
        QColor clear = base;
        clear.setAlpha(0);
        QColor solid = base;
        solid.setAlpha(255);
        g.setColorAt(0.0, clear);
        g.setColorAt(1.0, solid);
    }
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawRoundedRect(track, radius, radius);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 70), 1.1));
    p.drawRoundedRect(track.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);

    p.setPen(m_theme.text);
    p.setFont(QFont(QStringLiteral("Segoe UI"), 10, QFont::DemiBold));
    p.drawText(geom.header, Qt::AlignLeft | Qt::AlignVCenter, nameText);
    p.setPen(m_theme.textSecondary);
    p.drawText(geom.header, Qt::AlignRight | Qt::AlignVCenter, valueText);

    const QPointF pos = geom.posAt(t);
    const double ringD = geom.ringDiameter;
    const QRectF ring(pos.x() - ringD * 0.5, pos.y() - ringD * 0.5, ringD, ringD);
    p.setBrush(m_theme.bgMain);
    p.setPen(QPen(Qt::white, scrubbing ? 2.4 : 2.0));
    p.drawEllipse(ring);
    p.setPen(QPen(QColor(0, 0, 0, 90), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(ring.adjusted(1.5, 1.5, -1.5, -1.5));

    const double ringProgress = scrubbing && m_sliderScrubProgress > 0.0 ? m_sliderScrubProgress
                                                                        : (hovered ? progress : 0.0);
    if (scrubbing) {
        p.setPen(m_theme.text);
        p.setFont(QFont(QStringLiteral("Segoe UI Semibold"), 10, QFont::DemiBold));
        p.drawText(ring, Qt::AlignCenter, valueText);
    }
    if (ringProgress > 0.01) {
        paintProgress(p, ring.adjusted(-3, -3, 3, 3), ringProgress, visualsForItem(&item),
                      ProgressShape::Ellipse);
    }
}

void LayoutBoardPainter::paintCell(QPainter& p, const LayoutItem& item, const QRectF& r, bool onCard)
{
    const LayoutItemStyle st = resolvedItemStyle(item);
    const double radius = st.radius.value_or(14.0);
    if (item.kind == LayoutItemKind::Slider) {
        const bool hovered = item.interactive && (item.id == m_hoverId);
        paintSliderTrack(p, r, item, hovered, hovered ? m_hoverProgress : 0.0);
        return;
    }
    if (item.kind == LayoutItemKind::Preview) {
        paintPreviewSwatch(p, r, radius);
        if (!item.label.isEmpty()) {
            p.setPen(ThemeColors::contrastOn(m_previewColor));
            p.setFont(QFont(QStringLiteral("Segoe UI"), 11, QFont::DemiBold));
            p.drawText(r.adjusted(8, 6, -8, -6), Qt::AlignLeft | Qt::AlignTop, item.label);
            if (!item.caption.isEmpty()) {
                p.setFont(QFont(QStringLiteral("Segoe UI"), 9));
                p.drawText(r.adjusted(8, 24, -8, -6), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                           item.caption);
            }
        }
        return;
    }

    const bool hovered = item.interactive && (item.id == m_hoverId);
    const bool active = m_activeItemIds.contains(item.id);
    const bool hasSwitch = !item.activeState.isEmpty();
    QColor bg = st.background.value_or(QColor());
    QColor fg = st.foreground.value_or(m_theme.text);
    QColor border = st.borderColor.value_or(m_theme.border);
    double borderW = st.borderWidth.value_or(0.0);

    if (!st.background) {
        if (active && !hasSwitch) {
            bg = m_theme.cellActive;
            fg = m_theme.accent;
        } else if (hovered) {
            bg = m_theme.cellHover;
        } else if (!onCard) {
            bg = m_theme.cellBg;
        }
    } else if (active && !hasSwitch) {
        bg = m_theme.cellActive;
        fg = m_theme.accent;
    }
    if (hovered && bg.alpha() > 0 && st.background) {
        bg = bg.lighter(118);
    }
    if (!st.borderWidth && (hovered || (active && !hasSwitch))) {
        borderW = 1.0;
        if (active && !hasSwitch && !st.borderColor) {
            border = m_theme.accentHover;
        }
    }

    fillChrome(p, r, radius, bg, st);
    if (borderW > 0.0 && border.alpha() > 0) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(border, borderW));
        p.drawRoundedRect(r, radius, radius);
    }

    paintProgressChrome(p, r, hovered, m_hoverProgress, visualsForItem(&item), radius);

    p.setPen(fg);
    if (!item.icon.isEmpty()) {
        const QRectF iconR(r.left() + 6, r.top() + 6, r.width() - 12, r.height() * 0.52);
        MouseIcons::paint(p, item.icon, iconR, fg);
        p.setFont(QFont(QStringLiteral("Segoe UI"), 11, QFont::DemiBold));
        p.drawText(r.adjusted(8, r.height() * 0.52, -8, -6),
                   Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, item.label);
    } else {
        p.setFont(QFont(QStringLiteral("Segoe UI"), 13, QFont::DemiBold));
        const QString text =
            item.caption.isEmpty() ? item.label
                                   : QStringLiteral("%1\n%2").arg(item.label, item.caption);
        p.drawText(r.adjusted(8, 8, -8, -8), Qt::AlignCenter | Qt::TextWordWrap, text);
    }

    if (hasSwitch) {
        paintRadioButton(p, r, active);
    }
}

void LayoutBoardPainter::paintStaticItem(QPainter& p, const LayoutItem& item, const QRectF& r)
{
    const LayoutItemStyle st = resolvedItemStyle(item);
    const double radius = st.radius.value_or(4.0);
    if (st.background && st.background->isValid() && st.background->alpha() > 0) {
        fillChrome(p, r, radius, *st.background, st);
        if (st.borderWidth.value_or(0.0) > 0.0) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(st.borderColor.value_or(m_theme.border), st.borderWidth.value_or(1.0)));
            p.drawRoundedRect(r, radius, radius);
        }
    }

    const QString ts = item.textStyle.toLower();
    int titlePx = 14;
    int titleWeight = QFont::DemiBold;
    if (ts == QLatin1String("caption")) {
        titlePx = 12;
        titleWeight = QFont::Normal;
    } else if (ts == QLatin1String("body")) {
        titlePx = 14;
        titleWeight = QFont::Normal;
    } else if (ts == QLatin1String("subtitle")) {
        titlePx = 20;
        titleWeight = QFont::DemiBold;
    } else if (ts == QLatin1String("title")) {
        titlePx = 22;
        titleWeight = QFont::DemiBold;
    } else if (ts == QLatin1String("section")) {
        QColor bar = m_theme.accent;
        p.setPen(Qt::NoPen);
        p.setBrush(bar);
        p.drawRoundedRect(QRectF(r.left(), r.center().y() - 7.0, 4.0, 14.0), 2.0, 2.0);
        p.setPen(m_theme.textSecondary);
        p.setFont(QFont(QStringLiteral("Segoe UI"), 12, QFont::DemiBold));
        p.drawText(r.adjusted(16, 0, -8, -6), Qt::AlignLeft | Qt::AlignVCenter,
                   item.label.toUpper());
        QColor rule = m_theme.border;
        p.setPen(QPen(rule, 1.0));
        const double y = r.bottom() - 4.0;
        p.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
        return;
    }

    const bool readout = !item.settingKey.isEmpty() && item.caption.isEmpty();
    const QRectF pad = r.adjusted(10, 6, -10, -6);
    p.save();
    p.setClipRect(pad);
    const QString family = QStringLiteral("Segoe UI");
    if (readout) {
        const int flags = int(Qt::AlignCenter | Qt::TextWordWrap);
        const int px = fontPxToFit(family, QFont::DemiBold, ts.isEmpty() ? 16 : titlePx, 10,
                                   item.label, pad, flags);
        p.setPen(st.foreground.value_or(m_theme.accent));
        p.setFont(QFont(family, px, QFont::DemiBold));
        p.drawText(pad, flags, item.label);
        p.restore();
        return;
    }

    QColor titleFg = st.foreground.value_or(m_theme.text);
    if (ts == QLatin1String("caption") && !st.foreground) {
        titleFg = m_theme.textSecondary;
    }
    if (item.caption.isEmpty()) {
        const int flags = (ts == QLatin1String("title") || ts == QLatin1String("subtitle"))
                              ? int(Qt::AlignCenter | Qt::TextWordWrap)
                              : int(Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap);
        const int px = fontPxToFit(family, titleWeight, titlePx, 10, item.label, pad, flags);
        p.setPen(titleFg);
        p.setFont(QFont(family, px, titleWeight));
        p.drawText(pad, flags, item.label);
        p.restore();
        return;
    }

    const int titleFlags = int(Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap);
    const int capFlags = int(Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap);
    int tPx = titlePx;
    QFont tFont(family, tPx, titleWeight);
    auto titleH = [&]() {
        return QFontMetricsF(tFont).boundingRect(pad, titleFlags, item.label).height();
    };
    auto capLine = [&]() { return QFontMetricsF(QFont(family, 12, QFont::Normal)).lineSpacing(); };
    while (tPx > 10 && titleH() + capLine() + 2.0 > pad.height()) {
        --tPx;
        tFont.setPixelSize(tPx);
    }
    const double th = qBound(QFontMetricsF(tFont).lineSpacing(), titleH(),
                             qMax(QFontMetricsF(tFont).lineSpacing(), pad.height() - capLine() - 2.0));
    const QRectF titleR(pad.left(), pad.top(), pad.width(), th);
    const QRectF capR(pad.left(), titleR.bottom() + 2.0, pad.width(),
                      qMax(1.0, pad.bottom() - titleR.bottom() - 2.0));
    const int capPx = fontPxToFit(family, QFont::Normal, 12, 9, item.caption, capR, capFlags);
    p.setPen(titleFg);
    p.setFont(tFont);
    p.drawText(titleR, titleFlags, item.label);
    p.setPen(m_theme.textSecondary);
    p.setFont(QFont(family, capPx, QFont::Normal));
    p.drawText(capR, capFlags, item.caption);
    p.restore();
}

void LayoutBoardPainter::paintTab(QPainter& p, const LayoutItem& item, const QRectF& r)
{
    const bool selected = item.action.layoutId == m_layout.id
                          || (item.action.type == LayoutAction::Type::Unknown
                              && item.actions.isEmpty());
    const bool hovered = item.interactive && item.id == m_hoverId;
    const double radius = resolvedItemStyle(item).radius.value_or(4.0);

    if (hovered && !selected) {
        QColor fill = m_theme.bgSurfaceHover;
        if (!fill.isValid() || fill.alpha() == 0) {
            fill = m_theme.cellHover;
        }
        fill.setAlpha(qBound(20, fill.alpha(), 70));
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawRoundedRect(r.adjusted(4, 6, -4, 6), radius, radius);
    }

    p.setPen(selected ? m_theme.text : m_theme.textSecondary);
    p.setFont(QFont(QStringLiteral("Segoe UI"), 14, selected ? QFont::DemiBold : QFont::Normal));
    p.drawText(r.adjusted(8, 4, -8, -10), Qt::AlignCenter | Qt::TextWordWrap, item.label);

    const double t = selected ? 1.0 : (hovered ? m_hoverProgress : 0.0);
    if (t > 0.01) {
        const double maxW = qMax(24.0, r.width() - 48.0);
        const double barW = maxW * t;
        const QRectF bar(r.center().x() - barW * 0.5, r.bottom() - 7.0, barW, selected ? 3.0 : 2.0);
        p.setPen(Qt::NoPen);
        p.setBrush(m_theme.accent);
        p.drawRoundedRect(bar, 1.5, 1.5);
    }
}

void LayoutBoardPainter::paintCard(QPainter& p, const QRectF& r)
{
    QColor fill = m_theme.cellBg;
    fill.setAlpha(255);
    p.setPen(QPen(m_theme.border, 1.0));
    p.setBrush(fill);
    p.drawRoundedRect(r, 8.0, 8.0);
}

QRectF LayoutBoardPainter::toggleTrackRect(const QRectF& cell)
{
    const double edge = 16.0;
    const double maxW = qMax(0.0, cell.width() - edge);
    const double maxH = qMax(0.0, cell.height() - 8.0);
    const double trackW = qMin(104.0, maxW);
    const double trackH = qMin(48.0, maxH);
    return {cell.right() - edge - trackW, cell.center().y() - trackH * 0.5, trackW, trackH};
}

QRectF LayoutBoardPainter::toggleHitRect(const QRectF& cell)
{
    return toggleTrackRect(cell).adjusted(-10.0, -10.0, 10.0, 10.0).intersected(cell);
}

void LayoutBoardPainter::paintToggle(QPainter& p, const LayoutItem& item, const QRectF& r)
{
    const bool hovered = item.interactive && item.id == m_hoverId;
    const bool on = m_activeItemIds.contains(item.id);
    const QRectF track = toggleTrackRect(r);

    QColor trackFill = on ? m_theme.accent : m_theme.bgSurfaceActive;
    p.setPen(QPen(on ? m_theme.accentHover : m_theme.border, hovered ? 2.0 : 1.0));
    p.setBrush(trackFill);
    p.drawRoundedRect(track, track.height() * 0.5, track.height() * 0.5);

    const double thumb = qBound(8.0, track.height() - 8.0, 40.0);
    const double inset = 4.0;
    const double tx = on ? track.right() - inset - thumb : track.left() + inset;
    const QRectF knob(tx, track.center().y() - thumb * 0.5, thumb, thumb);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    p.drawEllipse(knob);

    QRectF textR = r.adjusted(12, 6, -(r.right() - track.left()) - 8.0, -6);
    p.setPen(m_theme.text);
    if (!item.icon.isEmpty()) {
        const QRectF iconR(textR.left(), textR.center().y() - 14.0, 28.0, 28.0);
        MouseIcons::paint(p, item.icon, iconR, m_theme.text);
        textR.setLeft(iconR.right() + 8.0);
    }
    p.setFont(QFont(QStringLiteral("Segoe UI"), 13, QFont::DemiBold));
    if (item.caption.isEmpty()) {
        p.drawText(textR, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, item.label);
    } else {
        const QRectF titleR(textR.left(), textR.top(), textR.width(), textR.height() * 0.5);
        p.drawText(titleR, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, item.label);
        p.setPen(m_theme.textSecondary);
        p.setFont(QFont(QStringLiteral("Segoe UI"), 11));
        p.drawText(QRectF(textR.left(), titleR.bottom(), textR.width(), textR.bottom() - titleR.bottom()),
                   Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, item.caption);
    }

    if (hovered && m_hoverProgress > 0.0) {
        paintProgress(p, track.adjusted(-4, -4, 4, 4), m_hoverProgress, visualsForItem(&item),
                      ProgressShape::RoundedRect, track.height() * 0.5);
    }
}

QRectF LayoutBoardPainter::clusterBounds(const QString& cluster) const
{
    QRectF bounds;
    if (cluster.isEmpty()) {
        return bounds;
    }
    for (const LayoutItem& item : m_layout.items) {
        if (item.cluster != cluster || !item.isClustered() || !itemShown(item)) {
            continue;
        }
        const QRectF r = m_itemLocalRects.value(item.id);
        if (!r.isEmpty()) {
            bounds = bounds.isEmpty() ? r : bounds.united(r);
        }
    }
    return bounds;
}

void LayoutBoardPainter::paintClusterFrame(QPainter& p, const QString& cluster)
{
    const QRectF bounds = clusterBounds(cluster);
    if (bounds.isEmpty()) {
        return;
    }
    bool stepper = false;
    for (const LayoutItem& item : m_layout.items) {
        if (item.cluster == cluster && item.kind == LayoutItemKind::Stepper) {
            stepper = true;
            break;
        }
    }
    p.setPen(QPen(m_theme.border, 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(bounds, stepper ? 4.0 : 6.0, stepper ? 4.0 : 6.0);
}

void LayoutBoardPainter::paintClusterSlot(QPainter& p, const LayoutItem& item, const QRectF& r)
{
    const bool hovered = item.interactive && item.id == m_hoverId;
    const bool selected = m_activeItemIds.contains(item.id);
    const bool stepper = item.kind == LayoutItemKind::Stepper;
    const QString slot = item.clusterSlot;

    if (hovered || (selected && !stepper)) {
        QColor fill = selected ? m_theme.accent : m_theme.cellHover;
        if (selected) {
            fill.setAlpha(50);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawRoundedRect(r.adjusted(2, 2, -2, -2), 3.0, 3.0);
    }

    QColor fg = m_theme.text;
    if (selected && !stepper) {
        fg = m_theme.accent;
    }
    if (slot == QLatin1String("value")) {
        fg = m_theme.accent;
    }
    p.setPen(fg);
    if (slot == QLatin1String("value")) {
        p.setFont(QFont(QStringLiteral("Segoe UI"), 15, QFont::DemiBold));
        p.drawText(r.adjusted(4, 2, -4, -2), Qt::AlignCenter | Qt::TextWordWrap, item.label);
    } else {
        p.setFont(QFont(QStringLiteral("Segoe UI"), stepper ? 16 : 13,
                        stepper ? QFont::Normal : QFont::DemiBold));
        p.drawText(r.adjusted(6, 4, -6, -4), Qt::AlignCenter | Qt::TextWordWrap, item.label);
    }

    if (hovered && m_hoverProgress > 0.0) {
        paintProgressChrome(p, r.adjusted(2, 2, -2, -2), true, m_hoverProgress,
                            visualsForItem(&item), 3.0);
    }
}

void LayoutBoardPainter::paint(QPainter& p)
{
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.setOpacity(m_boardOpacity);

    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(QRect(0, 0, m_width, m_height), Qt::transparent);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    bool hasTabs = false;
    bool hasTitleItem = false;
    for (const LayoutItem& item : m_layout.items) {
        if (item.kind == LayoutItemKind::Tab) {
            hasTabs = true;
        }
        if (item.kind == LayoutItemKind::Label
            && item.textStyle.compare(QLatin1String("title"), Qt::CaseInsensitive) == 0) {
            hasTitleItem = true;
        }
    }

    const auto& ws = m_layout.placement.style;
    const double winR = ws.radius.value_or(12.0);
    const double winBw = ws.borderWidth.value_or(0.0);
    QColor bgTop = ws.background.value_or(m_theme.bgSurface);
    QColor bgBot = ws.background ? QColor() : m_theme.bgMain;
    QColor border = ws.borderColor.value_or(m_theme.border);

    fillChrome(p, QRectF(0, 0, m_width, m_height), winR, bgTop, ws, bgBot);
    if (border.alpha() > 0 && winBw > 0) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(border, winBw));
        p.drawRoundedRect(QRectF(0, 0, m_width, m_height).adjusted(1.5, 1.5, -1.5, -1.5), winR,
                          winR);
    }

    if (!hasTitleItem && m_layout.grid.marginPx >= 42
        && (!m_layout.name.isEmpty() || !m_layout.description.isEmpty())) {
        p.setFont(QFont(QStringLiteral("Segoe UI"), 16, QFont::DemiBold));
        p.setPen(m_theme.text);
        p.drawText(QRect(24, 14, m_width - 48, 28), Qt::AlignLeft | Qt::AlignVCenter,
                   m_layout.name);
        if (!m_layout.description.isEmpty()) {
            p.setFont(QFont(QStringLiteral("Segoe UI"), 10));
            p.setPen(m_theme.textSecondary);
            p.drawText(QRect(24, 40, m_width - 48, 22), Qt::AlignLeft | Qt::AlignVCenter,
                       m_layout.description);
        }
    }

    if (!m_layout.isValid()) {
        p.setPen(m_theme.danger);
        p.drawText(QRect(0, 0, m_width, m_height), Qt::AlignCenter,
                   QStringLiteral("No layout loaded"));
        return;
    }

    QRectF tabBand;
    QSet<QString> clusterFrames;
    QHash<int, QRectF> rowBands;
    QHash<int, int> rowToolbarButtons;
    QHash<int, bool> rowCardAnchor;
    for (const LayoutItem& item : m_layout.items) {
        if (!itemShown(item) || item.isPageChrome() || !item.participatesInBoardGrid()) {
            continue;
        }
        const QRectF r = m_itemLocalRects.value(item.id);
        if (r.isEmpty()) {
            continue;
        }
        auto& band = rowBands[item.row];
        band = band.isEmpty() ? r : band.united(r);
        if (item.kind == LayoutItemKind::Toggle) {
            rowCardAnchor[item.row] = true;
        } else if (item.kind == LayoutItemKind::Label && !item.isClustered()) {
            const auto bg = item.style.background;
            if (!bg || !bg->isValid() || bg->alpha() == 0) {
                rowCardAnchor[item.row] = true;
            }
        } else if (item.kind == LayoutItemKind::Button) {
            rowToolbarButtons[item.row] += 1;
        }
    }
    QSet<int> cardRows;
    const auto inset = m_layout.grid.insets(m_width, m_height);
    for (auto it = rowBands.begin(); it != rowBands.end(); ++it) {
        const int row = it.key();
        if (!rowCardAnchor.value(row) || rowToolbarButtons.value(row) >= 3) {
            continue;
        }
        cardRows.insert(row);
        QRectF band = it.value();
        band.setLeft(inset.left);
        band.setRight(m_width - inset.right);
        paintCard(p, band);
    }
    for (const LayoutItem& item : m_layout.items) {
        if (!itemShown(item)) {
            continue;
        }
        const QRectF r = m_itemLocalRects.value(item.id);
        if (r.isEmpty()) {
            continue;
        }

        if (item.kind == LayoutItemKind::Tab) {
            tabBand = tabBand.united(r);
            paintTab(p, item, r);
            continue;
        }
        if (item.isClustered()) {
            if (!clusterFrames.contains(item.cluster)) {
                paintClusterFrame(p, item.cluster);
                clusterFrames.insert(item.cluster);
            }
            paintClusterSlot(p, item, r);
            continue;
        }
        if (item.kind == LayoutItemKind::Toggle) {
            paintToggle(p, item, r);
        } else if (item.kind == LayoutItemKind::Label) {
            paintStaticItem(p, item, r);
        } else {
            paintCell(p, item, r, cardRows.contains(item.row));
        }
    }

    if (hasTabs && !tabBand.isEmpty()) {
        const double y = tabBand.bottom() + 1.0;
        QColor rule = m_theme.border;
        rule.setAlpha(qBound(40, rule.alpha(), 90));
        p.setPen(QPen(rule, 1.0));
        const auto inset = m_layout.grid.insets(m_width, m_height);
        p.drawLine(QPointF(inset.left, y), QPointF(m_width - inset.right, y));
    }
}

} // namespace gazer
