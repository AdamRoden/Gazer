#include "ui/PickStyle.h"

#include "ui/ProgressPaint.h"

#include <QPainterPath>
#include <QtMath>

namespace gazer {
namespace PickStyle {

int sanitizeMag(int flags)
{
    flags &= kMagPickMask;
    return flags == 0 ? kDefaultMagPick : flags;
}

int sanitizeMouse(int flags)
{
    flags &= kMousePickMask;
    return flags == 0 ? kDefaultMousePick : flags;
}

bool has(int flags, Flag f)
{
    return (flags & int(f)) != 0;
}

QString label(int flags)
{
    QStringList parts;
    if (has(flags, Cursor)) {
        parts << QStringLiteral("Cursor");
    }
    if (has(flags, Dot)) {
        parts << QStringLiteral("Dot");
    }
    if (has(flags, Crosshair)) {
        parts << QStringLiteral("Crosshair");
    }
    if (has(flags, GazeIndicator)) {
        parts << QStringLiteral("Gaze");
    }
    return parts.isEmpty() ? QStringLiteral("Cursor") : parts.join(QStringLiteral(", "));
}

void paint(QPainter& p, const QPointF& c, int flags, double progress, const ProgressVisuals& visuals)
{
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor accent = visuals.progressColor.isValid() ? visuals.progressColor
                                                          : ThemeColors::defaultProgressColor();

    if (has(flags, GazeIndicator)) {
        QColor fill = accent;
        fill.setAlpha(70);
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawEllipse(c, 72.0, 72.0);
    }

    if (has(flags, Crosshair)) {
        p.setPen(QPen(QColor(0, 0, 0, 180), 3.0));
        p.drawLine(QPointF(c.x() - 28, c.y()), QPointF(c.x() + 28, c.y()));
        p.drawLine(QPointF(c.x(), c.y() - 28), QPointF(c.x(), c.y() + 28));
        p.setPen(QPen(Qt::white, 1.6));
        p.drawLine(QPointF(c.x() - 28, c.y()), QPointF(c.x() + 28, c.y()));
        p.drawLine(QPointF(c.x(), c.y() - 28), QPointF(c.x(), c.y() + 28));
    }

    if (has(flags, Dot)) {
        p.setPen(QPen(Qt::black, 1.2));
        p.setBrush(accent);
        p.drawEllipse(c, 5.5, 5.5);
    }

    if (has(flags, Cursor)) {
        QPainterPath path;
        path.moveTo(c.x(), c.y());
        path.lineTo(c.x(), c.y() + 22);
        path.lineTo(c.x() + 6, c.y() + 16);
        path.lineTo(c.x() + 10, c.y() + 26);
        path.lineTo(c.x() + 14, c.y() + 24);
        path.lineTo(c.x() + 10, c.y() + 14);
        path.lineTo(c.x() + 18, c.y() + 14);
        path.closeSubpath();
        p.setPen(QPen(Qt::black, 1.4));
        p.setBrush(QColor(255, 255, 255, 235));
        p.drawPath(path);
    }

    if (progress > 0.01
        && visuals.style.any()) {
        const qreal s = has(flags, GazeIndicator) ? 128.0 : 64.0;
        paintProgress(p, QRectF(c.x() - s * 0.5, c.y() - s * 0.5, s, s), progress, visuals,
                      ProgressShape::Ellipse);
    }
}

} // namespace PickStyle
} // namespace gazer
