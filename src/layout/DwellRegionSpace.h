#pragma once

#include <QPoint>
#include <QRect>
#include <QRectF>

namespace gazer {
namespace DwellRegionSpace {

/// On-screen half/quarter ellipse used when a zone's visual sits off the display.
/// PageDetector maps that to progressZone so dwell can finish on-screen.

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
    int along = 0;
    QRectF fullEllipse;
    QRectF onScreen;
    QRect hit;
};

[[nodiscard]] Edge nearestEdge(const QRect& target, const QRect& screen);
[[nodiscard]] int edgeWidth(int depth = kDepth);
[[nodiscard]] int clampAlong(Edge edge, const QRect& screen, int preferred, int depth);
[[nodiscard]] EdgeBand makeBand(Edge edge, const QRect& screen, int alongHint, int depth = kDepth);
[[nodiscard]] int alongHintFor(Edge edge, const QPoint& c);
[[nodiscard]] QPoint coercePointOntoScreen(const QPoint& p, const QRect& screen);
[[nodiscard]] Edge progressEdgeFor(const QRect& logical, const QRect& screen);
[[nodiscard]] EdgeBand bandForCoercedProgress(const QRect& logical, const QRect& screen);

} // namespace DwellRegionSpace
} // namespace gazer
