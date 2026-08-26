#pragma once

#include <QPointF>
#include <QtMath>

namespace gazer {
namespace ComboMouseHit {

inline constexpr int kSliceCount = 5;
inline constexpr double kSliceDeg = 360.0 / double(kSliceCount);
/// Hole / yellow-ring radii (120px and 240px diameters).
inline constexpr double kHoleRadiusPx = 60.0;
inline constexpr double kRingOuterPx = 120.0;

/// Clockwise from 12 o'clock: Right, Move, Cancel, Drag, Left.
enum class Slice { Right = 0, Move, Cancel, Drag, Left };

enum class Band { None, Deadzone, Drift, Slice };

struct Result {
    Band band = Band::None;
    Slice slice = Slice::Right;
};

/// 0° at 12 o'clock, increasing clockwise, in [0, 360).
[[nodiscard]] inline double clockwiseFromTopDeg(QPointF gaze, QPointF origin)
{
    const double dx = gaze.x() - origin.x();
    const double dy = gaze.y() - origin.y();
    double deg = qRadiansToDegrees(qAtan2(dy, dx)) + 90.0;
    while (deg < 0.0) {
        deg += 360.0;
    }
    while (deg >= 360.0) {
        deg -= 360.0;
    }
    return deg;
}

[[nodiscard]] inline Slice sliceAtDeg(double clockwiseFromTop)
{
    int i = int(clockwiseFromTop / kSliceDeg);
    if (i < 0) {
        i = 0;
    }
    if (i >= kSliceCount) {
        i = kSliceCount - 1;
    }
    return Slice(i);
}

[[nodiscard]] inline Result hit(QPointF gaze, QPointF origin, double deadzone, double ringOuter,
                                double pieOuter)
{
    Result r;
    const double dx = gaze.x() - origin.x();
    const double dy = gaze.y() - origin.y();
    const double dist = qSqrt(dx * dx + dy * dy);
    if (pieOuter <= 0.0 || dist > pieOuter) {
        return r;
    }
    if (dist < deadzone) {
        r.band = Band::Deadzone;
        return r;
    }
    if (dist < ringOuter) {
        r.band = Band::Drift;
        return r;
    }
    r.band = Band::Slice;
    r.slice = sliceAtDeg(clockwiseFromTopDeg(gaze, origin));
    return r;
}

} // namespace ComboMouseHit
} // namespace gazer
