#pragma once

#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QtGlobal>

class QPainter;

namespace gazer {
namespace ColorField {

struct Visual {
    QRectF field;
    double handleR = 8.0;

    [[nodiscard]] QPointF posAt(double s, double v) const
    {
        const double u = qBound(0.0, s, 1.0);
        const double w = qBound(0.0, v, 1.0);
        return {field.left() + handleR + u * qMax(1.0, field.width() - 2.0 * handleR),
                field.top() + handleR + (1.0 - w) * qMax(1.0, field.height() - 2.0 * handleR)};
    }

    /// @p s saturation 0–1 left→right; @p v value 0–1 bottom→top.
    void svAt(const QPointF& p, double* s, double* v) const
    {
        const double spanX = qMax(1.0, field.width() - 2.0 * handleR);
        const double spanY = qMax(1.0, field.height() - 2.0 * handleR);
        const double u = (p.x() - field.left() - handleR) / spanX;
        const double t = (p.y() - field.top() - handleR) / spanY;
        if (s) {
            *s = qBound(0.0, u, 1.0);
        }
        if (v) {
            *v = qBound(0.0, 1.0 - t, 1.0);
        }
    }
};

[[nodiscard]] Visual visual(const QRectF& cell);

void paint(QPainter& p, const QRectF& cell, const QColor& color);

} // namespace ColorField
} // namespace gazer
