#include "layout/PageDetector.h"

#include "layout/PageDim.h"

#include <QtGlobal>

namespace gazer {
namespace PageDetector {

PageDetectorGeom cell(const QRectF& visualCell, const QRectF& screen)
{
    PageDetectorGeom g;
    g.visual = visualCell;
    g.dwellZone = visualCell;
    g.progressZone = visualCell.intersected(screen);
    g.progressCoerced = false;
    return g;
}

PageDetectorGeom zone(const QRectF& visual, const QRectF& dwell, const QRectF& /*screen*/)
{
    PageDetectorGeom g;
    g.visual = visual;
    g.dwellZone = dwell.isEmpty() ? visual : dwell;
    g.progressZone = visual;
    g.progressCoerced = false;
    return g;
}

PageDetectorGeom zoneFromDef(const PageZone& z, const QRectF& bounds, const QRectF& screen)
{
    const QSizeF metrics(bounds.width(), bounds.height());
    const QRectF progress =
        PageDimParse::placeRect(bounds, z.anchor, z.offset, z.size, metrics);
    PageDimPair dwellSize = z.dwellSize.isSet() ? z.dwellSize : z.size;
    PageDimPair dwellOffset = z.dwellOffset;
    if (!dwellOffset.isSet()) {
        dwellOffset.x = PageDim::pixels(0);
        dwellOffset.y = PageDim::pixels(0);
    }
    // Dwell uses the same anchor type as the zone. Offset is from the progress
    // box's matching anchor point, not the progress top-left.
    const QRectF dwell =
        PageDimParse::placeRect(progress, z.anchor, dwellOffset, dwellSize, metrics);
    return zone(progress, dwell, screen);
}

} // namespace PageDetector
} // namespace gazer
