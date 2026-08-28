#pragma once

#include "assist/GazeFollowProfile.h"

#include <QLineF>
#include <QPointF>
#include <QtGlobal>

namespace gazer {

/// Shared error→EMA follow. One `GazeFollowProfile` drives the live lens,
/// reticle, gaze→mouse, dwell-move cursor, mag-pick, and foresight.
struct GazeFollowStickiness {
    double jitterPx = 6.0;
    double fullTrackPx = 140.0;
    double alphaMin = 0.05;
    double alphaMax = 0.88;

    static GazeFollowStickiness fromProfile(GazeFollowProfile profile)
    {
        static const GazeFollowStickiness kTable[] = {
            {12.0, 200.0, 0.03, 0.72}, // Slow
            {6.0, 140.0, 0.05, 0.88},  // Sticky
            {4.5, 115.0, 0.08, 0.92},  // Smooth
            {3.0, 90.0, 0.12, 0.95},   // Snappy
        };
        return kTable[int(gazeFollowProfileFromInt(int(profile)))];
    }

    [[nodiscard]] double alphaForError(double errorPx) const
    {
        const double span = qMax(1.0, fullTrackPx - jitterPx);
        double t = (errorPx - jitterPx) / span;
        t = qBound(0.0, t, 1.0);
        const double s = t * t * (3.0 - 2.0 * t);
        const double shaped = s * s;
        return alphaMin + (alphaMax - alphaMin) * shaped;
    }

    /// EMA-smooth @p smooth toward @p raw using stickiness for this error.
    void smoothPoint(QPointF& smooth, bool& valid, const QPointF& raw) const
    {
        if (!valid) {
            smooth = raw;
            valid = true;
            return;
        }
        const double err = QLineF(smooth, raw).length();
        const double a = alphaForError(err);
        smooth.setX(smooth.x() * (1.0 - a) + raw.x() * a);
        smooth.setY(smooth.y() * (1.0 - a) + raw.y() * a);
    }

    void smoothPoint(QPointF& smooth, const QPointF& raw) const
    {
        bool valid = true;
        smoothPoint(smooth, valid, raw);
    }
};

} // namespace gazer
