#pragma once

#include "layout/PageTypes.h"

#include <QRectF>

namespace gazer {

/// DwellDetector geometry: gaze maps in dwellZone; chrome draws in progressZone.
struct PageDetectorGeom {
    /// Authored display rect (cell or Zone size at anchor). May be off-screen.
    QRectF visual;
    QRectF dwellZone;
    /// On-screen drawable area. Coerced to an edge strip when visual misses the screen.
    QRectF progressZone;
    /// True when progressZone is a fallback strip, not the on-screen part of visual.
    bool progressCoerced = false;

    /// Persistent content to paint (keys, on-screen Zone chips). Empty if fully off-screen.
    [[nodiscard]] QRectF contentOnScreen() const
    {
        return progressCoerced ? QRectF() : progressZone;
    }

    /// Dock edge chips: dwell sits outside the visual, so chrome stays hidden until progress/flash.
    [[nodiscard]] bool hidesUntilProgress() const
    {
        return !visual.isEmpty() && !dwellZone.isEmpty()
               && !visual.contains(dwellZone.center());
    }
};

namespace PageDetector {

/// CellDetector: both zones centered on the visual cell. Progress is clipped to screen.
[[nodiscard]] PageDetectorGeom cell(const QRectF& visualCell, const QRectF& screen);

/// ZoneDetector: progress from the visual rect (clipped to screen); dwell unrestricted.
[[nodiscard]] PageDetectorGeom zone(const QRectF& visual, const QRectF& dwell, const QRectF& screen);

/// Resolve a Zone against a desktop/screen bounds rect (anchor/offset/size + dwellOffset/size).
[[nodiscard]] PageDetectorGeom zoneFromDef(const PageZone& z, const QRectF& bounds,
                                           const QRectF& screen);

} // namespace PageDetector
} // namespace gazer
