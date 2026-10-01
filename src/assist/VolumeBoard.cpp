#include "assist/VolumeBoard.h"

#include "assist/SystemVolume.h"
#include "ui/SliderTrack.h"

namespace gazer {
namespace VolumeBoard {
namespace {

bool isSlider(const QString& role, const QString& caption)
{
    return role.compare(QLatin1String("slider"), Qt::CaseInsensitive) == 0
           && caption.compare(QLatin1String("volume"), Qt::CaseInsensitive) == 0;
}

void stampGrid(PageGrid& grid, const QString& label)
{
    for (PageCell& cell : grid.cells) {
        if (isSlider(cell.role, cell.caption)) {
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
        if (isSlider(zone.role, zone.caption)) {
            zone.label = label;
        }
    }
}

std::optional<int> percentAt(const QVector<PageTarget>& targets,
                             const QVector<PageGridPaint>& grids, double drawerScale,
                             const QPointF& gaze)
{
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
        if (!isSlider(t.role, t.caption)) {
            return std::nullopt;
        }
        return SystemVolume::clampPercent(
            qRound(SliderTrack::volumeFractionAtX(cell, gaze.x()) * 100.0));
    }
    return std::nullopt;
}

} // namespace VolumeBoard
} // namespace gazer
