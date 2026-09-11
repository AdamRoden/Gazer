#include "ui/ColorField.h"

#include "ui/SliderTrack.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QtGlobal>

namespace gazer {
namespace ColorField {

Visual visual(const QRectF& cell)
{
    Visual g;
    g.field = cell.adjusted(2.0, 2.0, -2.0, -2.0);
    g.handleR = qBound(7.0, qMin(g.field.width(), g.field.height()) * 0.035, 12.0);
    return g;
}

void paint(QPainter& p, const QRectF& cell, const QColor& color)
{
    const Visual geom = visual(cell);
    const QRectF r = geom.field;
    if (r.width() < 4.0 || r.height() < 4.0) {
        return;
    }
    const double radius = qBound(6.0, qMin(r.width(), r.height()) * 0.04, 14.0);
    QColor c = color.isValid() ? color : QColor(0, 220, 255);
    int h = 0, s = 0, v = 0, a = 255;
    c.getHsv(&h, &s, &v, &a);
    if (h < 0) {
        h = 0;
    }

    QPainterPath clip;
    clip.addRoundedRect(r, radius, radius);
    p.save();
    p.setClipPath(clip);
    SliderTrack::fillChecker(p, r);
    const QColor hue = QColor::fromHsv(h, 255, 255);
    QLinearGradient sat(r.left(), r.top(), r.right(), r.top());
    sat.setColorAt(0.0, QColor(255, 255, 255));
    sat.setColorAt(1.0, hue);
    p.fillRect(r, sat);
    QLinearGradient val(r.left(), r.top(), r.left(), r.bottom());
    val.setColorAt(0.0, QColor(0, 0, 0, 0));
    val.setColorAt(1.0, QColor(0, 0, 0, 255));
    p.fillRect(r, val);
    p.restore();

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 70), 1.2));
    p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);

    const QPointF pos = geom.posAt(s / 255.0, v / 255.0);
    const double d = geom.handleR * 2.0;
    const QRectF ring(pos.x() - geom.handleR, pos.y() - geom.handleR, d, d);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(Qt::white, 2.2));
    p.drawEllipse(ring);
    p.setPen(QPen(QColor(0, 0, 0, 180), 1.2));
    p.drawEllipse(ring.adjusted(1.4, 1.4, -1.4, -1.4));
}

} // namespace ColorField
} // namespace gazer
