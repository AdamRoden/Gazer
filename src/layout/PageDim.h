#pragma once

#include "layout/PageTypes.h"

#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QStringView>

namespace gazer {

namespace PageDimParse {

[[nodiscard]] PageDim parse(const QString& token, QString* error = nullptr);
[[nodiscard]] PageDimPair parsePair(const QString& csv, QString* error = nullptr);
[[nodiscard]] QVector<int> parseIntList(const QString& csv, QString* error = nullptr);
[[nodiscard]] PageTrackSize parseTrack(const QString& token, QString* error = nullptr);
[[nodiscard]] QVector<PageTrackSize> parseTrackList(const QString& csv, QString* error = nullptr);
[[nodiscard]] PageAnchor parseAnchor(const QString& name, bool* ok = nullptr);
[[nodiscard]] QString anchorName(PageAnchor a);
[[nodiscard]] QString token(const PageDim& d);
[[nodiscard]] QString token(const PageTrackSize& t);
[[nodiscard]] QString tokenList(const QVector<PageTrackSize>& tracks);
[[nodiscard]] QPoint anchorDelta(PageAnchor a, int amount);
[[nodiscard]] bool strictBool(QStringView t, bool* out);

/// Place a box of `size` at `anchor` on `bounds`, then add `offset`.
/// @p screen feeds `A_ScreenWidth` / `A_ScreenHeight`. Empty uses `bounds`.
[[nodiscard]] QRectF placeRect(const QRectF& bounds, PageAnchor anchor, const PageDimPair& offset,
                               const PageDimPair& size, const QSizeF& screen = {});

/// Grid cell box inside @p gridRect. Empty @p screen uses @p gridRect (same as placeRect).
[[nodiscard]] QRectF cellRect(const PageGrid& grid, const QRectF& gridRect, int row, int col,
                              int rowSpan, int colSpan, const QSizeF& screen = {});
/// Column in x, row in y. {-1,-1} if pos is outside the grid rect.
[[nodiscard]] QPoint cellIndexAt(const PageGrid& grid, const QRectF& gridRect, const QPointF& pos,
                                 const QSizeF& screen = {});

} // namespace PageDimParse
} // namespace gazer
