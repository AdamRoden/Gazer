#pragma once

#include <QtGlobal>

namespace gazer {

inline constexpr double kLtsSpeedStops[] = {1.0, 2.0, 5.0, 10.0, 20.0};
inline constexpr int kLtsSpeedStopCount = 5;

/// Once engaged, never stream slower than this (post-accel rate). Targets that
/// latch a gesture (Win32 thumb-track) treat long gaps as end/start and bounce.
/// 12 px/s ≈ one high-res wheel unit / 55 ms — same leftover cadence as OptiKey.
inline constexpr double kLtsMinEngagedPxPerSec = 12.0;

/// Distance past the deadzone, as a 0–1 fraction of falloff. Linear, like OptiKey.
[[nodiscard]] inline double easeLtsFalloff(double t)
{
    return qBound(0.0, t, 1.0);
}

[[nodiscard]] inline int ltsDeadzoneHysteresisPx(int deadzonePx)
{
    return qBound(16, qMax(1, deadzonePx) / 4, 40);
}

/// Deadzone is the start line; deadzone − hysteresis is the stop line.
/// While engaged, LTS still emits (at least kLtsMinEngagedPxPerSec), including
/// in the band just inside the ring, so tracker jitter does not break the stream.
[[nodiscard]] inline bool ltsKeepScrolling(bool engaged, double dist, int deadzonePx)
{
    const double zone = double(qMax(1, deadzonePx));
    if (engaged) {
        return dist > zone - double(ltsDeadzoneHysteresisPx(deadzonePx));
    }
    return dist > zone;
}

/// True when this axis is contributing (offset beyond the deadzone hysteresis).
[[nodiscard]] inline bool ltsAxisAccelActive(double deltaAxis, int deadzonePx)
{
    return qAbs(deltaAxis) > double(ltsDeadzoneHysteresisPx(deadzonePx));
}

/// Reset @p sec when the axis is at rest. Add @p dt only while @p accumulate.
/// Active + not accumulating freezes the time (e.g. inside the ring).
inline void stepLtsAxisAccelSec(double& sec, bool active, bool accumulate, double dt)
{
    if (!active) {
        sec = 0.0;
        return;
    }
    if (accumulate && dt > 0.0) {
        sec += dt;
    }
}

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
