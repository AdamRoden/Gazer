#pragma once

#include "layout/PageDetector.h"
#include "layout/PageTypes.h"

#include <QPoint>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QSet>
#include <QString>
#include <QTransform>
#include <QVariantMap>
#include <QVector>

namespace gazer {

struct PageTarget {
    enum class Kind { Cell, Zone };

    Kind kind = Kind::Cell;
    QString pageId;
    QString gridId;
    QString id;
    QString label;
    QString icon;
    QString caption;
    QString textStyle;
    QString role;
    bool dwellExempt = false;
    bool interactive = true;
    bool shell = false;
    bool drawerMotion = false;
    QString cluster;
    QString clusterSlot;
    QString activeState;
    PageChrome chrome;
    PageDwell dwell;
    QVector<PageAction> actions;
    PageDetectorGeom geom;
    bool actionLoop = false;
};

[[nodiscard]] inline QString sessionKey(const PageTarget& t)
{
    if (t.pageId.isEmpty() || t.id.startsWith(t.pageId + QLatin1Char('/'))) {
        return t.id;
    }
    return t.pageId + QLatin1Char('/') + t.id;
}

struct PageFrame {
    QRectF screen;
    QRectF desktop;
};

struct PageGridPaint {
    QRectF visual;
    PageChrome chrome;
    QString pageId;
    QString gridId;
    bool drawerMotion = false;
    bool shell = false;
};

namespace PageHit {

[[nodiscard]] QRectF cellRect(const PageGrid& grid, const QRectF& gridRect, int row, int col,
                              int rowSpan, int colSpan);

[[nodiscard]] QRectF gridBounds(const PageGrid& grid, const PageFrame& frame);

/// Column in x, row in y. {-1,-1} if pos is outside the grid rect.
[[nodiscard]] QPoint cellIndexAt(const PageGrid& grid, const QRectF& gridRect, const QPointF& pos);

/// Front-to-back paint order. Hit-test walks this in reverse (topmost first).
/// Zones are appended after grids so they win, matching root chips over the drawer.
[[nodiscard]] QVector<PageTarget> collect(const PageDocument& page, const PageFrame& frame,
                                          const QSet<QString>& hiddenGrids = {},
                                          const QSet<QString>& hiddenZones = {},
                                          const QVariantMap& props = {},
                                          bool dwellSuspended = false,
                                          QVector<PageGridPaint>* grids = nullptr);

/// Topmost non-shell board whose visual contains pos. Empty if none.
[[nodiscard]] QString coveringPageId(const QVector<PageGridPaint>& grids, const QPointF& pos,
                                     double drawerScale = 1.0, const QVector<PageTarget>& targets = {},
                                     const QTransform* xf = nullptr);

[[nodiscard]] const PageTarget* at(const QVector<PageTarget>& targets, const QPointF& gaze,
                                   double drawerScale = 1.0, const QString& engagedId = {},
                                   const QVector<PageGridPaint>& grids = {},
                                   const QTransform* xf = nullptr);
/// Mouse / on-screen chrome (progressZone), not off-screen dwell.
[[nodiscard]] const PageTarget* atProgress(const QVector<PageTarget>& targets, const QPointF& pos,
                                           double drawerScale = 1.0, const QString& engagedId = {},
                                           const QVector<PageGridPaint>& grids = {},
                                           const QTransform* xf = nullptr);

/// Identity when scale is ~1. Pivot is the union of drawerMotion visuals, about bottom-center.
[[nodiscard]] QTransform drawerTransform(const QVector<PageTarget>& targets, double scale,
                                         const QVector<PageGridPaint>& grids = {});
[[nodiscard]] QRectF mapDrawer(bool drawerMotion, const QRectF& r, const QTransform& xf,
                               double scale);
[[nodiscard]] QRectF mapDrawer(const PageTarget& t, const QRectF& r, const QTransform& xf,
                               double scale);

/// Union of painted chrome in global coords (grids, on-screen content, progress strips).
[[nodiscard]] QRectF paintBounds(const QVector<PageTarget>& targets,
                                 const QVector<PageGridPaint>& grids, double drawerScale = 1.0);
[[nodiscard]] QRectF frostedBounds(const QVector<PageTarget>& targets,
                                   const QVector<PageGridPaint>& grids, double drawerScale = 1.0);

[[nodiscard]] bool shapeContains(const QRectF& r, const PageChrome& chrome, bool clustered,
                                 const QPointF& pos);

/// Scan: dwell AABB. After scan grace (`engagedId` matches): dwell ∪ unrounded
/// progress ∪ the straight-line gap joining closest corners.
[[nodiscard]] QPolygonF gazeHitPolygon(const PageTarget& t, const QString& engagedId = {});
[[nodiscard]] QRectF gazeHitRect(const PageTarget& t, const QString& engagedId = {});

} // namespace PageHit
} // namespace gazer
