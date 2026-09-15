#pragma once

#include "layout/PageTypes.h"

#include <QRectF>

namespace gazer {

/// Gaze maps in dwellZone; chrome draws in progressZone.
struct PageDetectorGeom {
    /// Authored progress box (Zone size at the zone anchor). May sit off-screen.
    QRectF visual;
    /// Scan-grace rect. Same anchor type as the zone; offset from the progress box's anchor.
    QRectF dwellZone;
    /// Unrounded progress box used for accumulation hit-test (same as visual for zones).
    QRectF progressZone;

    /// Persistent content to paint (keys, on-screen Zone chips). Empty if fully off-screen.
    [[nodiscard]] QRectF contentOnScreen() const
    {
        return visual.isEmpty() ? progressZone : visual;
    }

    /// Dock edge chips: dwell sits outside the visual, so chrome stays hidden until scan grace.
    [[nodiscard]] bool hidesUntilProgress() const
    {
        return !visual.isEmpty() && !dwellZone.isEmpty()
               && !visual.contains(dwellZone.center());
    }
};

namespace PageDetector {

/// Both zones centered on the visual cell. Progress is clipped to screen.
[[nodiscard]] PageDetectorGeom cell(const QRectF& visualCell, const QRectF& screen);

/// Progress from the visual rect (clipped to screen); dwell unrestricted.
[[nodiscard]] PageDetectorGeom zone(const QRectF& visual, const QRectF& dwell, const QRectF& screen);

/// Resolve a Zone against a desktop/screen bounds rect (anchor/offset/size + dwellOffset/size).
[[nodiscard]] PageDetectorGeom zoneFromDef(const PageZone& z, const QRectF& bounds,
                                           const QRectF& screen);

} // namespace PageDetector
} // namespace gazer
