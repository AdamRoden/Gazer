#pragma once

#include "layout/LayoutTypes.h"

#include <QPoint>
#include <QRect>
#include <QRectF>
#include <QScreen>
#include <QSize>

namespace gazer {
namespace DwellRegionSpace {

/// Shared edge-affordance geometry: logical target, gaze hit band, and bubble paint
/// all derive from one EdgeBand so they cannot drift.
/// Implementations live in DwellRegionSpace.cpp.

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

[[nodiscard]] QRect virtualDesktop();
[[nodiscard]] QScreen* nearestScreen(const QPoint& p);
[[nodiscard]] bool mostlyOffDisplay(const QRect& target, const QRect& desktop);

// --- edge classification -----------------------------------------------------

[[nodiscard]] Edge nearestEdge(const QRect& target, const QRect& screen);
[[nodiscard]] Edge edgeFromAnchor(LayoutDwellRegion::ScreenAnchor a);

// --- band geometry (single source for paint + hit) ---------------------------

[[nodiscard]] int edgeWidth(int depth = kDepth);
[[nodiscard]] int clampAlong(Edge edge, const QRect& screen, int preferred, int depth);
[[nodiscard]] EdgeBand makeBand(Edge edge, const QRect& screen, int alongHint,
                                int depth = kDepth);
[[nodiscard]] QRectF progressStrip(const EdgeBand& band, double progress);

// --- logical placement -------------------------------------------------------

[[nodiscard]] QSize resolveRegionSize(const LayoutDwellRegion& region, const QRect& ref);
[[nodiscard]] QPoint resolveRegionOffset(const LayoutDwellRegion& region, const QRect& ref,
                                         bool useMarginFallback);
[[nodiscard]] QRect logicalFromAnchor(const LayoutDwellRegion& region, const QRect& screen);
[[nodiscard]] QRect logicalFromBoardLocal(const LayoutDwellRegion& region,
                                          const QPoint& boardOrigin,
                                          const QSize& boardSize = QSize());

// --- resolve -----------------------------------------------------------------

[[nodiscard]] int alongHintFor(Edge edge, const QPoint& c);
[[nodiscard]] EdgeBand bandForTarget(Edge edge, const QRect& screen, const QRect& target);
[[nodiscard]] QPoint coercePointOntoScreen(const QPoint& p, const QRect& screen);
[[nodiscard]] Edge progressEdgeFor(const QRect& logical, const QRect& screen);
[[nodiscard]] EdgeBand bandForCoercedProgress(const QRect& logical, const QRect& screen);
[[nodiscard]] Resolved resolveFromLogical(const QRect& logical, const QRect& boardScreenRect,
                                          bool boardless, bool hasPreferredEdge = false,
                                          Edge preferredEdge = Edge::Bottom);
[[nodiscard]] Resolved resolveItem(const LayoutItem& item, const QPoint& boardOrigin,
                                   const QRect& boardScreenRect, QScreen* boardScreen);

} // namespace DwellRegionSpace
} // namespace gazer
