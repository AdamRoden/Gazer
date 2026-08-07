#pragma once

#include "layout/LayoutTypes.h"

#include <QGuiApplication>
#include <QPoint>
#include <QRect>
#include <QRectF>
#include <QScreen>
#include <QtMath>

#include <climits>

namespace gazer {
namespace DwellRegionSpace {

/// Shared edge-affordance geometry: logical target, gaze hit band, and bubble paint
/// all derive from one EdgeBand so they cannot drift.

inline constexpr int kDepth = 72;
inline constexpr int kWidthMul = 4;

enum class Edge {
    Top,
    Bottom,
    Left,
    Right,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight
};

struct EdgeBand {
    Edge edge = Edge::Bottom;
    QRect screen;
    int depth = kDepth;
    int along = 0; // center along cardinal edge (widget/screen coords)
    QRectF fullEllipse;
    QRectF onScreen; // half/quarter visible on display
    QRect hit;       // integer gaze capture (== onScreen rounded)
};

struct Resolved {
    QRect logical;   // may be off-screen
    QRect hit;       // engage rect (logical; no free edge lip)
    bool showBubble = false;
    Edge edge = Edge::Bottom;
    EdgeBand band;   // edge bubble + optional post-engage drift lip

    /// After dwell has started, expand hit with the on-screen edge band so gaze
    /// can drift onto the display without losing the target.
    [[nodiscard]] QRect hitWithDriftLip(bool engaged) const
    {
        if (!engaged || band.hit.isEmpty()) {
            return hit;
        }
        return hit.united(band.hit);
    }
};

// --- screen helpers ----------------------------------------------------------

[[nodiscard]] inline QRect virtualDesktop()
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

[[nodiscard]] inline QScreen* nearestScreen(const QPoint& p)
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

[[nodiscard]] inline bool mostlyOffDisplay(const QRect& target, const QRect& desktop)
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

// --- edge classification -----------------------------------------------------

[[nodiscard]] inline Edge nearestEdge(const QRect& target, const QRect& screen)
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

[[nodiscard]] inline Edge edgeFromAnchor(LayoutDwellRegion::ScreenAnchor a)
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

// --- band geometry (single source for paint + hit) ---------------------------

[[nodiscard]] inline int edgeWidth(int depth = kDepth)
{
    return depth * kWidthMul;
}

[[nodiscard]] inline int clampAlong(Edge edge, const QRect& screen, int preferred, int depth)
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

[[nodiscard]] inline EdgeBand makeBand(Edge edge, const QRect& screen, int alongHint,
                                       int depth = kDepth)
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

/// Progress fill strip from screen edge toward interior (fraction 0..1 of depth).
[[nodiscard]] inline QRectF progressStrip(const EdgeBand& band, double progress)
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

// --- logical placement -------------------------------------------------------

/// All screen anchors place the logical rect fully outside the display edge.
[[nodiscard]] inline QRect logicalFromAnchor(const LayoutDwellRegion& region, const QRect& screen)
{
    if (!region.isValid() || !screen.isValid()) {
        return {};
    }
    const int m = qMax(0, region.marginPx);
    const int w = region.widthPx;
    const int h = region.heightPx;
    using SA = LayoutDwellRegion::ScreenAnchor;
    switch (region.screenAnchor) {
    case SA::Top:
    case SA::TopCenter:
        return QRect(screen.center().x() - w / 2, screen.top() - h - m, w, h);
    case SA::Bottom:
    case SA::BottomCenter:
        return QRect(screen.center().x() - w / 2, screen.bottom() + 1 + m, w, h);
    case SA::Left:
    case SA::LeftCenter:
        return QRect(screen.left() - w - m, screen.center().y() - h / 2, w, h);
    case SA::Right:
    case SA::RightCenter:
        return QRect(screen.right() + 1 + m, screen.center().y() - h / 2, w, h);
    case SA::TopLeft:
        return QRect(screen.left() - w - m, screen.top() - h - m, w, h);
    case SA::TopRight:
        return QRect(screen.right() + 1 + m, screen.top() - h - m, w, h);
    case SA::BottomLeft:
        return QRect(screen.left() - w - m, screen.bottom() + 1 + m, w, h);
    case SA::BottomRight:
        return QRect(screen.right() + 1 + m, screen.bottom() + 1 + m, w, h);
    case SA::None:
        break;
    }
    return {};
}

[[nodiscard]] inline QRect logicalFromBoardLocal(const LayoutDwellRegion& region,
                                                 const QPoint& boardOrigin)
{
    if (!region.isValid()) {
        return {};
    }
    return QRect(boardOrigin + QPoint(qRound(region.x), qRound(region.y)),
                 QSize(region.widthPx, region.heightPx));
}

// --- resolve -----------------------------------------------------------------

[[nodiscard]] inline int alongHintFor(Edge edge, const QPoint& c)
{
    return (edge == Edge::Left || edge == Edge::Right) ? c.y() : c.x();
}

[[nodiscard]] inline EdgeBand bandForTarget(Edge edge, const QRect& screen, const QRect& target)
{
    return makeBand(edge, screen, alongHintFor(edge, target.center()), kDepth);
}

/// @param preferredEdge  if set (screen-anchor), use it; else nearestEdge(logical).
[[nodiscard]] inline Resolved resolveFromLogical(const QRect& logical,
                                                 const QRect& boardScreenRect,
                                                 bool boardless,
                                                 bool hasPreferredEdge = false,
                                                 Edge preferredEdge = Edge::Bottom)
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
    r.band = bandForTarget(r.edge, screen, logical);

    // Hit is always the logical rect so marginPx fully controls how far off-screen
    // gaze must go. Do not union a free on-screen edge band — that made margin
    // 20/50 behave like the screen edge while only margin > kDepth felt "far".
    r.hit = logical;
    r.showBubble = boardless || outsideBoard || off;
    return r;
}

[[nodiscard]] inline Resolved resolveItem(const LayoutItem& item, const QPoint& boardOrigin,
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

    const QRect logical = logicalFromBoardLocal(item.dwellRegion, boardOrigin);
    return resolveFromLogical(logical, boardScreenRect, boardless);
}

} // namespace DwellRegionSpace
} // namespace gazer
