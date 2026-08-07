#pragma once

#include <QLineF>
#include <QPointF>
#include <QtGlobal>

namespace gazer {

/// Shared magnifier-style stickiness (error → EMA alpha). Used by magnifier,
/// gaze reticle, and gaze→mouse so one profile drives all three.
struct GazeFollowStickiness {
    double jitterPx = 6.0;
    double fullTrackPx = 140.0;
    double alphaMin = 0.05;
    double alphaMax = 0.88;

    static GazeFollowStickiness fromProfile(int profile)
    {
        GazeFollowStickiness s;
        switch (qBound(0, profile, 2)) {
        case 0: // sticky
            s.jitterPx = 12.0;
            s.fullTrackPx = 200.0;
            s.alphaMin = 0.03;
            s.alphaMax = 0.72;
            break;
        case 2: // snappy
            s.jitterPx = 3.0;
            s.fullTrackPx = 90.0;
            s.alphaMin = 0.12;
            s.alphaMax = 0.95;
            break;
        default: // balanced
            break;
        }
        return s;
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
};

} // namespace gazer
