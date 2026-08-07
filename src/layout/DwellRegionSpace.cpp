#include "layout/DwellRegionSpace.h"

#include <QGuiApplication>
#include <QtMath>

#include <climits>

namespace gazer {
namespace DwellRegionSpace {

QRect virtualDesktop()
{
    QRect u;
    for (QScreen* s : QGuiApplication::screens()) {
        if (s) {
            u = u.united(s->geometry());
        }
    }
    if (u.isEmpty()) {
        if (QScreen* s = QGuiApplication::primaryScreen()) {
            u = s->geometry();
        }
    }
    return u;
}

QScreen* nearestScreen(const QPoint& p)
{
    if (QScreen* s = QGuiApplication::screenAt(p)) {
        return s;
    }
    QScreen* best = QGuiApplication::primaryScreen();
    int bestDist = INT_MAX;
    for (QScreen* s : QGuiApplication::screens()) {
        if (!s) {
            continue;
        }
        const QRect g = s->geometry();
        const int dx = p.x() < g.left() ? g.left() - p.x()
                     : p.x() > g.right() ? p.x() - g.right() : 0;
        const int dy = p.y() < g.top() ? g.top() - p.y()
                     : p.y() > g.bottom() ? p.y() - g.bottom() : 0;
        const int dist = dx + dy;
        if (dist < bestDist) {
            bestDist = dist;
            best = s;
        }
    }
    return best;
}

bool mostlyOffDisplay(const QRect& target, const QRect& desktop)
{
    if (target.isEmpty() || !desktop.isValid()) {
        return true;
    }
    if (!desktop.contains(target.center())) {
        return true;
    }
    const QRect inter = desktop.intersected(target);
    return inter.isEmpty() || inter.width() < target.width() / 3
           || inter.height() < target.height() / 3;
}

Edge nearestEdge(const QRect& target, const QRect& screen)
{
    const QPoint c = target.center();
    const bool outL = c.x() < screen.left();
    const bool outR = c.x() > screen.right();
    const bool outT = c.y() < screen.top();
    const bool outB = c.y() > screen.bottom();

    if (outT && outL) {
        return Edge::TopLeft;
    }
    if (outT && outR) {
        return Edge::TopRight;
    }
    if (outB && outL) {
        return Edge::BottomLeft;
    }
    if (outB && outR) {
        return Edge::BottomRight;
    }
    if (outT) {
        return Edge::Top;
    }
    if (outB) {
        return Edge::Bottom;
    }
    if (outL) {
        return Edge::Left;
    }
    if (outR) {
        return Edge::Right;
    }

    const int dl = c.x() - screen.left();
    const int dr = screen.right() - c.x();
    const int dt = c.y() - screen.top();
    const int db = screen.bottom() - c.y();
    const int m = qMin(qMin(dl, dr), qMin(dt, db));
    if (m == dt) {
        return Edge::Top;
    }
    if (m == db) {
        return Edge::Bottom;
    }
    if (m == dl) {
        return Edge::Left;
    }
    return Edge::Right;
}

Edge edgeFromAnchor(LayoutDwellRegion::ScreenAnchor a)
{
    using SA = LayoutDwellRegion::ScreenAnchor;
    switch (a) {
    case SA::Top:
    case SA::TopCenter:
        return Edge::Top;
    case SA::Bottom:
    case SA::BottomCenter:
        return Edge::Bottom;
    case SA::Left:
    case SA::LeftCenter:
        return Edge::Left;
    case SA::Right:
    case SA::RightCenter:
        return Edge::Right;
    case SA::TopLeft:
        return Edge::TopLeft;
    case SA::TopRight:
        return Edge::TopRight;
    case SA::BottomLeft:
        return Edge::BottomLeft;
    case SA::BottomRight:
        return Edge::BottomRight;
    case SA::None:
        break;
    }
    return Edge::Bottom;
}

int edgeWidth(int depth)
{
    return depth * kWidthMul;
}

int clampAlong(Edge edge, const QRect& screen, int preferred, int depth)
{
    const int half = edgeWidth(depth) / 2;
    switch (edge) {
    case Edge::Top:
    case Edge::Bottom:
        return qBound(screen.left() + half, preferred, screen.right() - half + 1);
    case Edge::Left:
    case Edge::Right:
        return qBound(screen.top() + half, preferred, screen.bottom() - half + 1);
    default:
        return preferred;
    }
}

EdgeBand makeBand(Edge edge, const QRect& screen, int alongHint, int depth)
{
    EdgeBand b;
    b.edge = edge;
    b.screen = screen;
    b.depth = depth;
    const qreal d = depth;
    const int w = edgeWidth(depth);
    const qreal halfW = w / 2.0;
    const int along = clampAlong(edge, screen, alongHint, depth);
    b.along = along;

    switch (edge) {
    case Edge::Top:
        b.fullEllipse = QRectF(along - halfW, screen.top() - d, w, d * 2.0);
        b.onScreen = QRectF(along - halfW, screen.top(), w, d);
        break;
    case Edge::Bottom:
        b.fullEllipse = QRectF(along - halfW, screen.bottom() - d + 1, w, d * 2.0);
        b.onScreen = QRectF(along - halfW, screen.bottom() - d + 1, w, d);
        break;
    case Edge::Left:
        b.fullEllipse = QRectF(screen.left() - d, along - halfW, d * 2.0, w);
        b.onScreen = QRectF(screen.left(), along - halfW, d, w);
        break;
    case Edge::Right:
        b.fullEllipse = QRectF(screen.right() - d + 1, along - halfW, d * 2.0, w);
        b.onScreen = QRectF(screen.right() - d + 1, along - halfW, d, w);
        break;
    case Edge::TopLeft:
        b.fullEllipse = QRectF(screen.left() - d, screen.top() - d, d * 2.0, d * 2.0);
        b.onScreen = QRectF(screen.left(), screen.top(), d, d);
        break;
    case Edge::TopRight:
        b.fullEllipse = QRectF(screen.right() - d + 1, screen.top() - d, d * 2.0, d * 2.0);
        b.onScreen = QRectF(screen.right() - d + 1, screen.top(), d, d);
        break;
    case Edge::BottomLeft:
        b.fullEllipse = QRectF(screen.left() - d, screen.bottom() - d + 1, d * 2.0, d * 2.0);
        b.onScreen = QRectF(screen.left(), screen.bottom() - d + 1, d, d);
        break;
    case Edge::BottomRight:
        b.fullEllipse = QRectF(screen.right() - d + 1, screen.bottom() - d + 1, d * 2.0, d * 2.0);
        b.onScreen = QRectF(screen.right() - d + 1, screen.bottom() - d + 1, d, d);
        break;
    }
    b.hit = b.onScreen.toRect();
    return b;
}

QRectF progressStrip(const EdgeBand& band, double progress)
{
    const qreal t = qBound(0.0, progress, 1.0);
    if (t <= 0.0) {
        return {};
    }
    const qreal fill = band.depth * t;
    const QRectF& o = band.onScreen;
    switch (band.edge) {
    case Edge::Top:
        return QRectF(o.left(), o.top(), o.width(), fill);
    case Edge::Bottom:
        return QRectF(o.left(), o.bottom() - fill + 1, o.width(), fill);
    case Edge::Left:
        return QRectF(o.left(), o.top(), fill, o.height());
    case Edge::Right:
        return QRectF(o.right() - fill + 1, o.top(), fill, o.height());
    case Edge::TopLeft:
        return QRectF(o.left(), o.top(), fill, fill);
    case Edge::TopRight:
        return QRectF(o.right() - fill + 1, o.top(), fill, fill);
    case Edge::BottomLeft:
        return QRectF(o.left(), o.bottom() - fill + 1, fill, fill);
    case Edge::BottomRight:
        return QRectF(o.right() - fill + 1, o.bottom() - fill + 1, fill, fill);
    }
    return {};
}

QSize resolveRegionSize(const LayoutDwellRegion& region, const QRect& ref)
{
    const int w = region.width.resolveInt(ref.width(), 80);
    const int h = region.height.resolveInt(ref.height(), 80);
    return {qMax(1, w), qMax(1, h)};
}

QPoint resolveRegionOffset(const LayoutDwellRegion& region, const QRect& ref,
                           bool useMarginFallback)
{
    int ox = region.x.isSet() ? region.x.resolveInt(ref.width(), 0) : 0;
    int oy = region.y.isSet() ? region.y.resolveInt(ref.height(), 0) : 0;
    if (useMarginFallback && !region.x.isSet() && !region.y.isSet() && region.marginPx != 0) {
        return {0, 0};
    }
    return {ox, oy};
}

QRect logicalFromAnchor(const LayoutDwellRegion& region, const QRect& screen)
{
    if (!region.isValid() || !screen.isValid()) {
        return {};
    }
    const QSize sz = resolveRegionSize(region, screen);
    const int w = sz.width();
    const int h = sz.height();
    const int m = qMax(0, region.marginPx);
    const bool hasXY = region.x.isSet() || region.y.isSet();
    const QPoint off = resolveRegionOffset(region, screen, /*useMarginFallback=*/!hasXY);

    using SA = LayoutDwellRegion::ScreenAnchor;
    int baseX = 0;
    int baseY = 0;
    switch (region.screenAnchor) {
    case SA::Top:
    case SA::TopCenter:
        baseX = screen.center().x() - w / 2;
        baseY = hasXY ? screen.top() : (screen.top() - h - m);
        break;
    case SA::Bottom:
    case SA::BottomCenter:
        baseX = screen.center().x() - w / 2;
        baseY = hasXY ? (screen.bottom() + 1) : (screen.bottom() + 1 + m);
        break;
    case SA::Left:
    case SA::LeftCenter:
        baseX = hasXY ? screen.left() : (screen.left() - w - m);
        baseY = screen.center().y() - h / 2;
        break;
    case SA::Right:
    case SA::RightCenter:
        baseX = hasXY ? (screen.right() + 1) : (screen.right() + 1 + m);
        baseY = screen.center().y() - h / 2;
        break;
    case SA::TopLeft:
        baseX = hasXY ? screen.left() : (screen.left() - w - m);
        baseY = hasXY ? screen.top() : (screen.top() - h - m);
        break;
    case SA::TopRight:
        baseX = hasXY ? (screen.right() + 1 - w) : (screen.right() + 1 + m);
        baseY = hasXY ? screen.top() : (screen.top() - h - m);
        break;
    case SA::BottomLeft:
        baseX = hasXY ? screen.left() : (screen.left() - w - m);
        baseY = hasXY ? (screen.bottom() + 1 - h) : (screen.bottom() + 1 + m);
        break;
    case SA::BottomRight:
        baseX = hasXY ? (screen.right() + 1 - w) : (screen.right() + 1 + m);
        baseY = hasXY ? (screen.bottom() + 1 - h) : (screen.bottom() + 1 + m);
        break;
    case SA::None:
        return {};
    }
    return QRect(baseX + off.x(), baseY + off.y(), w, h);
}

QRect logicalFromBoardLocal(const LayoutDwellRegion& region, const QPoint& boardOrigin,
                            const QSize& boardSize)
{
    if (!region.isValid()) {
        return {};
    }
    const QRect ref(0, 0, boardSize.width() > 0 ? boardSize.width() : 1,
                    boardSize.height() > 0 ? boardSize.height() : 1);
    const QSize sz = resolveRegionSize(region, ref);
    const int ox = region.x.isSet() ? region.x.resolveInt(ref.width(), 0) : 0;
    const int oy = region.y.isSet() ? region.y.resolveInt(ref.height(), 0) : 0;
    return QRect(boardOrigin + QPoint(ox, oy), sz);
}

int alongHintFor(Edge edge, const QPoint& c)
{
    return (edge == Edge::Left || edge == Edge::Right) ? c.y() : c.x();
}

EdgeBand bandForTarget(Edge edge, const QRect& screen, const QRect& target)
{
    return makeBand(edge, screen, alongHintFor(edge, target.center()), kDepth);
}

QPoint coercePointOntoScreen(const QPoint& p, const QRect& screen)
{
    if (!screen.isValid()) {
        return p;
    }
    return {qBound(screen.left(), p.x(), screen.right()),
            qBound(screen.top(), p.y(), screen.bottom())};
}

Edge progressEdgeFor(const QRect& logical, const QRect& screen)
{
    const QPoint c = logical.center();
    const int outL = qMax(0, screen.left() - c.x());
    const int outR = qMax(0, c.x() - screen.right());
    const int outT = qMax(0, screen.top() - c.y());
    const int outB = qMax(0, c.y() - screen.bottom());
    const int totalOut = outL + outR + outT + outB;
    if (totalOut <= 0) {
        const Edge n = nearestEdge(logical, screen);
        switch (n) {
        case Edge::TopLeft:
        case Edge::TopRight:
            return Edge::Top;
        case Edge::BottomLeft:
        case Edge::BottomRight:
            return Edge::Bottom;
        case Edge::Left:
        case Edge::Right:
        case Edge::Top:
        case Edge::Bottom:
            return n;
        }
        return Edge::Bottom;
    }
    Edge e = Edge::Bottom;
    int best = outB;
    if (outT > best) {
        best = outT;
        e = Edge::Top;
    }
    if (outL > best) {
        best = outL;
        e = Edge::Left;
    }
    if (outR > best) {
        e = Edge::Right;
    }
    return e;
}

EdgeBand bandForCoercedProgress(const QRect& logical, const QRect& screen)
{
    const QPoint coerced = coercePointOntoScreen(logical.center(), screen);
    const Edge e = progressEdgeFor(logical, screen);
    return makeBand(e, screen, alongHintFor(e, coerced), kDepth);
}

Resolved resolveFromLogical(const QRect& logical, const QRect& boardScreenRect, bool boardless,
                            bool hasPreferredEdge, Edge preferredEdge)
{
    Resolved r;
    r.logical = logical;
    if (logical.isEmpty()) {
        return r;
    }

    const QRect desktop = virtualDesktop();
    QScreen* scr = nearestScreen(logical.center());
    if (!scr && !desktop.isEmpty()) {
        scr = nearestScreen(desktop.center());
    }
    if (!scr) {
        r.hit = logical;
        r.showBubble = boardless;
        return r;
    }
    const QRect screen = scr->geometry();
    const bool off = mostlyOffDisplay(logical, desktop);
    const bool outsideBoard =
        boardScreenRect.isEmpty() || !boardScreenRect.intersects(logical);

    r.edge = hasPreferredEdge ? preferredEdge : nearestEdge(logical, screen);
    r.band = bandForCoercedProgress(logical, screen);
    r.hit = logical;
    r.showBubble = boardless || outsideBoard || off;
    return r;
}

Resolved resolveItem(const LayoutItem& item, const QPoint& boardOrigin,
                     const QRect& boardScreenRect, QScreen* boardScreen)
{
    if (!item.hasDwellRegion || !item.dwellRegion.isValid()) {
        return {};
    }

    const bool boardless = item.unbounded || item.hasDwellRegion;

    if (item.dwellRegion.usesScreenAnchor()) {
        QRect screenGeo = boardScreen ? boardScreen->geometry() : QRect();
        if (!screenGeo.isValid()) {
            if (QScreen* s = nearestScreen(boardScreenRect.center())) {
                screenGeo = s->geometry();
            }
        }
        const QRect logical = logicalFromAnchor(item.dwellRegion, screenGeo);
        return resolveFromLogical(logical, boardScreenRect, /*boardless=*/true,
                                  /*hasPreferredEdge=*/true,
                                  edgeFromAnchor(item.dwellRegion.screenAnchor));
    }

    const QSize boardSize = boardScreenRect.isValid() ? boardScreenRect.size() : QSize();
    const QRect logical = logicalFromBoardLocal(item.dwellRegion, boardOrigin, boardSize);
    return resolveFromLogical(logical, boardScreenRect, boardless);
}

} // namespace DwellRegionSpace
} // namespace gazer
