#pragma once

#include <QByteArray>
#include <QColor>
#include <QPainter>
#include <QRectF>
#include <QString>

namespace gazer {
namespace MaterialIcons {

/// Online Material Symbols SVGs, tinted like `KeySymbols`. Unknown names return false.
bool paint(QPainter& p, const QString& name, const QRectF& r, const QColor& color);

[[nodiscard]] bool contains(const QString& name);

/// Keep an SVG for later paints. Invalid documents are ignored.
void store(const QString& name, const QByteArray& svg);

} // namespace MaterialIcons
} // namespace gazer
