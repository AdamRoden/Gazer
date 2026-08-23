#include "layout/PageHit.h"

#include "layout/PageDim.h"
#include "layout/PageResolve.h"
#include "layout/RoundBox.h"

#include <QPolygonF>
#include <QTransform>
#include <QtGlobal>
#include <algorithm>
#include <utility>

namespace gazer {
namespace PageHit {

namespace {

[[nodiscard]] bool evalVisibleWhen(const QString& expr, const QVariantMap& props)
{
    const QString trimmed = expr.trimmed();
    if (trimmed.isEmpty()) {
        return true;
    }
    const bool negated = trimmed.startsWith(QLatin1Char('!'));
    const QString key = (negated ? trimmed.mid(1) : trimmed).trimmed();
    if (key.isEmpty()) {
        return true;
    }
    for (const QChar c : key) {
        if (!c.isLetterOrNumber() && c != QLatin1Char('_')) {
            return true;
        }
    }
    const bool truthy = props.value(key).toBool();
    return negated ? !truthy : truthy;
}

} // namespace

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
    const QRectF& ref =
        (grid.desktopMode || grid.aboveTaskbar) ? frame.desktop : frame.screen;
    PageDimPair size = grid.size;
    if (!size.isSet()) {
        size.x = PageDim::pixels(600);
        size.y = PageDim::pixels(400);
    }
    return PageDimParse::placeRect(ref, grid.anchor, grid.offset, size);
}

QPoint cellIndexAt(const PageGrid& grid, const QRectF& gridRect, const QPointF& pos)
{
    if (!gridRect.contains(pos)) {
        return {-1, -1};
    }
    const int cols = qMax(1, grid.columns);
    const int rows = qMax(1, grid.rows);
    const int gap = qMax(0, grid.gapPx);
    const int margin = qMax(0, grid.marginPx);
    const double innerW = gridRect.width() - 2.0 * margin - gap * (cols - 1);
    const double innerH = gridRect.height() - 2.0 * margin - gap * (rows - 1);
    if (innerW <= 0.0 || innerH <= 0.0) {
        return {-1, -1};
    }
    const double strideW = innerW / cols + gap;
    const double strideH = innerH / rows + gap;
    const double lx = pos.x() - gridRect.left() - margin;
    const double ly = pos.y() - gridRect.top() - margin;
    const int col = qBound(0, int(lx / qMax(1.0, strideW)), cols - 1);
    const int row = qBound(0, int(ly / qMax(1.0, strideH)), rows - 1);
    return {col, row};
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
        gp.gridId = grid.id;
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
        t.actionLoop = cell.actionLoop;
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
        const QRectF bounds =
            (z.desktopMode || z.aboveTaskbar) ? frame.desktop : frame.screen;
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
        t.actionLoop = z.actionLoop;
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

double cross(const QPointF& o, const QPointF& a, const QPointF& b)
{
    return (a.x() - o.x()) * (b.y() - o.y()) - (a.y() - o.y()) * (b.x() - o.x());
}

QPolygonF convexHull(QVector<QPointF> pts)
{
    pts.erase(std::remove_if(pts.begin(), pts.end(),
                             [](const QPointF& p) { return !qIsFinite(p.x()) || !qIsFinite(p.y()); }),
              pts.end());
    if (pts.size() <= 1) {
        return QPolygonF(pts);
    }
    std::sort(pts.begin(), pts.end(), [](const QPointF& a, const QPointF& b) {
        return a.x() < b.x() || (qFuzzyCompare(a.x() + 1.0, b.x() + 1.0) && a.y() < b.y());
    });
    QVector<QPointF> uniq;
    uniq.reserve(pts.size());
    for (const QPointF& p : pts) {
        if (!uniq.isEmpty() && qFuzzyCompare(uniq.last().x() + 1.0, p.x() + 1.0)
            && qFuzzyCompare(uniq.last().y() + 1.0, p.y() + 1.0)) {
            continue;
        }
        uniq.push_back(p);
    }
    if (uniq.size() <= 2) {
        return QPolygonF(uniq);
    }
    QVector<QPointF> lower;
    for (const QPointF& p : uniq) {
        while (lower.size() >= 2
               && cross(lower[lower.size() - 2], lower[lower.size() - 1], p) <= 0.0) {
            lower.pop_back();
        }
        lower.push_back(p);
    }
    QVector<QPointF> upper;
    for (int i = uniq.size() - 1; i >= 0; --i) {
        const QPointF& p = uniq[i];
        while (upper.size() >= 2
               && cross(upper[upper.size() - 2], upper[upper.size() - 1], p) <= 0.0) {
            upper.pop_back();
        }
        upper.push_back(p);
    }
    lower.pop_back();
    upper.pop_back();
    QPolygonF hull;
    hull.reserve(lower.size() + upper.size());
    for (const QPointF& p : lower) {
        hull << p;
    }
    for (const QPointF& p : upper) {
        hull << p;
    }
    return hull;
}

QPolygonF rectPoly(const QRectF& r)
{
    if (r.isEmpty()) {
        return {};
    }
    QPolygonF p;
    p << r.topLeft() << r.topRight() << r.bottomRight() << r.bottomLeft();
    return p;
}

QPolygonF mapPoly(const QPolygonF& poly, bool drawerMotion, const QTransform& xf, double scale)
{
    if (poly.isEmpty()) {
        return poly;
    }
    if (drawerMotion && scale < 0.999) {
        return xf.map(poly);
    }
    return poly;
}

} // namespace

QPolygonF gazeHitPolygon(const PageTarget& t, const QString& engagedId)
{
    const QRectF dwell = t.geom.dwellZone;
    if (t.kind != PageTarget::Kind::Zone) {
        return rectPoly(dwell);
    }
    const bool accumulate = !engagedId.isEmpty() && sessionKey(t) == engagedId;
    if (!accumulate) {
        return rectPoly(dwell);
    }
    const QRectF progress =
        t.geom.visual.isEmpty() ? t.geom.progressZone : t.geom.visual;
    if (progress.isEmpty() || progress == dwell) {
        return rectPoly(dwell.isEmpty() ? progress : dwell);
    }
    QVector<QPointF> pts;
    auto addRect = [&](const QRectF& r) {
        if (r.isEmpty()) {
            return;
        }
        pts << r.topLeft() << r.topRight() << r.bottomRight() << r.bottomLeft();
    };
    addRect(dwell);
    addRect(progress);
    QPolygonF hull = convexHull(pts);
    return hull.isEmpty() ? rectPoly(dwell.united(progress)) : hull;
}

QRectF gazeHitRect(const PageTarget& t, const QString& engagedId)
{
    return gazeHitPolygon(t, engagedId).boundingRect();
}

namespace {

const PageTarget* hit(const QVector<PageTarget>& targets, const QPointF& pos, bool progress,
                      double drawerScale, const QString& engagedId,
                      const QVector<PageGridPaint>& grids, const QTransform* cachedXf)
{
    const QTransform local = cachedXf ? QTransform() : drawerTransform(targets, drawerScale, grids);
    const QTransform& xf = cachedXf ? *cachedXf : local;
    const QString cover = coveringPageId(grids, pos, drawerScale, targets, &xf);
    for (int i = targets.size() - 1; i >= 0; --i) {
        const PageTarget& t = targets[i];
        if (!t.interactive) {
            continue;
        }
        if (!cover.isEmpty() && !t.shell && t.pageId != cover) {
            continue;
        }
        const bool clustered = !t.cluster.isEmpty();
        if (progress) {
            QRectF z = t.geom.progressZone;
            z = mapDrawer(t, z, xf, drawerScale);
            if (shapeContains(z, t.chrome, clustered, pos)) {
                return &t;
            }
        } else {
            QPolygonF poly = gazeHitPolygon(t, engagedId);
            poly = mapPoly(poly, t.drawerMotion, xf, drawerScale);
            if (!poly.isEmpty() && poly.containsPoint(pos, Qt::WindingFill)) {
                return &t;
            }
        }
    }
    return nullptr;
}

} // namespace

QString coveringPageId(const QVector<PageGridPaint>& grids, const QPointF& pos, double drawerScale,
                       const QVector<PageTarget>& targets, const QTransform* cachedXf)
{
    const QTransform local = cachedXf ? QTransform() : drawerTransform(targets, drawerScale, grids);
    const QTransform& xf = cachedXf ? *cachedXf : local;
    for (int i = grids.size() - 1; i >= 0; --i) {
        const PageGridPaint& g = grids[i];
        if (g.shell || g.visual.isEmpty()) {
            continue;
        }
        const QRectF z = mapDrawer(g.drawerMotion, g.visual, xf, drawerScale);
        if (shapeContains(z, g.chrome, false, pos)) {
            return g.pageId;
        }
    }
    return {};
}

const PageTarget* at(const QVector<PageTarget>& targets, const QPointF& gaze, double drawerScale,
                     const QString& engagedId, const QVector<PageGridPaint>& grids,
                     const QTransform* xf)
{
    return hit(targets, gaze, false, drawerScale, engagedId, grids, xf);
}

const PageTarget* atProgress(const QVector<PageTarget>& targets, const QPointF& pos,
                             double drawerScale, const QString& engagedId,
                             const QVector<PageGridPaint>& grids, const QTransform* xf)
{
    return hit(targets, pos, true, drawerScale, engagedId, grids, xf);
}

QRectF paintBounds(const QVector<PageTarget>& targets, const QVector<PageGridPaint>& grids,
                   double drawerScale)
{
    const QTransform xf = drawerTransform(targets, drawerScale, grids);
    QRectF u;
    auto add = [&](const QRectF& r) {
        if (r.isEmpty()) {
            return;
        }
        u = u.isEmpty() ? r : u.united(r);
    };
    for (const PageGridPaint& g : grids) {
        add(mapDrawer(g.drawerMotion, g.visual, xf, drawerScale));
    }
    for (const PageTarget& t : targets) {
        add(mapDrawer(t, t.geom.contentOnScreen(), xf, drawerScale));
        if (t.geom.hidesUntilProgress()) {
            add(mapDrawer(t, t.geom.progressZone, xf, drawerScale));
        }
    }
    return u;
}

bool shapeContains(const QRectF& r, const PageChrome& chrome, bool clustered, const QPointF& pos)
{
    return roundedBoxContains(r, chrome.resolvedRadius(clustered), pos);
}

QRectF frostedBounds(const QVector<PageTarget>& targets, const QVector<PageGridPaint>& grids,
                     double drawerScale)
{
    const QTransform xf = drawerTransform(targets, drawerScale, grids);
    QRectF u;
    auto add = [&](const QRectF& r) {
        if (r.isEmpty()) {
            return;
        }
        u = u.isEmpty() ? r : u.united(r);
    };
    for (const PageGridPaint& g : grids) {
        if (g.chrome.hasBlur()) {
            add(mapDrawer(g.drawerMotion, g.visual, xf, drawerScale));
        }
    }
    for (const PageTarget& t : targets) {
        if (t.chrome.hasBlur()) {
            add(mapDrawer(t, t.geom.contentOnScreen(), xf, drawerScale));
        }
    }
    return u;
}

} // namespace PageHit
} // namespace gazer
