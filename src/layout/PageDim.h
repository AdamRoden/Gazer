#pragma once

#include "layout/PageTypes.h"

#include <QRectF>
#include <QString>

namespace gazer {
namespace PageDimParse {

[[nodiscard]] PageDim parse(const QString& token, QString* error = nullptr);
[[nodiscard]] PageDimPair parsePair(const QString& csv, QString* error = nullptr);
[[nodiscard]] QVector<int> parseIntList(const QString& csv, QString* error = nullptr);
[[nodiscard]] PageAnchor parseAnchor(const QString& name, bool* ok = nullptr);
[[nodiscard]] QString anchorName(PageAnchor a);

/// Place a box of `size` at `anchor` on `bounds`, then add `offset`.
[[nodiscard]] QRectF placeRect(const QRectF& bounds, PageAnchor anchor, const PageDimPair& offset,
                               const PageDimPair& size);

} // namespace PageDimParse
} // namespace gazer
