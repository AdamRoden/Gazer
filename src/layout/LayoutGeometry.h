#pragma once

#include "layout/LayoutTypes.h"

#include <QHash>
#include <QRectF>
#include <QString>
#include <QVector>
#include <algorithm>

namespace gazer {

/// Pure layout geometry: cell rects in a local coordinate system (0,0 = board top-left).
namespace LayoutGeometry {

[[nodiscard]] inline QRectF cellRect(const LayoutGrid& grid,
                                     int boardW,
                                     int boardH,
                                     int row,
                                     int col,
                                     int rowSpan,
                                     int colSpan)
{
    const int cols = grid.columns;
    const int rows = grid.rows;
    const int gap = grid.gapPx;
    const auto inset = grid.insets(boardW, boardH);

    const double innerW = boardW - inset.left - inset.right - gap * (cols - 1);
    const double innerH = boardH - inset.top - inset.bottom - gap * (rows - 1);
    if (cols < 1 || rows < 1 || innerW <= 0 || innerH <= 0) {
        return {};
    }

    const double cellW = innerW / cols;
    const double cellH = innerH / rows;
    const double x = inset.left + col * (cellW + gap);
    const double y = inset.top + row * (cellH + gap);
    const double w = cellW * colSpan + gap * (colSpan - 1);
    const double h = cellH * rowSpan + gap * (rowSpan - 1);
    return QRectF(x, y, w, h);
}

/// Voice OSK-style: each row independently sized by widthUnits (sum ≈ 12).
[[nodiscard]] inline QHash<QString, QRectF> itemRectsUnitRows(const LayoutDocument& layout,
                                                             int boardW,
                                                             int boardH)
{
    QHash<QString, QRectF> out;
    if (!layout.isValid() || layout.items.isEmpty()) {
        return out;
    }

    const auto inset = layout.grid.insets(boardW, boardH);
    const int gap = layout.grid.gapPx;
    const int rows = layout.grid.rows > 0 ? layout.grid.rows : 1;
    const double innerW = boardW - inset.left - inset.right;
    const double innerH = boardH - inset.top - inset.bottom - gap * (rows - 1);
    if (innerW <= 0 || innerH <= 0) {
        return out;
    }
    const double rowH = innerH / rows;

    // Group item indices by row (skip board-less unbounded / dwellRegion items).
    QHash<int, QVector<int>> byRow;
    for (int i = 0; i < layout.items.size(); ++i) {
        if (!layout.items[i].participatesInBoardGrid()) {
            continue;
        }
        byRow[layout.items[i].row].push_back(i);
    }

    for (auto it = byRow.begin(); it != byRow.end(); ++it) {
        const int row = it.key();
        QVector<int> idxs = it.value();
        std::sort(idxs.begin(), idxs.end(), [&](int a, int b) {
            return layout.items[a].col < layout.items[b].col;
        });

        double totalU = 0.0;
        for (int i : idxs) {
            double u = layout.items[i].widthUnits;
            if (u <= 0.0) {
                u = 1.0;
            }
            totalU += u;
        }
        if (totalU <= 0.0) {
            continue;
        }

        const int n = idxs.size();
        const double usableW = innerW - gap * qMax(0, n - 1);
        double x = inset.left;
        const double y = inset.top + row * (rowH + gap);

        for (int i : idxs) {
            double u = layout.items[i].widthUnits;
            if (u <= 0.0) {
                u = 1.0;
            }
            const double w = usableW * (u / totalU);
            out.insert(layout.items[i].id, QRectF(x, y, w, rowH));
            x += w + gap;
        }
    }
    return out;
}

inline void equalizeSegmentClusters(const LayoutDocument& layout, QHash<QString, QRectF>& rects);

[[nodiscard]] inline QHash<QString, QRectF> itemRects(const LayoutDocument& layout,
                                                     int boardW,
                                                     int boardH)
{
    if (!layout.isValid()) {
        return {};
    }

    bool useUnits = layout.grid.unitRows;
    if (!useUnits) {
        for (const LayoutItem& it : layout.items) {
            if (!it.participatesInBoardGrid()) {
                continue;
            }
            if (it.widthUnits > 0.0) {
                useUnits = true;
                break;
            }
        }
    }
    if (useUnits) {
        auto out = itemRectsUnitRows(layout, boardW, boardH);
        equalizeSegmentClusters(layout, out);
        return out;
    }

    QHash<QString, QRectF> out;
    out.reserve(layout.items.size());
    for (const LayoutItem& item : layout.items) {
        if (!item.participatesInBoardGrid()) {
            continue;
        }
        out.insert(item.id,
                   cellRect(layout.grid, boardW, boardH, item.row, item.col, item.rowSpan,
                            item.colSpan));
    }
    equalizeSegmentClusters(layout, out);
    return out;
}

inline void equalizeSegmentClusters(const LayoutDocument& layout, QHash<QString, QRectF>& rects)
{
    QHash<QString, QVector<QString>> byCluster;
    for (const LayoutItem& item : layout.items) {
        if (item.kind != LayoutItemKind::Segment || !item.participatesInBoardGrid()) {
            continue;
        }
        if (rects.value(item.id).isEmpty()) {
            continue;
        }
        byCluster[item.cluster].push_back(item.id);
    }
    for (auto it = byCluster.begin(); it != byCluster.end(); ++it) {
        QVector<QString> ids = it.value();
        if (ids.size() < 2) {
            continue;
        }
        std::sort(ids.begin(), ids.end(), [&](const QString& a, const QString& b) {
            return rects.value(a).left() < rects.value(b).left();
        });
        QRectF span;
        for (const QString& id : ids) {
            const QRectF r = rects.value(id);
            span = span.isEmpty() ? r : span.united(r);
        }
        if (span.width() < 2.0) {
            continue;
        }
        const double w = span.width() / double(ids.size());
        for (int i = 0; i < ids.size(); ++i) {
            rects.insert(ids[i], QRectF(span.left() + w * i, span.top(), w, span.height()));
        }
    }
}

/// Hit-test gaze in board-local coordinates.
[[nodiscard]] inline QString hitTestLocal(const QHash<QString, QRectF>& itemLocalRects,
                                          const QPointF& localPoint)
{
    for (auto it = itemLocalRects.constBegin(); it != itemLocalRects.constEnd(); ++it) {
        if (it.value().contains(localPoint)) {
            return it.key();
        }
    }
    return {};
}

/// Hit-test gaze in screen coordinates given the board's top-left in screen space.
[[nodiscard]] inline QString hitTestScreen(const QHash<QString, QRectF>& itemLocalRects,
                                           const QPoint& boardTopLeftScreen,
                                           const QPointF& screenPoint)
{
    const QPointF local(screenPoint.x() - boardTopLeftScreen.x(),
                        screenPoint.y() - boardTopLeftScreen.y());
    return hitTestLocal(itemLocalRects, local);
}

} // namespace LayoutGeometry
} // namespace gazer
