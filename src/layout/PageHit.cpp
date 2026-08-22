#include "layout/PageHit.h"

#include "layout/LayoutVisibility.h"
#include "layout/PageDim.h"
#include "layout/PageResolve.h"

#include <QTransform>
#include <QtGlobal>
#include <utility>

namespace gazer {
namespace PageHit {

QRectF cellRect(const PageGrid& grid, const QRectF& gridRect, int row, int col, int rowSpan,
                int colSpan)
{
    const int cols = qMax(1, grid.columns);
    const int rows = qMax(1, grid.rows);
    const int gap = qMax(0, grid.gapPx);
    const int margin = qMax(0, grid.marginPx);
    const double innerW = gridRect.width() - 2.0 * margin - gap * (cols - 1);
    const double innerH = gridRect.height() - 2.0 * margin - gap * (rows - 1);
    if (innerW <= 0.0 || innerH <= 0.0) {
        return {};
    }
    const double cellW = innerW / cols;
    const double cellH = innerH / rows;
    const double x = gridRect.left() + margin + col * (cellW + gap);
    const double y = gridRect.top() + margin + row * (cellH + gap);
    const double w = cellW * colSpan + gap * (colSpan - 1);
    const double h = cellH * rowSpan + gap * (rowSpan - 1);
    return QRectF(x, y, w, h);
}

QRectF gridBounds(const PageGrid& grid, const PageFrame& frame)
{
    const QRectF& ref = grid.desktopMode ? frame.desktop : frame.screen;
    PageDimPair size = grid.size;
    if (!size.isSet()) {
        size.x = PageDim::pixels(600);
        size.y = PageDim::pixels(400);
    }
    return PageDimParse::placeRect(ref, grid.anchor, grid.offset, size);
}

namespace {

void walkGrid(const PageDocument& page, const PageGrid& grid, const QRectF& bounds,
              const QRectF& screen, const QSet<QString>& hiddenGrids, const QVariantMap& props,
              bool dwellSuspended, bool shell, bool drawerMotion, QVector<const PageGrid*>& chain,
              QVector<PageTarget>& out, QVector<PageGridPaint>* grids)
{
    if (!grid.id.isEmpty() && hiddenGrids.contains(grid.id)) {
        return;
    }

    chain.push_back(&grid);
    const bool layer = shell || grid.shell;
    const bool drawer = drawerMotion || grid.drawerMotion;

    if (grids && (!grid.nested || grid.style.hasAny() || !grid.styleId.isEmpty())) {
        PageGridPaint gp;
        gp.visual = bounds;
        gp.chrome = PageResolve::style(page, {}, chain, {}, {});
        gp.drawerMotion = drawer;
        gp.shell = layer;
        grids->push_back(std::move(gp));
    }

    for (const PageCell& cell : grid.cells) {
        if (!cell.visible) {
            continue;
        }
        if (!evalVisibleWhen(cell.visibleWhen, props)) {
            continue;
        }
        const QRectF visual =
            cellRect(grid, bounds, cell.row, cell.col, cell.rowSpan, cell.colSpan);
        PageTarget t;
        t.kind = PageTarget::Kind::Cell;
        t.gridId = grid.id;
        t.id = cell.id;
        t.label = cell.label;
        t.icon = cell.icon;
        t.caption = cell.caption;
        t.textStyle = cell.textStyle;
        t.role = cell.role;
        t.dwellExempt = cell.dwellExempt;
        t.interactive = cell.interactive && !(dwellSuspended && !cell.dwellExempt);
        t.shell = layer || cell.shell;
        t.drawerMotion = drawer;
        t.cluster = cell.cluster;
        t.clusterSlot = cell.clusterSlot;
        t.activeState = cell.activeState;
        t.chrome = PageResolve::style(page, {}, chain, cell.styleId, cell.style);
        t.dwell = PageResolve::dwell(page, {}, chain, cell.dwellId, cell.dwell);
        t.actions = cell.actions;
        t.geom = PageDetector::cell(visual, screen);
        out.push_back(t);
    }

    for (const PageGrid& sub : grid.subGrids) {
        if (!sub.id.isEmpty() && hiddenGrids.contains(sub.id)) {
            continue;
        }
        const QRectF slot =
            cellRect(grid, bounds, sub.row, sub.col, sub.rowSpan, sub.colSpan);
        walkGrid(page, sub, slot, screen, hiddenGrids, props, dwellSuspended, layer, drawer, chain,
                 out, grids);
    }
    chain.pop_back();
}

} // namespace

QVector<PageTarget> collect(const PageDocument& page, const PageFrame& frame,
                            const QSet<QString>& hiddenGrids, const QSet<QString>& hiddenZones,
                            const QVariantMap& props, bool dwellSuspended,
                            QVector<PageGridPaint>* grids)
{
    QVector<PageTarget> rest;
    QVector<PageTarget> shell;
    const QRectF screen = frame.screen.isEmpty() ? frame.desktop : frame.screen;

    for (const PageGrid& g : page.grids) {
        QVector<const PageGrid*> chain;
        QVector<PageTarget> piece;
        walkGrid(page, g, gridBounds(g, frame), screen, hiddenGrids, props, dwellSuspended, g.shell,
                 g.drawerMotion, chain, piece, grids);
        for (PageTarget& t : piece) {
            (t.shell ? shell : rest).push_back(std::move(t));
        }
    }

    for (const PageZone& z : page.zones) {
        if (!z.visible) {
            continue;
        }
        if (!z.id.isEmpty() && hiddenZones.contains(z.id)) {
            continue;
        }
        if (!evalVisibleWhen(z.visibleWhen, props)) {
            continue;
        }
        const QRectF bounds = z.desktopMode ? frame.desktop : frame.screen;
        PageTarget t;
        t.kind = PageTarget::Kind::Zone;
        t.id = z.id;
        t.label = z.label;
        t.icon = z.icon;
        t.caption = z.caption;
        t.dwellExempt = z.dwellExempt;
        t.interactive = z.interactive && !(dwellSuspended && !z.dwellExempt);
        t.shell = z.shell;
        t.activeState = z.activeState;
        t.chrome = PageResolve::zoneStyle(page, {}, z);
        t.dwell = PageResolve::zoneDwell(page, {}, z);
        t.actions = z.actions;
        t.geom = PageDetector::zoneFromDef(z, bounds, screen);
        (t.shell ? shell : rest).push_back(std::move(t));
    }
    rest.append(shell);
    return rest;
}

QTransform drawerTransform(const QVector<PageTarget>& targets, double scale,
                           const QVector<PageGridPaint>& grids)
{
    QTransform xf;
    if (scale >= 0.999) {
        return xf;
    }
    QRectF full;
    for (const PageTarget& t : targets) {
        if (!t.drawerMotion) {
            continue;
        }
        const QRectF r = t.geom.visual.isEmpty() ? t.geom.contentOnScreen() : t.geom.visual;
        if (!r.isEmpty()) {
            full = full.isEmpty() ? r : full.united(r);
        }
    }
    for (const PageGridPaint& g : grids) {
        if (!g.drawerMotion || g.visual.isEmpty()) {
            continue;
        }
        full = full.isEmpty() ? g.visual : full.united(g.visual);
    }
    if (full.isEmpty()) {
        return xf;
    }
    xf.translate(full.center().x(), full.bottom());
    xf.scale(scale, scale);
    xf.translate(-full.center().x(), -full.bottom());
    return xf;
}

QRectF mapDrawer(bool drawerMotion, const QRectF& r, const QTransform& xf, double scale)
{
    if (drawerMotion && scale < 0.999) {
        return xf.mapRect(r);
    }
    return r;
}

QRectF mapDrawer(const PageTarget& t, const QRectF& r, const QTransform& xf, double scale)
{
    return mapDrawer(t.drawerMotion, r, xf, scale);
}

namespace {

QRectF dwellHitRect(const PageTarget& t, bool progress, const QString& engagedId)
{
    if (progress) {
        return t.geom.progressZone;
    }
    QRectF z = t.geom.dwellZone;
    if (t.kind == PageTarget::Kind::Zone && !engagedId.isEmpty() && t.id == engagedId) {
        z = z.united(t.geom.progressZone);
    }
    return z;
}

const PageTarget* hit(const QVector<PageTarget>& targets, const QPointF& pos, bool progress,
                      double drawerScale, const QString& engagedId,
                      const QVector<PageGridPaint>& grids)
{
    const QTransform xf = drawerTransform(targets, drawerScale, grids);
    const QString cover = coveringPageId(grids, pos, drawerScale, targets);
    for (int i = targets.size() - 1; i >= 0; --i) {
        const PageTarget& t = targets[i];
        if (!t.interactive) {
            continue;
        }
        if (!cover.isEmpty() && !t.shell && t.pageId != cover) {
            continue;
        }
        QRectF z = dwellHitRect(t, progress, engagedId);
        z = mapDrawer(t, z, xf, drawerScale);
        if (z.contains(pos)) {
            return &t;
        }
    }
    return nullptr;
}

} // namespace

QString coveringPageId(const QVector<PageGridPaint>& grids, const QPointF& pos, double drawerScale,
                       const QVector<PageTarget>& targets)
{
    const QTransform xf = drawerTransform(targets, drawerScale, grids);
    for (int i = grids.size() - 1; i >= 0; --i) {
        const PageGridPaint& g = grids[i];
        if (g.shell || g.visual.isEmpty()) {
            continue;
        }
        if (mapDrawer(g.drawerMotion, g.visual, xf, drawerScale).contains(pos)) {
            return g.pageId;
        }
    }
    return {};
}

const PageTarget* at(const QVector<PageTarget>& targets, const QPointF& gaze, double drawerScale,
                     const QString& engagedId, const QVector<PageGridPaint>& grids)
{
    return hit(targets, gaze, false, drawerScale, engagedId, grids);
}

const PageTarget* atProgress(const QVector<PageTarget>& targets, const QPointF& pos,
                             double drawerScale, const QString& engagedId,
                             const QVector<PageGridPaint>& grids)
{
    return hit(targets, pos, true, drawerScale, engagedId, grids);
}

} // namespace PageHit
} // namespace gazer
