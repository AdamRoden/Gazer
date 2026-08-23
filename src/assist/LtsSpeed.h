#pragma once

#include <QtGlobal>

namespace gazer {

inline constexpr double kLtsSpeedStops[] = {1.0, 5.0, 10.0, 20.0, 50.0};
inline constexpr int kLtsSpeedStopCount = 5;

[[nodiscard]] inline int nearestLtsSpeedIndex(double n)
{
    int idx = 0;
    double bestD = qAbs(n - kLtsSpeedStops[0]);
    for (int i = 1; i < kLtsSpeedStopCount; ++i) {
        const double d = qAbs(n - kLtsSpeedStops[i]);
        if (d <= bestD) {
            bestD = d;
            idx = i;
        }
    }
    return idx;
}

[[nodiscard]] inline double snapLtsSpeed(double n)
{
    return kLtsSpeedStops[nearestLtsSpeedIndex(n)];
}

[[nodiscard]] inline double nudgeLtsSpeed(double n, int dir)
{
    const int idx = nearestLtsSpeedIndex(n);
    if (dir > 0) {
        if (n < kLtsSpeedStops[idx] - 0.001) {
            return kLtsSpeedStops[idx];
        }
        return kLtsSpeedStops[qMin(idx + 1, kLtsSpeedStopCount - 1)];
    }
    if (dir < 0) {
        if (n > kLtsSpeedStops[idx] + 0.001) {
            return kLtsSpeedStops[idx];
        }
        return kLtsSpeedStops[qMax(idx - 1, 0)];
    }
    return kLtsSpeedStops[idx];
}

} // namespace gazer
