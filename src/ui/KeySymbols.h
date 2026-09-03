#pragma once

#include <QColor>
#include <QPainter>
#include <QRectF>
#include <QString>
#include <QStringList>

namespace gazer {
namespace KeySymbols {

/// Paint a cell/zone icon in @p r, tinted with @p color.
/// @p name is a stem from `resources/icons/svg/` (`menu`, `mouseLeftClick`, `Tab`).
/// Matching is case-insensitive; a trailing `Icon` suffix is ignored.
/// Returns false if unknown — callers should fall back to text.
bool paint(QPainter& p, const QString& name, const QRectF& r, const QColor& color);

[[nodiscard]] bool contains(const QString& name);

/// SVG stems as on disk, sorted.
[[nodiscard]] QStringList names();

} // namespace KeySymbols
} // namespace gazer
