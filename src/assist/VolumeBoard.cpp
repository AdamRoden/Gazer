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

int stepToward(int current, int target, qint64 nowMs, double perSec, Slew* slew)
{
    const int clamped = SystemVolume::clampPercent(current);
    const int goal = SystemVolume::clampPercent(target);
    if (!slew || perSec <= 0.0) {
        return goal;
    }
    if (slew->atMs < 0) {
        slew->atMs = nowMs;
        slew->credit = 0;
        return clamped;
    }
    const qint64 dt = qBound(qint64(0), nowMs - slew->atMs, qint64(200));
    slew->atMs = nowMs;
    if (clamped == goal) {
        slew->credit = 0;
        return clamped;
    }
    slew->credit += perSec * static_cast<double>(dt) / 1000.0;
    const int room = static_cast<int>(slew->credit);
    if (room <= 0) {
        return clamped;
    }
    const int step = qBound(-room, goal - clamped, room);
    slew->credit -= qAbs(step);
    return clamped + step;
}

std::optional<Gaze> at(const QVector<PageTarget>& targets, const QVector<PageGridPaint>& grids,
                       double drawerScale, const QPointF& gaze, qint64 nowMs, int fallbackGraceMs,
                       Arm* arm)
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
            *arm = {};
            return std::nullopt;
        }
        const QString key = sessionKey(t);
        const int grace = t.dwell.scanGrace.value_or(qMax(0, fallbackGraceMs));
        if (arm->key != key) {
            arm->key = key;
            arm->sinceMs = nowMs;
        }
        const qint64 elapsed = qMax(qint64(0), nowMs - arm->sinceMs);
        Gaze out;
        out.key = key;
        if (grace <= 0 || elapsed >= grace) {
            out.arm = 1;
            out.percent = SystemVolume::clampPercent(
                qRound(SliderTrack::volumeFractionAtX(cell, gaze.x()) * 100.0));
        } else {
            out.arm = static_cast<double>(elapsed) / static_cast<double>(grace);
        }
        return out;
    }
    *arm = {};
    return std::nullopt;
}

} // namespace VolumeBoard
} // namespace gazer
