#include "ui/ScrollBar.h"

#include <QPainter>
#include <QPen>
#include <QStringList>

namespace gazer {
namespace ScrollBar {

Spec parseSpec(const QString& caption)
{
    Spec s;
    const QStringList parts = caption.split(QLatin1Char(','));
    if (parts.size() >= 3) {
        s.offset = parts[0].trimmed().toInt();
        s.visible = parts[1].trimmed().toInt();
        s.total = parts[2].trimmed().toInt();
    }
    return s;
}

QString formatSpec(int offset, int visible, int total)
{
    return QStringLiteral("%1,%2,%3").arg(offset).arg(visible).arg(total);
}

Visual visual(const QRectF& cell, const Spec& spec, double t)
{
    Visual g;
    const double u = t >= 0.0 ? qBound(0.0, t, 1.0) : spec.t();
    const double w = qBound(8.0, cell.width() * 0.36, 22.0);
    const double insetY = qBound(6.0, cell.height() * 0.04, 12.0);
    g.track = QRectF(cell.center().x() - w * 0.5, cell.top() + insetY, w,
                     qMax(w, cell.height() - insetY * 2.0));
    const double frac =
        (spec.total <= 0 || spec.visible <= 0)
            ? 1.0
            : qBound(0.0, double(spec.visible) / double(spec.total), 1.0);
    const double minH = qMin(g.track.height(), qMax(w * 1.8, 28.0));
    const double thumbH = qBound(minH, g.track.height() * frac, g.track.height());
    const double usable = g.track.height() - thumbH;
    g.thumb = QRectF(g.track.left(), g.track.top() + usable * u, g.track.width(), thumbH);
    return g;
}

int offsetAtY(const QRectF& cell, double y, const Spec& spec)
{
    const int max = spec.maxOffset();
    if (max <= 0) {
        return 0;
    }
    return qBound(0, int(qRound(visual(cell, spec).tAtY(y) * double(max))), max);
}

void paint(QPainter& p, const QRectF& cell, const ThemeColors& theme, const Spec& spec, double t)
{
    const Visual g = visual(cell, spec, t);
    QColor well = theme.bgMain.isValid() ? theme.bgMain : QColor(24, 24, 26);
    QColor fill = theme.accent.isValid() ? theme.accent : ThemeColors::defaultProgressColor();
    if (spec.maxOffset() <= 0) {
        fill = ThemeColors::mix(well, fill, 0.35);
    }
    const double radius = g.track.width() * 0.5;
    p.setPen(Qt::NoPen);
    p.setBrush(well);
    p.drawRoundedRect(g.track, radius, radius);
    p.setBrush(fill);
    p.drawRoundedRect(g.thumb, radius, radius);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 70), 1.1));
    p.drawRoundedRect(g.track.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
}

} // namespace ScrollBar
} // namespace gazer
