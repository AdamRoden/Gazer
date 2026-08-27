#pragma once

#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QtMath>

namespace gazer {
namespace ComboMouseHit {

inline constexpr int kSliceCount = 5;
inline constexpr double kSliceDeg = 360.0 / double(kSliceCount);
inline constexpr double kCornerInnerSliceDeg = 45.0;
inline constexpr double kCornerOuterSliceDeg = 30.0;
inline constexpr double kCornerArcSpanDeg = 90.0;
/// Hole / yellow-ring radii (120px and 240px diameters).
inline constexpr double kHoleRadiusPx = 60.0;
inline constexpr double kRingOuterPx = 120.0;
inline constexpr double kHalfPieScale = 1.5;

/// Clockwise from 12 o'clock: Right, Move, Cancel, Drag, Left.
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
    double pieOuter = 220.0;
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
           && qFuzzyCompare(a.pieOuter + 1.0, b.pieOuter + 1.0)
           && a.wedgeCount == b.wedgeCount;
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

inline void fillClockwiseBand(Layout& L, double inner, double outer, double startCw, double spanDeg)
{
    const double slice = spanDeg / double(kSliceCount);
    for (int i = 0; i < kSliceCount; ++i) {
        addWedge(L, Slice(i), inner, outer, startCw + double(i) * slice, slice);
    }
}

/// Interior command sector so the hole can sit on a screen edge. 360° when the
/// full pie fits; 180° on one edge; 90° in a corner.
inline void interiorArc(bool nearL, bool nearR, bool nearT, bool nearB, double& start,
                        double& span)
{
    start = 0.0;
    span = 360.0;
    if ((nearL && nearR) || (nearT && nearB)) {
        return;
    }
    if (nearT && nearL) {
        start = 90.0;
        span = 90.0;
        return;
    }
    if (nearT && nearR) {
        start = 180.0;
        span = 90.0;
        return;
    }
    if (nearB && nearR) {
        start = 270.0;
        span = 90.0;
        return;
    }
    if (nearB && nearL) {
        start = 0.0;
        span = 90.0;
        return;
    }
    if (nearT) {
        start = 90.0;
        span = 180.0;
        return;
    }
    if (nearR) {
        start = 180.0;
        span = 180.0;
        return;
    }
    if (nearB) {
        start = 270.0;
        span = 180.0;
        return;
    }
    if (nearL) {
        start = 0.0;
        span = 180.0;
    }
}

[[nodiscard]] inline Layout makeLayout(QPointF origin, const QRectF& screen, double deadzone,
                                       double ringOuter, double fullPieOuter)
{
    Layout L;
    L.deadzone = deadzone;
    L.ringOuter = ringOuter;
    L.pieOuter = fullPieOuter;
    const bool nearL = origin.x() - screen.left() < fullPieOuter;
    const bool nearR = screen.right() - origin.x() < fullPieOuter;
    const bool nearT = origin.y() - screen.top() < fullPieOuter;
    const bool nearB = screen.bottom() - origin.y() < fullPieOuter;
    interiorArc(nearL, nearR, nearT, nearB, L.arcStartDeg, L.arcSpanDeg);

    if (L.arcSpanDeg <= kCornerArcSpanDeg + 1e-6) {
        const double band = qBound(56.0, fullPieOuter - ringOuter, 100.0);
        const double innerOuter = ringOuter + band;
        L.pieOuter = innerOuter + band;
        const double s = L.arcStartDeg;
        addWedge(L, Slice::Right, ringOuter, innerOuter, s, kCornerInnerSliceDeg);
        addWedge(L, Slice::Left, ringOuter, innerOuter, s + kCornerInnerSliceDeg,
                 kCornerInnerSliceDeg);
        addWedge(L, Slice::Move, innerOuter, L.pieOuter, s, kCornerOuterSliceDeg);
        addWedge(L, Slice::Cancel, innerOuter, L.pieOuter, s + kCornerOuterSliceDeg,
                 kCornerOuterSliceDeg);
        addWedge(L, Slice::Drag, innerOuter, L.pieOuter, s + 2.0 * kCornerOuterSliceDeg,
                 kCornerOuterSliceDeg);
        return L;
    }
    if (L.arcSpanDeg < 360.0) {
        L.pieOuter = qMax(fullPieOuter * kHalfPieScale,
                          ringOuter + (fullPieOuter - ringOuter) * kHalfPieScale);
    }
    fillClockwiseBand(L, ringOuter, L.pieOuter, L.arcStartDeg, L.arcSpanDeg);
    return L;
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
