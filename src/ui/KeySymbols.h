#pragma once

#include <QColor>
#include <QPainter>
#include <QRectF>
#include <QString>
#include <QStringList>

namespace gazer {
namespace KeySymbols {

/// Paint an OptiKey symbol in @p r, filled with @p color.
/// @p name is the OptiKey key with or without an `Icon` suffix (`BackOne` / `BackOneIcon`).
/// Returns false if unknown — callers should fall back to text.
bool paint(QPainter& p, const QString& name, const QRectF& r, const QColor& color);

[[nodiscard]] bool contains(const QString& name);

/// Canonical names without the `Icon` suffix, sorted.
[[nodiscard]] QStringList names();

} // namespace KeySymbols
} // namespace gazer
