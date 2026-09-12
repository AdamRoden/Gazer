#pragma once

#include <QColor>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QtMath>

namespace gazer {
namespace ComboMouseHit {

inline constexpr int kSliceCount = 5;
inline constexpr double kSliceDeg = 360.0 / double(kSliceCount);
inline constexpr double kCornerOuterScale = 2.0;
/// Defaults: inner drift-ring radius, shared radius, outer command-pie radius.
inline constexpr int kMinInnerRadiusPx = 20;
inline constexpr int kMaxInnerRadiusPx = 200;
inline constexpr int kMinSharedRadiusPx = 60;
inline constexpr int kMaxSharedRadiusPx = 400;
inline constexpr int kMinOuterRadiusPx = 120;
inline constexpr int kMaxOuterRadiusPx = 600;
inline constexpr int kDefaultInnerRadiusPx = 40;
inline constexpr int kDefaultSharedRadiusPx = 100;
inline constexpr int kDefaultOuterRadiusPx = 200;
inline constexpr double kHoleRadiusPx = double(kDefaultInnerRadiusPx);
inline constexpr double kRingOuterPx = double(kDefaultSharedRadiusPx);
inline constexpr double kPieOuterPx = double(kDefaultOuterRadiusPx);
inline constexpr double kMinInnerThicknessPx = 20.0;
inline constexpr double kMinOuterThicknessPx = 40.0;
inline const QColor kDefaultInnerFill{255, 196, 40, 51};  // 20%
inline const QColor kDefaultOuterFill{12, 14, 18, 153};   // 60%

/// Command ids. ComboMouse visual order is per-region in makeComboLayout.
enum class Slice { Right = 0, Move, Cancel, Drag, Left };

enum class Band { None, Deadzone, Drift, Slice };

struct Result {
    Band band = Band::None;
    Slice slice = Slice::Right;
};

/// One command button: polar annulus sector, clockwise from 12 o'clock.
struct Wedge {
    Slice id = Slice::Right;
    double inner = 0.0;
    double outer = 0.0;
    double startCw = 0.0;
    double spanDeg = 0.0;
};

struct Layout {
    double deadzone = kHoleRadiusPx;
    double ringOuter = kRingOuterPx;
    double pieOuter = kPieOuterPx;
    /// Visible interior sector, clockwise from 12 o'clock — not ComboMouse fill origin.
    double arcStartDeg = 0.0;
    double arcSpanDeg = 360.0;
    int wedgeCount = 0;
    Wedge wedges[kSliceCount];
};

[[nodiscard]] inline double wrap360(double deg)
{
    while (deg < 0.0) {
        deg += 360.0;
    }
    while (deg >= 360.0) {
        deg -= 360.0;
    }
    return deg;
}

/// 0° at 12 o'clock, increasing clockwise, in [0, 360).
[[nodiscard]] inline double clockwiseFromTopDeg(QPointF gaze, QPointF origin)
{
    const double dx = gaze.x() - origin.x();
    const double dy = gaze.y() - origin.y();
    return wrap360(qRadiansToDegrees(qAtan2(dy, dx)) + 90.0);
}

/// Snap a gaze offset to one of eight unit steps (N/NE/E/SE/S/SW/W/NW).
[[nodiscard]] inline QPoint snap8(QPointF dir)
{
    const double len = qSqrt(dir.x() * dir.x() + dir.y() * dir.y());
    if (len < 0.01) {
        return {};
    }
    const double ang = qAtan2(dir.y(), dir.x());
    int oct = int(qRound(ang / qDegreesToRadians(45.0)));
    oct = ((oct % 8) + 8) % 8;
    static const int dx[8] = {1, 1, 0, -1, -1, -1, 0, 1};
    static const int dy[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    return {dx[oct], dy[oct]};
}

[[nodiscard]] inline QPointF pointOnRay(QPointF origin, double clockwiseFromTop, double radius)
{
    const double rad = qDegreesToRadians(clockwiseFromTop - 90.0);
    return QPointF(origin.x() + qCos(rad) * radius, origin.y() + qSin(rad) * radius);
}

[[nodiscard]] inline bool packingEqual(const Layout& a, const Layout& b)
{
    return qFuzzyCompare(a.arcStartDeg + 1.0, b.arcStartDeg + 1.0)
           && qFuzzyCompare(a.arcSpanDeg + 1.0, b.arcSpanDeg + 1.0)
           && qFuzzyCompare(a.deadzone + 1.0, b.deadzone + 1.0)
           && qFuzzyCompare(a.ringOuter + 1.0, b.ringOuter + 1.0)
           && qFuzzyCompare(a.pieOuter + 1.0, b.pieOuter + 1.0)
           && a.wedgeCount == b.wedgeCount;
}

inline void clampRadii(double& inner, double& shared, double& outer)
{
    inner = qBound(double(kMinInnerRadiusPx), inner, double(kMaxInnerRadiusPx));
    shared = qBound(double(kMinSharedRadiusPx), shared, double(kMaxSharedRadiusPx));
    outer = qBound(double(kMinOuterRadiusPx), outer, double(kMaxOuterRadiusPx));
    if (shared < inner + kMinInnerThicknessPx) {
        shared = qMin(double(kMaxSharedRadiusPx), inner + kMinInnerThicknessPx);
    }
    if (outer < shared + kMinOuterThicknessPx) {
        outer = qMin(double(kMaxOuterRadiusPx), shared + kMinOuterThicknessPx);
    }
    if (shared > outer - kMinOuterThicknessPx) {
        shared = qMax(double(kMinSharedRadiusPx), outer - kMinOuterThicknessPx);
    }
    if (inner > shared - kMinInnerThicknessPx) {
        inner = qMax(double(kMinInnerRadiusPx), shared - kMinInnerThicknessPx);
    }
}

[[nodiscard]] inline const Wedge* wedgeById(const Layout& L, Slice id)
{
    for (int i = 0; i < L.wedgeCount; ++i) {
        if (L.wedges[i].id == id) {
            return &L.wedges[i];
        }
    }
    return nullptr;
}

inline void addWedge(Layout& L, Slice id, double inner, double outer, double startCw, double spanDeg)
{
    if (L.wedgeCount >= kSliceCount) {
        return;
    }
    L.wedges[L.wedgeCount++] = Wedge{id, inner, outer, startCw, spanDeg};
}

inline void fillBand(Layout& L, double inner, double outer, double startCw, double spanDeg,
                     bool clockwise, const Slice order[kSliceCount])
{
    const double slice = spanDeg / double(kSliceCount);
    for (int i = 0; i < kSliceCount; ++i) {
        double start = clockwise ? startCw + double(i) * slice
                                 : startCw - double(i + 1) * slice;
        // Exclusive-end hit-test: nudge CCW wedges so fill origin sits inside [start, start+span).
        if (!clockwise) {
            start += 1e-6;
        }
        addWedge(L, order[i], inner, outer, wrap360(start), slice);
    }
}

inline void fillClockwiseBand(Layout& L, double inner, double outer, double startCw, double spanDeg)
{
    const Slice order[kSliceCount] = {Slice::Right, Slice::Move, Slice::Cancel, Slice::Drag,
                                      Slice::Left};
    fillBand(L, inner, outer, startCw, spanDeg, true, order);
}

enum class Region : int { Full = 0, Left, Right, Top, Bottom, TL, TR, BR, BL };

inline constexpr int kRegionCount = int(Region::BL) + 1;

struct RegionArc {
    double startDeg;
    double spanDeg;
};

/// Visible interior sector, indexed by Region.
inline constexpr RegionArc kRegionArc[kRegionCount] = {
    {0.0, 360.0},   // Full
    {0.0, 180.0},   // Left
    {180.0, 180.0}, // Right
    {90.0, 180.0},  // Top
    {270.0, 180.0}, // Bottom
    {90.0, 90.0},   // TL
    {180.0, 90.0},  // TR
    {270.0, 90.0},  // BR
    {0.0, 90.0},    // BL
};

struct ComboPack {
    double fillStartDeg;
    bool clockwise;
    double outerScale;
    Slice order[kSliceCount];
};

/// ComboMouse fill, indexed by Region. Edges/corners share left, right, drag, move, cancel.
inline constexpr ComboPack kComboPack[kRegionCount] = {
    {0.0, true, 1.0, {Slice::Drag, Slice::Move, Slice::Cancel, Slice::Left, Slice::Right}},
    {0.0, true, 1.0, {Slice::Left, Slice::Right, Slice::Drag, Slice::Move, Slice::Cancel}},
    {0.0, false, 1.0, {Slice::Left, Slice::Right, Slice::Drag, Slice::Move, Slice::Cancel}},
    {270.0, false, 1.0, {Slice::Left, Slice::Right, Slice::Drag, Slice::Move, Slice::Cancel}},
    {270.0, true, 1.0, {Slice::Left, Slice::Right, Slice::Drag, Slice::Move, Slice::Cancel}},
    {180.0, false, kCornerOuterScale,
     {Slice::Left, Slice::Right, Slice::Drag, Slice::Move, Slice::Cancel}},
    {270.0, false, kCornerOuterScale,
     {Slice::Left, Slice::Right, Slice::Drag, Slice::Move, Slice::Cancel}},
    {270.0, true, kCornerOuterScale,
     {Slice::Left, Slice::Right, Slice::Drag, Slice::Move, Slice::Cancel}},
    {0.0, true, kCornerOuterScale,
     {Slice::Left, Slice::Right, Slice::Drag, Slice::Move, Slice::Cancel}},
};

[[nodiscard]] inline Region regionOf(bool nearL, bool nearR, bool nearT, bool nearB)
{
    if ((nearL && nearR) || (nearT && nearB)) {
        return Region::Full;
    }
    if (nearT && nearL) {
        return Region::TL;
    }
    if (nearT && nearR) {
        return Region::TR;
    }
    if (nearB && nearR) {
        return Region::BR;
    }
    if (nearB && nearL) {
        return Region::BL;
    }
    if (nearT) {
        return Region::Top;
    }
    if (nearR) {
        return Region::Right;
    }
    if (nearB) {
        return Region::Bottom;
    }
    if (nearL) {
        return Region::Left;
    }
    return Region::Full;
}

inline Region initSector(Layout& L, QPointF origin, const QRectF& screen, double& deadzone,
                         double& ringOuter, double& fullPieOuter)
{
    clampRadii(deadzone, ringOuter, fullPieOuter);
    L.deadzone = deadzone;
    L.ringOuter = ringOuter;
    L.pieOuter = fullPieOuter;
    const bool nearL = origin.x() - screen.left() < fullPieOuter;
    const bool nearR = screen.right() - origin.x() < fullPieOuter;
    const bool nearT = origin.y() - screen.top() < fullPieOuter;
    const bool nearB = screen.bottom() - origin.y() < fullPieOuter;
    const Region r = regionOf(nearL, nearR, nearT, nearB);
    L.arcStartDeg = kRegionArc[int(r)].startDeg;
    L.arcSpanDeg = kRegionArc[int(r)].spanDeg;
    return r;
}

[[nodiscard]] inline Layout makePackedLayout(QPointF origin, const QRectF& screen, double deadzone,
                                             double ringOuter, double fullPieOuter,
                                             const ComboPack packs[kRegionCount])
{
    Layout L;
    const Region r = initSector(L, origin, screen, deadzone, ringOuter, fullPieOuter);
    const ComboPack& pack = packs[int(r)];
    L.pieOuter = fullPieOuter * pack.outerScale;
    fillBand(L, ringOuter, L.pieOuter, pack.fillStartDeg, L.arcSpanDeg, pack.clockwise, pack.order);
    return L;
}

[[nodiscard]] inline Layout makeComboLayout(QPointF origin, const QRectF& screen, double deadzone,
                                            double ringOuter, double fullPieOuter)
{
    return makePackedLayout(origin, screen, deadzone, ringOuter, fullPieOuter, kComboPack);
}

[[nodiscard]] inline QRect overlayRect(QPointF origin, const Layout& L, const QRectF& screen)
{
    QRectF box(origin.x() - L.pieOuter, origin.y() - L.pieOuter, L.pieOuter * 2.0,
               L.pieOuter * 2.0);
    QRectF vis = box.adjusted(-10.0, -10.0, 10.0, 10.0).intersected(screen);
    if (vis.width() < 8.0 || vis.height() < 8.0) {
        vis = QRectF(origin.x() - 4.0, origin.y() - 4.0, 8.0, 8.0).intersected(screen);
    }
    const QRect aligned = vis.toAlignedRect();
    return aligned.intersected(QRect(qFloor(screen.left()), qFloor(screen.top()),
                                     qCeil(screen.width()), qCeil(screen.height())));
}

[[nodiscard]] inline bool inWedge(double dist, double deg, const Wedge& w, double pieOuter)
{
    if (dist < w.inner || dist > w.outer) {
        return false;
    }
    if (dist == w.outer && w.outer < pieOuter) {
        return false;
    }
    const double rel = wrap360(deg - w.startCw);
    return rel + 1e-9 < w.spanDeg;
}

[[nodiscard]] inline Result hit(QPointF gaze, QPointF origin, const Layout& L)
{
    Result r;
    const double dx = gaze.x() - origin.x();
    const double dy = gaze.y() - origin.y();
    const double dist = qSqrt(dx * dx + dy * dy);
    if (dist < L.deadzone) {
        r.band = Band::Deadzone;
        return r;
    }
    if (dist < L.ringOuter) {
        r.band = Band::Drift;
        return r;
    }
    if (L.pieOuter <= 0.0 || dist > L.pieOuter) {
        return r;
    }
    const double deg = clockwiseFromTopDeg(gaze, origin);
    for (int i = 0; i < L.wedgeCount; ++i) {
        if (inWedge(dist, deg, L.wedges[i], L.pieOuter)) {
            r.band = Band::Slice;
            r.slice = L.wedges[i].id;
            return r;
        }
    }
    return r;
}

[[nodiscard]] inline Result hit(QPointF gaze, QPointF origin, double deadzone, double ringOuter,
                                double pieOuter)
{
    Layout L;
    L.deadzone = deadzone;
    L.ringOuter = ringOuter;
    L.pieOuter = pieOuter;
    L.arcStartDeg = 0.0;
    L.arcSpanDeg = 360.0;
    fillClockwiseBand(L, ringOuter, pieOuter, 0.0, 360.0);
    return hit(gaze, origin, L);
}

} // namespace ComboMouseHit
} // namespace gazer
