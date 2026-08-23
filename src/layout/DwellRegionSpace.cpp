#include "layout/DwellRegionSpace.h"

#include <QtGlobal>

namespace gazer {
namespace DwellRegionSpace {

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

int alongHintFor(Edge edge, const QPoint& c)
{
    return (edge == Edge::Left || edge == Edge::Right) ? c.y() : c.x();
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

} // namespace DwellRegionSpace
} // namespace gazer
