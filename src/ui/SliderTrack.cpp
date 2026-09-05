#include "ui/SliderTrack.h"

#include "ui/ProgressPaint.h"

#include <QFont>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

namespace gazer {
namespace SliderTrack {

Visual visual(const QRectF& cell, bool scrubbing)
{
    Visual g;
    const double headerH = qBound(16.0, cell.height() * 0.30, 22.0);
    g.header = QRectF(cell.left() + 8.0, cell.top() + 2.0, qMax(1.0, cell.width() - 16.0), headerH);
    const double trackH = 40.0;
    g.ringDiameter = scrubbing ? 52.0 : 40.0;
    const double inset = g.ringDiameter * 0.5;
    const double bandTop = g.header.bottom();
    const double bandH = qMax(trackH + 10.0, cell.bottom() - bandTop);
    g.trackCy = bandTop + bandH * 0.5;
    g.track = QRectF(cell.left() + 6.0, g.trackCy - trackH * 0.5, qMax(8.0, cell.width() - 12.0),
                     trackH);
    g.valueLeft = g.track.left() + inset;
    g.valueRight = g.track.right() - inset;
    if (g.valueRight <= g.valueLeft + 1.0) {
        const double mid = g.track.center().x();
        g.valueLeft = mid - 1.0;
        g.valueRight = mid + 1.0;
    }
    return g;
}

void paintPreview(QPainter& p, const QRectF& r, double radius, const QColor& color)
{
    const int cell = 10;
    for (int y = int(r.top()); y < int(r.bottom()); y += cell) {
        for (int x = int(r.left()); x < int(r.right()); x += cell) {
            const bool lite = ((x / cell) + (y / cell)) % 2 == 0;
            p.fillRect(x, y, cell, cell, lite ? QColor(200, 200, 200) : QColor(140, 140, 140));
        }
    }
    p.setPen(Qt::NoPen);
    p.setBrush(color.isValid() ? color : ThemeColors::defaultProgressColor());
    p.drawRoundedRect(r, radius, radius);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 80), 1.5));
    p.drawRoundedRect(r.adjusted(1, 1, -1, -1), radius, radius);
}

namespace {

struct ChannelInfo {
    QString name;
    QString value;
    double t = 0.0;
};

ChannelInfo channelInfo(const QString& channel, const QColor& color)
{
    QColor c = color.isValid() ? color : ThemeColors::defaultProgressColor();
    int h = 0, s = 0, l = 0, a = 255;
    c.getHsl(&h, &s, &l, &a);
    if (h < 0) {
        h = 0;
    }
    const QString ch = channel.toLower();
    ChannelInfo info;
    if (ch == QLatin1String("h") || ch == QLatin1String("hue")) {
        info.name = QStringLiteral("Hue");
        info.value = QString::number(h);
        info.t = h / 359.0;
    } else if (ch == QLatin1String("s") || ch == QLatin1String("sat")) {
        const int pct = qBound(0, qRound(s / 2.55), 100);
        info.name = QStringLiteral("Saturation");
        info.value = QStringLiteral("%1%").arg(pct);
        info.t = s / 255.0;
    } else if (ch == QLatin1String("l") || ch == QLatin1String("light")
               || ch == QLatin1String("lightness")) {
        const int pct = qBound(0, qRound(l / 2.55), 100);
        info.name = QStringLiteral("Lightness");
        info.value = QStringLiteral("%1%").arg(pct);
        info.t = l / 255.0;
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
        info.name = ch == QLatin1String("opacity") ? QStringLiteral("Opacity")
                                                   : QStringLiteral("Alpha");
        info.value = QStringLiteral("%1%").arg(pct);
        info.t = c.alpha() / 255.0;
    }
    info.t = qBound(0.0, info.t, 1.0);
    return info;
}

Visual volumeVisual(const QRectF& cell, bool scrubbing)
{
    Visual geom;
    const double trackH = qBound(10.0, cell.height() * 0.42, 22.0);
    geom.ringDiameter = scrubbing ? qMin(cell.height() - 4.0, trackH + 10.0) : trackH;
    const double inset = geom.ringDiameter * 0.5;
    geom.trackCy = cell.center().y();
    geom.track = QRectF(cell.left() + 6.0, geom.trackCy - trackH * 0.5,
                        qMax(8.0, cell.width() - 12.0), trackH);
    geom.valueLeft = geom.track.left() + inset;
    geom.valueRight = geom.track.right() - inset;
    if (geom.valueRight <= geom.valueLeft + 1.0) {
        const double mid = geom.track.center().x();
        geom.valueLeft = mid - 1.0;
        geom.valueRight = mid + 1.0;
    }
    return geom;
}

int percentFromLabel(const QString& label)
{
    bool ok = false;
    const int n = QString(label).remove(QLatin1Char('%')).trimmed().toInt(&ok);
    return ok ? qBound(0, n, 100) : 0;
}

void paintVolumeBar(QPainter& p, const QRectF& cell, const ThemeColors& theme,
                    const ProgressVisuals& pv, const QString& label, bool hovered,
                    double hoverProgress, bool scrubbing, double scrubT, double scrubProgress)
{
    const double t = scrubbing ? qBound(0.0, scrubT, 1.0) : percentFromLabel(label) / 100.0;
    const Visual geom = volumeVisual(cell, scrubbing);
    const QRectF track = geom.track;
    const double radius = track.height() * 0.5;
    QColor empty = theme.bgMain.isValid() ? theme.bgMain : QColor(24, 24, 26);
    QColor fill = theme.accent.isValid() ? theme.accent : QColor(80, 160, 220);
    p.setPen(Qt::NoPen);
    p.setBrush(empty);
    p.drawRoundedRect(track, radius, radius);
    const double fillRight = geom.valueLeft + (geom.valueRight - geom.valueLeft) * t;
    QRectF filled = track;
    filled.setRight(
        qBound(track.left() + radius, fillRight + geom.ringDiameter * 0.075, track.right()));
    p.setBrush(fill);
    p.drawRoundedRect(filled, radius, radius);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 70), 1.1));
    p.drawRoundedRect(track.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);

    const QPointF pos = geom.posAt(t);
    const double ringD = geom.ringDiameter;
    const QRectF ring(pos.x() - ringD * 0.5, pos.y() - ringD * 0.5, ringD, ringD);
    p.setBrush(empty);
    p.setPen(QPen(Qt::white, scrubbing ? 2.4 : 2.0));
    p.drawEllipse(ring);
    p.setPen(QPen(QColor(0, 0, 0, 90), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(ring.adjusted(1.5, 1.5, -1.5, -1.5));
    const double ringProgress =
        scrubbing && scrubProgress > 0.0 ? scrubProgress : (hovered ? hoverProgress : 0.0);
    if (ringProgress > 0.01) {
        paintProgress(p, ring.adjusted(-3, -3, 3, 3), ringProgress, pv, ProgressShape::Ellipse);
    }
}

} // namespace

void paint(QPainter& p, const QRectF& cell, const ThemeColors& theme, const ProgressVisuals& pv,
           const QColor& previewColor, const QString& channel, const QString& label, bool hovered,
           double hoverProgress, bool scrubbing, double scrubT, const QString& scrubValue,
           double scrubProgress)
{
    if (channel.compare(QLatin1String("volume"), Qt::CaseInsensitive) == 0) {
        paintVolumeBar(p, cell, theme, pv, label, hovered, hoverProgress, scrubbing, scrubT,
                       scrubProgress);
        return;
    }
    const Visual geom = visual(cell, scrubbing);
    QColor base = previewColor.isValid() ? previewColor : ThemeColors::defaultProgressColor();
    int h = 0, s = 0, l = 0, a = 255;
    base.getHsl(&h, &s, &l, &a);
    if (h < 0) {
        h = 0;
    }
    const int cr = base.red();
    const int cg = base.green();
    const int cb = base.blue();
    const ChannelInfo info = channelInfo(channel, base);
    double t = scrubbing ? scrubT : info.t;
    QString valueText = scrubbing && !scrubValue.isEmpty() ? scrubValue : info.value;
    const QString nameText = label.isEmpty() ? info.name : label;
    const QString ch = channel.toLower();
    const QRectF track = geom.track;
    const double radius = track.height() * 0.5;

    if (ch == QLatin1String("a") || ch == QLatin1String("alpha")
        || ch == QLatin1String("opacity")) {
        const int tile = 6;
        for (int y = int(track.top()); y < int(track.bottom()); y += tile) {
            for (int x = int(track.left()); x < int(track.right()); x += tile) {
                const bool lite = ((x / tile) + (y / tile)) % 2 == 0;
                p.fillRect(x, y, tile, tile, lite ? QColor(210, 210, 210) : QColor(150, 150, 150));
            }
        }
    }

    QLinearGradient g(track.left(), track.center().y(), track.right(), track.center().y());
    if (ch == QLatin1String("h") || ch == QLatin1String("hue")) {
        for (int i = 0; i <= 6; ++i) {
            g.setColorAt(i / 6.0, QColor::fromHsl(qMin(359, i * 60), 255, 128));
        }
    } else if (ch == QLatin1String("s") || ch == QLatin1String("sat")) {
        g.setColorAt(0.0, QColor::fromHsl(h, 0, l));
        g.setColorAt(1.0, QColor::fromHsl(h, 255, l));
    } else if (ch == QLatin1String("l") || ch == QLatin1String("light")
               || ch == QLatin1String("lightness")) {
        g.setColorAt(0.0, QColor::fromHsl(h, s, 0));
        g.setColorAt(0.5, QColor::fromHsl(h, s, 128));
        g.setColorAt(1.0, QColor::fromHsl(h, s, 255));
    } else if (ch == QLatin1String("r") || ch == QLatin1String("red")) {
        g.setColorAt(0.0, QColor(0, cg, cb));
        g.setColorAt(1.0, QColor(255, cg, cb));
    } else if (ch == QLatin1String("g") || ch == QLatin1String("green")) {
        g.setColorAt(0.0, QColor(cr, 0, cb));
        g.setColorAt(1.0, QColor(cr, 255, cb));
    } else if (ch == QLatin1String("b") || ch == QLatin1String("blue")) {
        g.setColorAt(0.0, QColor(cr, cg, 0));
        g.setColorAt(1.0, QColor(cr, cg, 255));
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

    p.setPen(theme.text);
    p.setFont(QFont(QStringLiteral("Segoe UI"), 10, QFont::DemiBold));
    p.drawText(geom.header, Qt::AlignLeft | Qt::AlignVCenter, nameText);
    p.setPen(theme.textSecondary);
    p.drawText(geom.header, Qt::AlignRight | Qt::AlignVCenter, valueText);

    const QPointF pos = geom.posAt(t);
    const double ringD = geom.ringDiameter;
    const QRectF ring(pos.x() - ringD * 0.5, pos.y() - ringD * 0.5, ringD, ringD);
    p.setBrush(theme.bgMain);
    p.setPen(QPen(Qt::white, scrubbing ? 2.4 : 2.0));
    p.drawEllipse(ring);
    p.setPen(QPen(QColor(0, 0, 0, 90), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(ring.adjusted(1.5, 1.5, -1.5, -1.5));

    const double ringProgress =
        scrubbing && scrubProgress > 0.0 ? scrubProgress : (hovered ? hoverProgress : 0.0);
    if (scrubbing) {
        p.setPen(theme.text);
        p.setFont(QFont(QStringLiteral("Segoe UI Semibold"), 10, QFont::DemiBold));
        p.drawText(ring, Qt::AlignCenter, valueText);
    }
    if (ringProgress > 0.01) {
        paintProgress(p, ring.adjusted(-3, -3, 3, 3), ringProgress, pv, ProgressShape::Ellipse);
    }
}

} // namespace SliderTrack
} // namespace gazer
