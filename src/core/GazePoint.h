#pragma once

#include <QtGlobal>
#include <QPointF>

namespace gazer {

/// Single gaze sample from a tracker backend.
///
/// Coordinates are in Qt screen space (logical pixels) relative to the
/// virtual desktop origin (QGuiApplication::screens() / QScreen geometry).
/// Values may lie outside all screen rects (e.g. Tobii gaze past the bezel);
/// consumers must not assume samples are clamped to the display.
struct GazePoint {
    double x = 0.0;
    double y = 0.0;
    /// Milliseconds since an arbitrary epoch (usually QElapsedTimer from start).
    qint64 timestampMs = 0;
    /// False when the tracker has no confident estimate (blink, lost tracking).
    bool valid = false;

    [[nodiscard]] QPointF toPointF() const { return {x, y}; }
};

} // namespace gazer
