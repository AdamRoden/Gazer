#include "layout/PageDetector.h"

#include "layout/DwellRegionSpace.h"
#include "layout/PageDim.h"

#include <QtGlobal>

namespace gazer {
namespace PageDetector {

namespace {

QRectF clipToScreen(const QRectF& visual, const QRectF& screen)
{
    const QRectF hit = visual.intersected(screen);
    if (!hit.isEmpty()) {
        return hit;
    }
    if (screen.isEmpty() || visual.isEmpty()) {
        return {};
    }
    const auto band =
        DwellRegionSpace::bandForCoercedProgress(visual.toRect(), screen.toRect());
    return band.onScreen;
}

} // namespace

PageDetectorGeom cell(const QRectF& visualCell, const QRectF& screen)
{
    PageDetectorGeom g;
    g.visual = visualCell;
    g.dwellZone = visualCell;
    g.progressZone = visualCell.intersected(screen);
    g.progressCoerced = false;
    return g;
}

PageDetectorGeom zone(const QRectF& visual, const QRectF& dwell, const QRectF& screen)
{
    PageDetectorGeom g;
    g.visual = visual;
    g.dwellZone = dwell;
    const QRectF on = visual.intersected(screen);
    if (!on.isEmpty()) {
        g.progressZone = on;
        g.progressCoerced = false;
    } else {
        g.progressZone = clipToScreen(visual, screen);
        g.progressCoerced = !g.progressZone.isEmpty();
    }
    return g;
}

PageDetectorGeom zoneFromDef(const PageZone& z, const QRectF& bounds, const QRectF& screen)
{
    const QRectF visual = PageDimParse::placeRect(bounds, z.anchor, z.offset, z.size);
    const double ox = z.dwellOffset.x.isSet() ? z.dwellOffset.x.resolve(bounds.width()) : 0.0;
    const double oy = z.dwellOffset.y.isSet() ? z.dwellOffset.y.resolve(bounds.height()) : 0.0;
    const double dw = z.dwellSize.x.isSet() ? z.dwellSize.x.resolve(bounds.width())
                                            : visual.width();
    const double dh = z.dwellSize.y.isSet() ? z.dwellSize.y.resolve(bounds.height())
                                            : visual.height();
    const QRectF dwell(visual.left() + ox, visual.top() + oy, dw, dh);
    return zone(visual, dwell, screen);
}

} // namespace PageDetector
} // namespace gazer
