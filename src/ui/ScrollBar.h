#pragma once

#include "ui/Theme.h"

#include <QColor>
#include <QRectF>
#include <QString>
#include <QtGlobal>

class QPainter;

namespace gazer {
namespace ScrollBar {

/// Caption payload for `role="scrollbar"`: `offset,visible,total`.
struct Spec {
    int offset = 0;
    int visible = 0;
    int total = 0;

    [[nodiscard]] int maxOffset() const { return qMax(0, total - qMax(0, visible)); }
    [[nodiscard]] double t() const
    {
        const int max = maxOffset();
        return max > 0 ? qBound(0.0, double(offset) / double(max), 1.0) : 0.0;
    }
};

struct Visual {
    QRectF track;
    QRectF thumb;

    [[nodiscard]] double tAtY(double y) const
    {
        const double usable = track.height() - thumb.height();
        if (usable <= 1.0) {
            return 0.0;
        }
        return qBound(0.0, (y - track.top() - thumb.height() * 0.5) / usable, 1.0);
    }
};

[[nodiscard]] Spec parseSpec(const QString& caption);
[[nodiscard]] QString formatSpec(int offset, int visible, int total);
[[nodiscard]] Visual visual(const QRectF& cell, const Spec& spec, double t = -1.0);
[[nodiscard]] int offsetAtY(const QRectF& cell, double y, const Spec& spec);

void paint(QPainter& p, const QRectF& cell, const ThemeColors& theme, const Spec& spec,
           double t = -1.0);

} // namespace ScrollBar
} // namespace gazer
