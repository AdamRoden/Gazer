#pragma once

#include "layout/PageDetector.h"
#include "layout/PageTypes.h"

#include <QPointF>
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
};

struct PageFrame {
    QRectF screen;
    QRectF desktop;
};

struct PageGridPaint {
    QRectF visual;
    PageChrome chrome;
    QString pageId;
    bool drawerMotion = false;
    bool shell = false;
};

namespace PageHit {

[[nodiscard]] QRectF cellRect(const PageGrid& grid, const QRectF& gridRect, int row, int col,
                              int rowSpan, int colSpan);

[[nodiscard]] QRectF gridBounds(const PageGrid& grid, const PageFrame& frame);

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
                                     double drawerScale = 1.0, const QVector<PageTarget>& targets = {});

[[nodiscard]] const PageTarget* at(const QVector<PageTarget>& targets, const QPointF& gaze,
                                   double drawerScale = 1.0, const QString& engagedId = {},
                                   const QVector<PageGridPaint>& grids = {});
/// Mouse / on-screen chrome (progressZone), not off-screen dwell.
[[nodiscard]] const PageTarget* atProgress(const QVector<PageTarget>& targets, const QPointF& pos,
                                           double drawerScale = 1.0, const QString& engagedId = {},
                                           const QVector<PageGridPaint>& grids = {});

/// Identity when scale is ~1. Pivot is the union of drawerMotion visuals, about bottom-center.
[[nodiscard]] QTransform drawerTransform(const QVector<PageTarget>& targets, double scale,
                                         const QVector<PageGridPaint>& grids = {});
[[nodiscard]] QRectF mapDrawer(bool drawerMotion, const QRectF& r, const QTransform& xf,
                               double scale);
[[nodiscard]] QRectF mapDrawer(const PageTarget& t, const QRectF& r, const QTransform& xf,
                               double scale);

} // namespace PageHit
} // namespace gazer
