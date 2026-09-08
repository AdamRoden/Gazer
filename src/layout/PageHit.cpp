#include "layout/PageHit.h"

#include "layout/PageDim.h"
#include "layout/PageResolve.h"
#include "layout/RoundBox.h"

#include <QHash>
#include <QPolygonF>
#include <QSizeF>
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
                int colSpan, const QSizeF& screen)
{
    return PageDimParse::cellRect(grid, gridRect, row, col, rowSpan, colSpan, screen);
}

QRectF gridBounds(const PageGrid& grid, const PageFrame& frame)
{
    const QRectF& ref = grid.desktopMode ? frame.desktop : frame.screen;
    PageDimPair size = grid.size;
    if (!size.isSet()) {
        size.x = PageDim::pixels(600);
        size.y = PageDim::pixels(400);
    }
    const QSizeF metrics(ref.width(), ref.height());
    return PageDimParse::placeRect(ref, grid.anchor, grid.offset, size, metrics);
}

namespace {

void accumulateGrid(const PageGrid& grid, const QRectF& bounds, const QVector<int>& shown,
                    const QSizeF& metrics, QRectF& u)
{
    if (!layersVisible(grid.layers, shown) && grid.shell) {
        return;
    }
    if (!bounds.isEmpty()) {
        u = u.isEmpty() ? bounds : u.united(bounds);
    }
    for (const PageGrid& sub : grid.subGrids) {
        const QRectF slot =
            cellRect(grid, bounds, sub.row, sub.col, sub.rowSpan, sub.colSpan, metrics);
        accumulateGrid(sub, slot, shown, metrics, u);
    }
}

} // namespace

QRectF reservedBounds(const PageDocument& page, const PageFrame& frame)
{
    QRectF u;
    const QRectF screen = frame.screen.isEmpty() ? frame.desktop : frame.screen;
    const QSizeF metrics(screen.size());
    for (const PageGrid& g : page.grids) {
        accumulateGrid(g, gridBounds(g, frame), page.showLayers, metrics, u);
    }
    for (const PageZone& z : page.zones) {
        const QRectF ref = z.desktopMode ? frame.desktop : frame.screen;
        const QRectF vis = PageDetector::zoneFromDef(z, ref, screen).visual;
        if (vis.isEmpty()) {
            continue;
        }
        u = u.isEmpty() ? vis : u.united(vis);
    }
    return u;
}

QPoint cellIndexAt(const PageGrid& grid, const QRectF& gridRect, const QPointF& pos,
                   const QSizeF& screen)
{
    return PageDimParse::cellIndexAt(grid, gridRect, pos, screen);
}

namespace {

void walkGrid(const PageDocument& page, const PageGrid& grid, const QRectF& bounds,
              const QRectF& screen, const QVariantMap& props, bool dwellSuspended, bool shell,
              bool drawerMotion, bool includeDrawerMotion, const QVector<int>& shownLayers,
              QVector<PageTarget>& out, QVector<PageGridPaint>* grids)
{
    if (!layersVisible(grid.layers, shownLayers)
        && !(includeDrawerMotion && grid.drawerMotion)) {
        return;
    }

    const bool layer = shell || grid.shell;
    const bool drawer = drawerMotion || grid.drawerMotion;

    if (grids && (!grid.nested || grid.style.hasAny() || !grid.styleId.isEmpty())) {
        PageGridPaint gp;
        gp.visual = bounds;
        gp.chrome = PageResolve::gridStyle(page, grid.styleId, grid.style);
        gp.gridId = grid.id;
        gp.drawerMotion = drawer;
        gp.shell = layer;
        grids->push_back(std::move(gp));
    }

    for (const PageCell& cell : grid.cells) {
        if (!evalVisibleWhen(cell.visibleWhen, props)) {
            continue;
        }
        const QRectF visual = cellRect(grid, bounds, cell.row, cell.col, cell.rowSpan,
                                       cell.colSpan, screen.size());
        PageTarget t;
        t.kind = PageTarget::Kind::Cell;
        t.gridId = grid.id;
        t.id = cell.id;
        t.label = cell.label;
        t.icon = cell.icon;
        t.caption = cell.caption;
        t.textStyle = cell.textStyle;
        t.role = cell.role;
        t.settingKey = cell.settingKey;
        t.suspendExempt = cell.suspendExempt;
        t.interactive = cell.isInteractive() && !(dwellSuspended && !cell.suspendExempt);
        t.shell = layer;
        t.drawerMotion = drawer;
        t.activeState = cell.activeState;
        t.chrome = PageResolve::style(page, cell.styleId, cell.style);
        t.dwell = PageResolve::dwell(page, cell.dwellId, cell.dwell);
        t.actions = cell.actions;
        t.actionLoop = cell.actionLoop;
        t.geom = PageDetector::cell(visual, screen);
        out.push_back(t);
    }

    for (const PageGrid& sub : grid.subGrids) {
        if (!layersVisible(sub.layers, shownLayers)
            && !(includeDrawerMotion && sub.drawerMotion)) {
            continue;
        }
        const QRectF slot = cellRect(grid, bounds, sub.row, sub.col, sub.rowSpan, sub.colSpan,
                                     screen.size());
        walkGrid(page, sub, slot, screen, props, dwellSuspended, layer, drawer,
                 includeDrawerMotion, shownLayers, out, grids);
    }
}

} // namespace

QVector<PageTarget> collect(const PageDocument& page, const PageFrame& frame,
                            const QVariantMap& props, bool dwellSuspended, QVector<PageGridPaint>* grids,
                            bool includeDrawerMotion, const std::optional<QVector<int>>& shownLayers)
{
    QVector<PageTarget> rest;
    QVector<PageTarget> shell;
    const QRectF screen = frame.screen.isEmpty() ? frame.desktop : frame.screen;
    const QVector<int>& shown = shownLayers ? *shownLayers : page.showLayers;

    for (const PageGrid& g : page.grids) {
        QVector<PageTarget> piece;
        walkGrid(page, g, gridBounds(g, frame), screen, props, dwellSuspended, g.shell,
                 g.drawerMotion, includeDrawerMotion, shown, piece, grids);
        for (PageTarget& t : piece) {
            (t.shell ? shell : rest).push_back(std::move(t));
        }
    }

    for (const PageZone& z : page.zones) {
        if (!layersVisible(z.layers, shown)) {
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
        t.settingKey = z.settingKey;
        t.suspendExempt = z.suspendExempt;
        t.interactive = z.isInteractive() && !(dwellSuspended && !z.suspendExempt);
        t.shell = z.shell;
        t.activeState = z.activeState;
        t.chrome = PageResolve::zoneStyle(page, z);
        t.dwell = PageResolve::zoneDwell(page, z);
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
    const PageGridPaint* cover = coveringGrid(grids, pos, drawerScale, targets, &xf);
    const QHash<QString, int> stack = pageStackOrder(targets, grids);
    for (int i = targets.size() - 1; i >= 0; --i) {
        const PageTarget& t = targets[i];
        if (!t.interactive || buriedByCover(t, cover, stack)) {
            continue;
        }
        if (progress) {
            QRectF z = t.geom.progressZone;
            z = mapDrawer(t, z, xf, drawerScale);
            if (shapeContains(z, t.chrome, pos)) {
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

const PageGridPaint* coveringGrid(const QVector<PageGridPaint>& grids, const QPointF& pos,
                                  double drawerScale, const QVector<PageTarget>& targets,
                                  const QTransform* cachedXf)
{
    const QTransform local = cachedXf ? QTransform() : drawerTransform(targets, drawerScale, grids);
    const QTransform& xf = cachedXf ? *cachedXf : local;
    for (int i = grids.size() - 1; i >= 0; --i) {
        const PageGridPaint& g = grids[i];
        if (g.visual.isEmpty()) {
            continue;
        }
        const QRectF z = mapDrawer(g.drawerMotion, g.visual, xf, drawerScale);
        if (shapeContains(z, g.chrome, pos)) {
            return &g;
        }
    }
    return nullptr;
}

QString coveringPageId(const QVector<PageGridPaint>& grids, const QPointF& pos, double drawerScale,
                       const QVector<PageTarget>& targets, const QTransform* cachedXf)
{
    const PageGridPaint* g = coveringGrid(grids, pos, drawerScale, targets, cachedXf);
    return g ? g->pageId : QString();
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

bool shapeContains(const QRectF& r, const PageChrome& chrome, const QPointF& pos)
{
    return roundedBoxContains(r, chrome.resolvedRadius(), pos);
}

QRectF frostedBounds(const QVector<PageTarget>& targets, const QVector<PageGridPaint>& grids)
{
    QRectF u;
    auto add = [&](const QRectF& r) {
        if (r.isEmpty()) {
            return;
        }
        u = u.isEmpty() ? r : u.united(r);
    };
    for (const PageGridPaint& g : grids) {
        if (g.chrome.hasBlur()) {
            add(g.visual);
        }
    }
    for (const PageTarget& t : targets) {
        if (t.chrome.hasBlur()) {
            add(t.geom.contentOnScreen());
        }
    }
    return u;
}

} // namespace PageHit
} // namespace gazer
