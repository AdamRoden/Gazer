#pragma once

#include "layout/PageDetector.h"
#include "layout/PageTypes.h"

#include <QHash>
#include <QPoint>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QString>
#include <QTransform>
#include <QVariantMap>
#include <QVector>
#include <optional>

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
    QString settingKey;
    bool suspendExempt = false;
    bool interactive = true;
    bool shell = false;
    /// Live root (main master) page. Master paints and hits in front of attached pages.
    bool master = false;
    bool drawerMotion = false;
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

[[nodiscard]] inline QString localIdOf(const PageTarget& t)
{
    if (!t.pageId.isEmpty() && t.id.startsWith(t.pageId + QLatin1Char('/'))) {
        return t.id.mid(t.pageId.size() + 1);
    }
    return t.id;
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
    bool master = false;
};

namespace PageHit {

[[nodiscard]] QRectF cellRect(const PageGrid& grid, const QRectF& gridRect, int row, int col,
                              int rowSpan, int colSpan);

[[nodiscard]] QRectF gridBounds(const PageGrid& grid, const PageFrame& frame);

/// Column in x, row in y. {-1,-1} if pos is outside the grid rect.
[[nodiscard]] QPoint cellIndexAt(const PageGrid& grid, const QRectF& gridRect, const QPointF& pos);

/// Per-page collect order: grid cells, then zones. Live session appends each
/// page as one layer (attached oldest→newest, then master). Front-to-back:
/// master (zones, then grids/cells), then each open page the same way.
/// `shownLayers` unset uses `page.showLayers`. Pass a set to preview another
/// visible set (editor layer filter) without mutating the document.
[[nodiscard]] QVector<PageTarget> collect(const PageDocument& page, const PageFrame& frame,
                                          const QVariantMap& props = {},
                                          bool dwellSuspended = false,
                                          QVector<PageGridPaint>* grids = nullptr,
                                          bool includeDrawerMotion = false,
                                          const std::optional<QVector<int>>& shownLayers = std::nullopt);

/// Topmost painted grid whose visual contains pos (master included). Null if none.
[[nodiscard]] const PageGridPaint* coveringGrid(const QVector<PageGridPaint>& grids,
                                                const QPointF& pos, double drawerScale = 1.0,
                                                const QVector<PageTarget>& targets = {},
                                                const QTransform* xf = nullptr);
[[nodiscard]] QString coveringPageId(const QVector<PageGridPaint>& grids, const QPointF& pos,
                                     double drawerScale = 1.0, const QVector<PageTarget>& targets = {},
                                     const QTransform* xf = nullptr);

/// First-seen pageId order (back→front). Higher value is in front.
[[nodiscard]] inline QHash<QString, int> pageStackOrder(const QVector<PageTarget>& targets,
                                                        const QVector<PageGridPaint>& grids = {})
{
    QHash<QString, int> z;
    auto note = [&](const QString& id) {
        if (id.isEmpty() || z.contains(id)) {
            return;
        }
        z.insert(id, z.size());
    };
    for (const PageTarget& t : targets) {
        note(t.pageId);
    }
    for (const PageGridPaint& g : grids) {
        note(g.pageId);
    }
    return z;
}

/// True when @p cover is a grid of a page in front of @p t. Same-page grids do
/// not bury their own cells. A behind-page grid does not bury a front page.
[[nodiscard]] inline bool buriedByCover(const PageTarget& t, const PageGridPaint* cover,
                                        const QHash<QString, int>& stack)
{
    if (!cover || cover->pageId == t.pageId) {
        return false;
    }
    return stack.value(cover->pageId, -1) > stack.value(t.pageId, -1);
}

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
/// Unscaled chrome union for the host window. Drawer scale must not move this, or
/// non-drawer boards jump/shake while the drawer animates.
[[nodiscard]] inline QRectF hostBounds(const QVector<PageTarget>& targets,
                                       const QVector<PageGridPaint>& grids)
{
    return paintBounds(targets, grids, 1.0);
}
/// Authored grid/zone boxes, including off-layer items (except hidden shell grids, which
/// must not reserve host space). Host geometry keeps this space so ShowLayers
/// does not move already-visible boards.
[[nodiscard]] QRectF reservedBounds(const PageDocument& page, const PageFrame& frame);
/// Screen area frosted chrome occupies after any motion (drawer scale, etc.)
/// completes. Capture must use this rest pose, not the in-flight bounds.
[[nodiscard]] QRectF frostedBounds(const QVector<PageTarget>& targets,
                                   const QVector<PageGridPaint>& grids);

[[nodiscard]] bool shapeContains(const QRectF& r, const PageChrome& chrome,
                                 const QPointF& pos);

/// Scan: dwell AABB. After scan grace (`engagedId` matches): dwell ∪ unrounded
/// progress ∪ the straight-line gap joining closest corners.
[[nodiscard]] QPolygonF gazeHitPolygon(const PageTarget& t, const QString& engagedId = {});
[[nodiscard]] QRectF gazeHitRect(const PageTarget& t, const QString& engagedId = {});

} // namespace PageHit
} // namespace gazer
