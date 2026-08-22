#pragma once

#include "ui/ProgressVisuals.h"
#include "ui/Theme.h"

#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QString>

class QPainter;

namespace gazer {
namespace SliderTrack {

struct Visual {
    QRectF track;
    QRectF header;
    double valueLeft = 0.0;
    double valueRight = 0.0;
    double trackCy = 0.0;
    double ringDiameter = 16.0;

    [[nodiscard]] QPointF posAt(double t) const
    {
        const double u = qBound(0.0, t, 1.0);
        return {valueLeft + (valueRight - valueLeft) * u, trackCy};
    }

    [[nodiscard]] double tAtX(double x) const
    {
        const double span = valueRight - valueLeft;
        if (span <= 1.0) {
            return 0.0;
        }
        return qBound(0.0, (x - valueLeft) / span, 1.0);
    }
};

[[nodiscard]] Visual visual(const QRectF& cell, bool scrubbing);

void paintPreview(QPainter& p, const QRectF& r, double radius, const QColor& color);

void paint(QPainter& p, const QRectF& cell, const ThemeColors& theme, const ProgressVisuals& pv,
           const QColor& previewColor, const QString& channel, const QString& label, bool hovered,
           double hoverProgress, bool scrubbing, double scrubT, const QString& scrubValue,
           double scrubProgress);

} // namespace SliderTrack
} // namespace gazer
