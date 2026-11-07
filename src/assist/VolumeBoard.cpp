#include "assist/VolumeBoard.h"

#include "assist/SystemVolume.h"
#include "layout/PageTypes.h"
#include "ui/SliderTrack.h"

#include <QtGlobal>

namespace gazer {
namespace VolumeBoard {
namespace {

void stampGrid(PageGrid& grid, const QString& label)
{
    for (PageCell& cell : grid.cells) {
        if (isVolumeSlider(cell.role, cell.caption)) {
            cell.label = label;
        }
    }
    for (PageGrid& sub : grid.subGrids) {
        stampGrid(sub, label);
    }
}

} // namespace

void stamp(PageDocument& doc, int percent)
{
    const QString label = QStringLiteral("%1%").arg(SystemVolume::clampPercent(percent));
    for (PageGrid& grid : doc.grids) {
        stampGrid(grid, label);
    }
    for (PageZone& zone : doc.zones) {
        if (isVolumeSlider(zone.role, zone.caption)) {
            zone.label = label;
        }
    }
}

std::optional<Gaze> at(const QVector<PageTarget>& targets, const QVector<PageGridPaint>& grids,
                       double drawerScale, const QPointF& gaze, qint64 nowMs, GazeTrack::Lock* arm)
{
    if (!arm) {
        return std::nullopt;
    }
    const QTransform xf = PageHit::drawerTransform(targets, drawerScale, grids);
    const PageGridPaint* cover = PageHit::coveringGrid(grids, gaze, drawerScale, targets, &xf);
    const QHash<QString, int> stack = PageHit::pageStackOrder(targets, grids);
    for (int i = targets.size() - 1; i >= 0; --i) {
        const PageTarget& t = targets.at(i);
        if (PageHit::buriedByCover(t, cover, stack)) {
            continue;
        }
        QRectF cell = t.geom.contentOnScreen();
        if (cell.isEmpty()) {
            cell = t.geom.visual;
        }
        cell = PageHit::mapDrawer(t, cell, xf, drawerScale);
        if (cell.isEmpty() || !PageHit::shapeContains(cell, t.chrome, gaze)) {
            continue;
        }
        if (!isVolumeSlider(t.role, t.caption)) {
            return std::nullopt;
        }
        const QString key = sessionKey(t);
        const int grace = t.dwell.scanGrace.value_or(kTrackLockMs);
        const double progress = arm->arm(key, nowMs, grace);
        const SliderTrack::ValueAxis axis = SliderTrack::volumeValueAxis(cell);
        Gaze out;
        out.key = key;
        out.gazePx = gaze.x();
        out.track0 = axis.from;
        out.track1 = axis.to;
        out.ready = progress >= 1.0;
        out.arm = progress;
        return out;
    }
    return std::nullopt;
}

} // namespace VolumeBoard
} // namespace gazer
