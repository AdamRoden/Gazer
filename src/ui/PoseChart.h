#pragma once

#include "mapping/HeadPoseTypes.h"
#include "ui/Theme.h"

#include <QRectF>
#include <QString>
#include <QVector>

class QPainter;

namespace gazer {
namespace PoseChart {

void paintCurve(QPainter& p, const QRectF& cell, const ThemeColors& theme,
                const QVector<HeadPoseCurvePoint>& points, int selectedIndex = -1,
                double liveIn = 0.0, bool showLive = false);

void valueAt(const QRectF& cell, const QVector<HeadPoseCurvePoint>& points, const QPointF& pos,
             double* in, double* out);

[[nodiscard]] QVector<HeadPoseCurvePoint> parseCaption(const QString& caption);

} // namespace PoseChart
} // namespace gazer
