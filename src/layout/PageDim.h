#pragma once

#include "layout/PageTypes.h"

#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QStringView>

#include <optional>

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

/// Natural height of a grid whose fixed tracks are not scaled down to the slot.
struct ScrollMetrics {
    int offset = 0;
    int visible = 0;
    int total = 0;
};

/// `offset` is `grid.scrollOffset` clamped to `total - visible`. `visible` is the slot height.
[[nodiscard]] ScrollMetrics scrollMetrics(const PageGrid& grid, const QRectF& slot,
                                          const QSizeF& screen = {});

/// First `scroll="true"` child, the sibling subgrids that hold scroll chrome
/// (`scrollbar`, `scrollUp`, `scrollDown`), and column tracks with those
/// siblings' columns set to 0 when the child fits. nullopt when there is nothing to do.
struct ScrollFit {
    ScrollMetrics metrics;
    QVector<int> chrome;
    std::optional<QVector<PageTrackSize>> columns;
};

[[nodiscard]] std::optional<ScrollFit> scrollFit(const PageGrid& grid, const QRectF& slot,
                                                 const QSizeF& screen = {});

/// Grid cell box inside @p gridRect. Empty @p screen uses @p gridRect (same as placeRect).
[[nodiscard]] QRectF cellRect(const PageGrid& grid, const QRectF& gridRect, int row, int col,
                              int rowSpan, int colSpan, const QSizeF& screen = {});
/// Column in x, row in y. {-1,-1} if pos is outside the grid rect.
[[nodiscard]] QPoint cellIndexAt(const PageGrid& grid, const QRectF& gridRect, const QPointF& pos,
                                 const QSizeF& screen = {});

/// Star tracks above `row` when the grid's outer height is `A_ScreenHeight`.
/// nullopt when `row` is out of range, a track is not a star, or the weights sum to 0.
struct StarBand {
    /// `(A_ScreenHeight-<trim>)*<partsBefore>/<parts>+<pad>`. `trim` is the
    /// margins and gaps. `pad` is the top margin plus the gaps before `row`.
    QString topExpr;
    double partsBefore = 0;
    double parts = 0;
};

[[nodiscard]] std::optional<StarBand> starBandBeforeRow(const PageGrid& grid, int row);

} // namespace PageDimParse
} // namespace gazer
