#pragma once

#include "input/PixelScroller.h"

#include <QString>
#include <QtGlobal>

namespace gazer {
namespace GazeTrack {

/// Rest band around the thumb. The ramp past it runs to the end of the track
/// in the direction of the look, and that end is full speed.
inline constexpr int kSliderDeadzonePx = 40;
inline constexpr double kSliderMaxNotches = 3.0;

/// One look at a slider or scrollbar. Gaze has to leave the thumb by
/// `kSliderDeadzonePx` before it moves. Speed then follows how far the look is
/// from the thumb toward that end of the track: the end is full speed, and a
/// look partway is slower. Callers quantize; `posPx` keeps the fraction.
struct Drag {
    bool placed = false;
    double posPx = 0;
    qint64 atMs = -1;
};

/// Shared arm clock for volume, a list scrollbar, and a color hue or opacity
/// track. `key` is the scrub id. `arm` is 0–1; 1 means the thumb may move.
struct Lock {
    QString key;
    qint64 sinceMs = -1;

    void clear()
    {
        key.clear();
        sinceMs = -1;
    }

    /// A changed id restarts at this sample. `graceMs` <= 0 is already armed.
    [[nodiscard]] double arm(const QString& id, qint64 nowMs, int graceMs)
    {
        if (key != id) {
            key = id;
            sinceMs = nowMs;
        }
        if (graceMs <= 0) {
            return 1.0;
        }
        const qint64 elapsed = qMax(qint64(0), nowMs - sinceMs);
        if (elapsed >= graceMs) {
            return 1.0;
        }
        return double(elapsed) / double(graceMs);
    }
};

/// Absolute thumb position along the track, in the same pixels as @p thumbPx.
/// @p track0 and @p track1 are the ends of thumb travel.
[[nodiscard]] inline double moveThumb(Drag& d, double gazePx, double thumbPx, double track0,
                                      double track1, qint64 nowMs)
{
    if (track1 < track0) {
        qSwap(track0, track1);
    }
    const double dt = d.atMs < 0 ? 0.016
                                 : qBound(0.004, double(nowMs - d.atMs) / 1000.0, 0.05);
    d.atMs = nowMs;
    if (track1 - track0 <= 1.0) {
        d.placed = false;
        return thumbPx;
    }

    const double set = qBound(track0, gazePx, track1);
    const double error = set - thumbPx;
    const double dist = qAbs(error);
    const double end = error >= 0.0 ? track1 : track0;
    const double span = qAbs(end - thumbPx);
    // Shrink the rest band when the thumb is already near that end, so a look
    // on the end still runs at full speed for the last stretch.
    const double zone = qMin(double(kSliderDeadzonePx), span * 0.35);
    if (!(dist > zone && span > 0.5)) {
        d.placed = false;
        return thumbPx;
    }

    if (!d.placed) {
        d.placed = true;
        d.posPx = thumbPx;
    }
    const double gain = qBound(0.0, (dist - zone) / qMax(1.0, span - zone), 1.0);
    const double rate = kSliderMaxNotches * PixelScroller::kPixelsPerNotch * gain;
    const double sign = error > 0.0 ? 1.0 : -1.0;
    const double step = sign * qMin(dist, rate * dt);
    d.posPx = qBound(track0, d.posPx + step, track1);
    return d.posPx;
}

} // namespace GazeTrack
} // namespace gazer
